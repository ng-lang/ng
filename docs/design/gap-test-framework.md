# Testing Framework and Benchmarking

## Order

Recommended implementation order: **14**.

## Goal

Provide a built-in testing framework for NG, enabling unit tests, integration tests, benchmarks, and property-based testing without external tools.

## Motivation

NG currently has **no testing support in the language itself**. The project's own tests are written in C++ using Catch2. There is no:
- `test` keyword or attribute
- Assertion library beyond `assert()` (which aborts)
- Test runner
- Benchmarking tools
- Property-based testing

## Proposed Design

### 1. Unit Testing

```ng
// test_math.ng
import std.test;

test "addition works" {
    expectEq(add(2, 3), 5);
    expect(add(0, 0) == 0);
}

test "subtraction is not commutative" {
    expect(add(3, 1) != add(1, 3));  // Wait, it is commutative — test catches the bug!
}
```

### 2. Test Runner

```bash
ng test                          # Run all tests in the project
ng test test_math.ng             # Run tests in a specific file
ng test --filter "addition"      # Run tests matching filter
ng test --list                   # List all discovered tests
ng test --verbose                # Show verbose output
```

### 3. Test Expectations

```ng
import std.test;

expect(condition);               // Assert condition is true
expectEq(a, b);                  // Assert a == b
expectNe(a, b);                  // Assert a != b
expectError(expr);               // Assert expression throws/runtime errors
expectApprox(a, b, epsilon);     // Assert a ≈ b within epsilon (for floats)
expectType<T>(value);            // Assert value has type T
```

### 4. Test Organization

```ng
// Grouping
describe "Math operations" {
    test "addition" { ... }
    test "subtraction" { ... }
}

// Setup and teardown
describe "Database" {
    before {
        db = connect();
    }
    
    after {
        db.close();
    }
    
    test "query returns results" {
        expect(db.query("SELECT 1").len() > 0);
    }
}
```

### 5. Integration Testing

```ng
// test_integration.ng
import std.test;

// Test that a module compiles and runs correctly
test_integration "stdlib imports work" {
    val result = runNgi("import std.io; print(readFile('test.txt'));");
    expect(result == "file contents");
}
```

### 6. Benchmarking

```ng
import std.test;

bench "sort 10,000 integers" {
    val data = generateRandomArray(10000);
    
    // Time this block
    measure {
        sort(data);
    }
}

// Compare implementations
bench_comparison "sorting algorithms" {
    val data = generateRandomArray(10000);
    
    group "quicksort" {
        measure { quicksort(data.clone()); }
    }
    
    group "mergesort" {
        measure { mergesort(data.clone()); }
    }
}
```

### 7. Property-Based Testing

```ng
import std.test;

// Verify that sorting is idempotent
property "sort is idempotent" {
    forAll (list: [i32]) {
        val sorted = sort(list);
        expectEq(sort(sorted), sorted);  // sorted twice = sorted once
    }
}

// With custom generators
property "addition commutes" {
    forAll (a: i32, b: i32) {
        expectEq(a + b, b + a);
    }
}
```

### 8. Test Output Format

```
$ ng test
running 12 tests
  ✓ addition works (2ms)
  ✓ subtraction works (1ms)
  ✓ multiplication works (1ms)
  ✗ division by zero panics (0ms)
    expected: panic but got: Ok
  ...

test result: FAILED. 11 passed, 1 failed, 0 skipped
```

JUnit XML output for CI integration:

```bash
ng test --junit-xml results.xml
```

### The `std.test` Module

```ng
module std.test exports *;

// Core test primitives
fun describe(name: string, body: () -> unit) -> unit;
fun test(name: string, body: () -> unit) -> unit;
fun bench(name: string, body: () -> unit) -> unit;
fun property(name: string, body: () -> unit) -> unit;

// Assertions
fun expect(condition: bool) -> unit;
fun expectEq<T>(actual: T, expected: T) -> unit;
fun expectNe<T>(actual: T, expected: T) -> unit;
fun expectError(body: () -> unit) -> unit;
fun expectApprox(actual: f64, expected: f64, epsilon: f64) -> unit;

// Setup/teardown
fun before(body: () -> unit) -> unit;
fun after(body: () -> unit) -> unit;

// Benchmarking
fun measure(body: () -> unit) -> Duration;

// Property testing
fun forAll<T>(generator: () -> T, property: (T) -> bool) -> unit;
```

## Dependencies

- Requires `describe`/`test`/`bench` as built-in syntax or library.
- Property testing requires random number generation in stdlib.
- Benchmark measurement requires high-resolution timers (already in `std.time`).
- Unblocks: test-driven development, CI integration, regression prevention.

## Scope

**In scope:**
- `std.test` module with core assertion functions
- `ng test` command (discover and run tests)
- `describe`/`test` organization
- `before`/`after` hooks
- JUnit XML output
- Benchmarking with `measure` block
- Property-based testing with `forAll`

**Out of scope:**
- Code coverage instrumentation (requires runtime tracking — future)
- Mutation testing (far future)
- Fuzz testing (requires harness generation — future)
- Snapshot testing (future)

## Acceptance Criteria

- `ng test` discovers all `test "..." { ... }` blocks in a project
- A passing test suite produces exit code 0
- A failing test produces exit code 1 with the failure message
- `expect` correctly reports assertion failures with source location
- `bench` produces timing output in milliseconds
- `property` finds counterexamples for false properties
- Tests can be filtered by name
- The test framework works in both STUPID and ORGASM modes

## Potential Challenges

- Test discovery requires parsing source files without executing them — the NG parser already supports this.
- `describe`/`test` blocks are currently not part of the language — they could be added as functions taking closure arguments, or as special AST nodes.
- `expect` must capture source location for useful error messages.
- Property-based testing requires shrinking (finding minimal counterexamples) — a non-trivial algorithm.
- Benchmarks must account for VM warm-up and JIT compilation (if JIT is added).