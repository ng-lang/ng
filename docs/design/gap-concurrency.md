# Concurrency Model: Async/Await And Lightweight Tasks

## Order

Recommended implementation order: **6**.

## Goal

Add a concurrency model to NG, enabling asynchronous I/O and parallel computation without the complexity of manual thread management.

## Motivation

NG currently has **no concurrency support whatsoever**. The language cannot:
- Perform non-blocking I/O
- Spawn parallel tasks
- Coordinate concurrent operations
- Write a TCP server that handles multiple connections

In 2026, a language without concurrency cannot be considered general-purpose.

## Proposed Design

### Option A: Async/Await (Recommended)

A Rust/JavaScript-style async model built on top of lightweight tasks:

```ng
async fun fetchUrl(url: string) -> Result<string, IOError> {
    val conn = await TcpStream::connect(url)?;
    val response = await conn.readAll()?;
    return Ok(response);
}

fun main() {
    val task = spawn fetchUrl("https://example.com");
    // ... do other work ...
    val result = await task;
}
```

Key primitives:
- `async fun` — declares an async function returning a `Future<T>`
- `await expr` — suspends the current task until the future completes
- `spawn expr` — creates a new lightweight task
- `Future<T>` — a first-class type representing an asynchronous computation

### Option B: Goroutine-Style (Alternative)

A Go-inspired model with channels:

```ng
fun worker(id: i32, jobs: Chan<i32>, results: Chan<i32>) {
    loop {
        val job = <-jobs;          // receive from channel
        if (job == -1) { return; }
        results <- job * 2;        // send to channel
    }
}

fun main() {
    val jobs = Chan<i32>(100);
    val results = Chan<i32>(100);

    // Spawn 10 workers
    loop i = 0 { if (i < 10) {
        go worker(i, jobs, results);
        next i + 1;
    }}

    // Send jobs
    loop i = 0 { if (i < 100) {
        jobs <- i;
        next i + 1;
    }}
}
```

### Recommended: Start with Async/Await

Async/await composes better with NG's existing type system and is more familiar to most developers.

### Execution Model

- **Lightweight tasks** (not OS threads): multiplexed onto a thread pool
- **Single-threaded by default**: tasks run on one thread unless explicitly parallelized
- **Cooperative scheduling**: tasks yield at `await` points
- **Work-stealing thread pool**: for CPU-bound parallel tasks via `spawn`

### `Send` and `Sync` Traits

Reuse the existing `auto trait Send` declaration to enforce thread-safety:

```ng
auto trait Send;   // Types that can be sent across threads
auto trait Sync;   // Types that can be shared across threads

// Auto-implemented for primitive types and composed types
// Explicitly opt out for types with interior mutability or non-thread-safe native handles
```

## Dependencies

- Requires [Error Handling](gap-error-handling.md) for `Result<T, E>` (used for I/O fallibility).
- Requires the async runtime (a simple event loop + thread pool).
- The `Future` type is a tagged union or trait object — both already exist.
- Unblocks: networking libraries, concurrent data structures, parallel algorithms.

## Scope

**In scope:**
- `async`/`await` syntax
- `spawn` keyword
- `Future<T>` type definition
- Lightweight task scheduler (work-stealing thread pool)
- `Send`/`Sync` auto trait checking
- `Chan<T>` channel type for task communication (if Option B)
- Basic async I/O support in stdlib

**Out of scope:**
- `select` statement (multi-channel wait) — future enhancement
- `async` closures (lambdas that return futures) — future enhancement
- Cancellation and timeouts — future enhancement
- Distributed computing — far future
- Structured concurrency — future enhancement

## Acceptance Criteria

- `async fun` compiles and returns a `Future<T>`
- `await` suspends the current task and resumes when the future completes
- `spawn` creates a concurrently executing task
- Two async tasks can run concurrently and exchange data through channels
- `Send` checking prevents sending non-thread-safe values across tasks
- The async runtime has configurable thread pool size
- All existing tests pass (non-async code is unaffected)
- An async TCP echo server example exists

## Potential Challenges

- The VM is currently single-threaded — adding concurrency requires significant VM rework.
- ORGASM VM state must be either thread-local or carefully synchronized.
- The existing STUPID interpreter is even less suited for concurrency.
- `Send`/`Sync` auto trait checking requires the type checker to analyze field-level thread-safety.
- GC must become thread-safe (stop-the-world or concurrent GC).
- Native function calls from multiple threads must be reentrant-safe.