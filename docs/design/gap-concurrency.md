# Concurrency: Spawn/Wait Model (MVP)

> **Status:** Feasibility review found that full async/await with state machine desugaring requires fundamental VM rework (suspension points, compiler-generated types). Replaced with a simpler spawn/wait model that uses C++ thread pools and does not require VM suspension.

## Order

Recommended implementation order: **8** (after error handling, stdlib, and tooling).

## Goal

Introduce a minimal **spawn/wait** concurrency model that runs functions on a thread pool, enabling parallel CPU-bound computation without VM suspension changes.

## Motivation

While the current architecture cannot support async/await, many use cases only need parallel execution:

```ng
fun compute() -> i32 {
    val task1 = spawn heavyComputation(data1);
    val task2 = spawn heavyComputation(data2);
    return await(task1) + await(task2);  // Run in parallel
}
```

## Proposed Design

### 1. New Types

```ng
type Task<T> = native opaque;  // A handle to a running computation
```

### 2. New Keywords

Add to lexer: `KEYWORD_SPAWN`, `KEYWORD_AWAIT`.

### 3. AST Changes

```cpp
struct SpawnExpression : Expression {
    ASTRef<Expression> expr;  // Function call to spawn
};

struct AwaitExpression : Expression {
    ASTRef<Expression> expr;  // Task<T> to wait for
};
```

### 4. Syntax

```ng
// Spawn a function call on the thread pool:
val task = spawn compute(data);

// Wait for the result (blocks current thread):
val result = await(task);

// Type:
// spawn compute(data) : Task<T>   (where compute returns T)
// await(task)         : T
```

### 5. Thread Pool (C++)

A new component `src/runtime/thread_pool.cpp`:

```cpp
class ThreadPool {
    std::vector<std::thread> workers;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::queue<std::packaged_task<void()>> tasks;
    bool stop = false;

public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template<typename F>
    auto enqueue(F&& f) -> std::future<decltype(f())> {
        auto task = std::make_shared<std::packaged_task<decltype(f())()>>(std::forward<F>(f));
        auto result = task->get_future();
        {
            std::lock_guard lock(queueMutex);
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return result;
    }
};
```

### 6. ORGASM VM Changes

**New opcodes:**

```
Opcode::SPAWN: {
    // Pop function name + args, enqueue on thread pool
    auto funcName = read_string();
    auto args = pop_args(funcName);
    auto future = threadPool.enqueue([this, funcName, args]() {
        return execute_function(funcName, args);
    });
    auto task = makeTaskHandle(std::move(future));
    stack.push(task);
}

Opcode::AWAIT: {
    auto task = stack.pop();
    auto result = task.wait();  // Block current thread until complete
    stack.push(result);
}
```

### 7. Task Isolation

Each spawn creates a **new VM scope** with its own locals, stack, and execution context. This avoids shared-state concurrency issues (GC safety, global mutation). Tasks communicate only through their return values.

```cpp
// Each task runs in its own execution context:
auto execute_function(const Str &name, const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
    VM isolatedVm(modulePaths);
    isolatedVm.registerNatives(native_functions);
    return isolatedVm.callFunction(name, args);
}
```

### 8. Type Checker Changes

```cpp
void visit(SpawnExpression *node) {
    node->expr->accept(this);
    // expr must be a function call
    // return type becomes Task<return_type>
    latestType = makecheck<TaskType>(latestType);
}

void visit(AwaitExpression *node) {
    node->expr->accept(this);
    // expr must be Task<T>
    // result type is T
    auto taskType = std::dynamic_pointer_cast<TaskType>(latestType);
    if (!taskType) throw error;
    latestType = taskType->innerType;
}
```

## Scope

**In scope:**
- `spawn` keyword + expression
- `await` keyword + expression  
- `Task<T>` type (opaque native handle)
- C++ thread pool (1-2 week effort)
- GC-safe task isolation (separate VM instances)

**Explicitly out of scope (deferred to post-MVP):**
- `async fun` / `await` syntactic sugar (state machine desugaring)
- Single-threaded cooperative multitasking
- Channels / message passing
- `Send`/`Sync` checking
- Async I/O (readFile remains blocking)
- Closure capture across `spawn` boundaries

## Acceptance Criteria

- `spawn heavyComputation(data)` runs the function on another thread
- `await(task)` returns the correct result
- Two spawned functions run in parallel (total time < sum of individual times)
- A spawned function can call other functions normally
- Spawned functions have isolated state (no shared mutable globals)
- All existing tests pass (no regressions)

## Effort Estimate

| Component | Effort |
|---|---|
| Thread pool implementation | 1 week |
| AST nodes + parser | 1 day |
| Type checker (Task<T>, spawn, await) | 2 days |
| Compiler (emit SPAWN/AWAIT opcodes) | 2 days |
| VM (SPAWN/AWAIT handlers) | 3 days |
| Tests | 2 days |
| **Total** | **~2.5 weeks** |

## Dependencies

- C++ standard library (thread, future, mutex) — no external dependencies
- [Error Handling](gap-error-handling.md) for `Result<T,E>` in spawned functions
- No changes to existing VM execution model

## Why Not Async/Await?

Full async/await with state machine desugaring was the original design but was rejected after feasibility review:

| Issue | Impact |
|---|---|
| VM has no suspension points | Requires refactoring `execute_slots()` to support suspend/resume — a 3-4 week effort with high risk of regressions |
| Compiler cannot generate new types | State machine desugaring requires the compiler to create new type definitions, methods, and trait implementations at compile time — currently unsupported |
| GC is not concurrent-capable | Adding GC thread safety for cooperative multitasking is complex |
| YIELD opcode requires frame stack refactoring | The `call_stack` would need to be preserved across yields, requiring heap-allocated frames |

The spawn/wait model avoids all these issues by:
- Running each task in a separate VM instance (no shared state)
- Using C++ threads/processes for parallelism (not cooperative multitasking)
- Blocking on `await` (not suspending)
- Avoiding code generation (Tasks are runtime handles, not compiler-constructed types)