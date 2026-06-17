# Concurrency Model — Phase 1: Single-Threaded Async/Await

> **Status:** Restructured from original monolithic proposal. Split into 4 sequential phases.
> Phase 1 scope only: single-threaded async/await without Send/Sync checking.

## Order

Recommended implementation order: **8** (after error handling, stdlib, and tooling are stable).

## Goal (Phase 1)

Introduce a minimal async/await model that enables non-blocking I/O on a **single thread**, without thread pools, Send/Sync checking, or channels.

## Motivation

Phase 1 targets the 80% use case: **I/O-bound concurrency** where a single thread interleaves multiple tasks. This covers:
- Reading multiple files concurrently
- Handling multiple network connections
- UI event loops (beyond ImGui)

Full multi-threaded concurrency (Phase 2-4) can be added later without changing the async/await syntax.

## Proposed Design

### 1. New Types

```ng
// A Future represents a value that will be available later.
// It's a state machine that can be polled.
type Future<T> = Pending | Ready(value: T);
```

- `Future<T>` is a tagged union (reuses existing infrastructure)
- `Pending` — the computation has not completed yet
- `Ready(value)` — the value is available
- Futures are **lazy**: they don't start executing until polled

### 2. New Keywords

Add to lexer: `KEYWORD_ASYNC`, `KEYWORD_AWAIT`, `KEYWORD_YIELD`.

**Note:** An AST node `YIELD_STATEMENT = 0x504` already exists at line 90 of `include/ast.hpp` but has no parser support. The concurrency proposal adds `KEYWORD_YIELD` and implements the existing node rather than creating new infrastructure.

### 3. AST Changes

```cpp
// include/ast.hpp

struct AsyncFunctionDef : FunctionDef {
    // Same as FunctionDef but marked as async
    // The return type is automatically wrapped in Future<T>
    bool isAsync = true;
};

struct AwaitExpression : Expression {
    ASTRef<Expression> expr;  // The Future<T> to await

    explicit AwaitExpression(SourcePosition pos, ASTRef<Expression> expr)
        : Expression(ASTNodeType::AWAIT, std::move(pos)), expr(std::move(expr)) {}
};
```

### 4. Syntax

```ng
// Declare an async function
async fun fetchUrl(url: string) -> string {
    // ... implementation ...
}

// Await a future
async fun process() -> i32 {
    val result = await fetchUrl("https://example.com");
    return len(result);
}
```

### 5. Desugaring: State Machine Transformation

An `async fun` is transformed into a state machine at compile time:

```ng
// Source:
async fun process() -> i32 {
    val a = await fetchUrl("url1");
    val b = await fetchUrl("url2");
    return a + b;
}

// Desugared to (conceptual):
fun process() -> Future<i32> {
    val state = new ProcessState {
        __state: 0,
        __a: unit,
        __b: unit,
    };
    return state;
}

// State machine (generated):
impl Future<i32> for ProcessState {
    fun poll(self: ref<Self>) -> Future<i32> {
        switch (self.__state) {
            case 0 {
                val fut1 = fetchUrl("url1");
                switch (fut1) {
                    case Pending { return Pending; }  // yield
                    case Ready(v) {
                        self.__a = v;
                        self.__state = 1;
                        return self.poll();  // tail-call next state
                    }
                }
            }
            case 1 {
                val fut2 = fetchUrl("url2");
                switch (fut2) {
                    case Pending { return Pending; }
                    case Ready(v) {
                        self.__b = v;
                        self.__state = 2;
                        return self.poll();
                    }
                }
            }
            case 2 {
                return Ready(self.__a + self.__b);
            }
        }
    }
}
```

### 6. Type Checker Changes

- `async fun` return type is automatically wrapped: `T` → `Future<T>`
- `await expr` requires `expr: Future<T>`, produces `T`
- `await` can only appear inside `async fun` bodies
- `await` cannot appear inside `const if` or const functions

### 7. ORGASM VM Changes

**New opcodes:**
```
CREATE_FRAME       // Create a resumable frame
YIELD              // Suspend current task (return Pending)
TASK_RESUME        // Resume a suspended task
```

The VM gets a **task queue**:

```cpp
class TaskQueue {
    Vec<ResumableFrame> tasks;
    
    void spawn(ResumableFrame frame);
    void runAll();  // Run until all tasks complete
};
```

The execution model:
```
vm.run(entry) → enters main task
  main task calls async fun → CREATE_FRAME
  main task calls await → polls future
    if Pending → YIELD, push to task queue, switch to next task
    if Ready → continue
  YIELD → switch to next task in queue
```

### 8. Immutable Globals Restriction

In Phase 1, `async fun` bodies cannot access mutable globals. This is a compile-time restriction to prevent data races on single thread (enforced by the type checker). This restriction is lifted in Phase 2 when the runtime supports detection.

## Dependencies

- Requires [Error Handling](gap-error-handling.md) for `Result<T, E>` used in fallible async operations.
- ORGASM VM must support frame suspension and task queue.
- No dependency on threading libraries (Phase 1 is single-threaded).

## GC Thread-Safety — Impact Analysis

The existing GC (`src/runtime/managed_heap.cpp`) is **single-threaded**:
- No mutexes protecting heap structures
- No atomic operations for reference counts
- No concurrent marking algorithm
- Finalizers run inline during collection

**Impact on Concurrency Phases:**

| Phase | GC Requirement | Effort |
|---|---|---|
| Phase 1 (single-thread) | **No changes needed** — single-threaded async is safe | 0 |
| Phase 2a (multi-thread, stop-the-world) | Add global GC mutex. All allocation/marking/sweeping synchronized. Pause all threads during collection. | 2 weeks |
| Phase 2b (concurrent marking, optional) | Tri-color marking with write barrier. Thread-local allocation buffers. | 2-3 months |
| Phase 3 (Send/Sync) | No GC changes (only type-checker changes) | 0 |

**Recommendation:** Phase 2a is sufficient for MVP multi-threaded execution. Phase 2b should only be attempted if GC pause times become a measured bottleneck.

#### Phase 2a GC Changes

```cpp
// src/runtime/managed_heap.cpp

class ManagedHeap {
    std::mutex heapMutex;          // NEW: protects all heap access

    void collectGarbage() {
        std::lock_guard lock(heapMutex);  // NEW: exclusive access during GC
        mark();
        sweep();
    }

    StorageCell* allocate(size_t size) {
        std::lock_guard lock(heapMutex);  // NEW: synchronized allocation
        // ... existing logic ...
    }
};
```

## Scope

**Phase 1 in scope:**
- `async fun` syntax
- `await` expression
- `Future<T>` tagged union
- State machine desugaring at compile time
- Single-threaded task queue in ORGASM VM
- Compile-time restrictions (no mutable globals in async, no await in const)
- Async `readFile` example

**Out of scope (Phase 1):**
- Multi-threaded executor / thread pool
- `spawn` keyword
- `Send`/`Sync` checking
- Channels / message passing
- Async I/O in stdlib beyond basic file operations
- STUPID interpreter support (Phase 1 is ORGASM-only)

## Phase 1 Acceptance Criteria

- `async fun` compiles and returns `Future<T>`
- `await` correctly suspends and resumes the state machine
- Two async tasks can interleave on a single thread (cooperative multitasking)
- A task awaiting a `Pending` future does not block other tasks
- `await` outside `async fun` is a compile error
- Async functions with no `await` calls compile and return `Ready(value)` immediately
- Deeply nested async calls (3+ levels) work correctly
- All existing tests pass

## Phase 1 Effort Estimate

| Component | Effort |
|---|---|
| New token + keyword changes | 0.5 day |
| AST nodes (+ parser) | 1 day |
| State machine desugaring (compiler) | 5 days |
| `Future<T>` type checking | 2 days |
| VM task queue | 3 days |
| VM YIELD/RESUME opcodes | 2 days |
| VM CREATE_FRAME opcode | 1 day |
| Tests | 3 days |
| **Total** | **~3 weeks** |

## Future Phases

### Phase 2: Multi-Threaded Executor + `spawn` (Q2 2027)

- Thread pool with work-stealing
- `spawn asyncFun()` → runs on thread pool
- `spawn` returns `Future<T>` just like `async fun` calls
- Mutex/atomic support for shared state
- GC becomes thread-safe (stop-the-world)

### Phase 3: `Send`/`Sync` Checking (Q3 2027)

- Reuse existing `auto trait Send` declaration
- Type checker enforces that values sent across threads are `Send`
- Type checker enforces that shared borrows across threads are `Sync`
- Auto-implemented for primitive types; opt-out for non-thread-safe types

### Phase 4: Channels / Message Passing (Q4 2027)

- `Chan<T>` type with blocking send/receive
- `select` over multiple channels
- Work: `spawn processor(input, output)` pattern