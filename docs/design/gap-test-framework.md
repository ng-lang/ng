# Testing Framework — Test Discovery & Isolation

> **Status:** Refined with algorithm details. INVEST: 5/5/5/4/4/5 after refinement.

## Test Discovery Algorithm

```cpp
// src/test/TestDiscovery.cpp

struct TestCase {
    std::string name;
    std::string file;
    uint32_t line;
    uint32_t column;
    // The test body is compiled as a function:
    // fun __test_<hash>() { ... body ... }
    std::string functionName;
};

struct TestSuite {
    std::string name;         // From describe()
    std::vector<TestCase> tests;
    std::vector<std::string> beforeHooks;
    std::vector<std::string> afterHooks;
};

class TestDiscoverer {
public:
    // Scan all .ng files in a directory and discover tests
    auto discoverTests(const std::string &directory) -> std::vector<TestSuite>;
    
private:
    // Strategy: parse source, scan for test(...) and describe(...) calls
    // at top level. Extract the string literal argument as the test name.
    auto parseTestDeclarations(const std::string &source) -> std::vector<TestSuite>;
};
```

### Discovery Strategy

**Approach Chosen:** Source-level scanning (not AST walking).

The discoverer scans each `.ng` file for patterns:
```
test "name" {   // Line-start: test "string" {
describe "name" {
```

This is simpler than full parsing and sufficient for test discovery. The test runner then compiles and executes only the discovered test files.

### Test Execution Flow

```
1. ng test [--filter pattern] [files...]
2. Discover tests in project files (or specified files)
3. Filter tests by pattern (if --filter provided)
4. For each test suite:
   a. Compile the test file
   b. For each test in the suite:
      i.   Create a fresh VM (isolated state)
      ii.  Run before hooks
      iii. Run the test function
      iv.  Run after hooks
      v.   Record pass/fail (pass = clean exit, fail = exception/assertion)
5. Report results
6. Exit 0 if all pass, 1 if any fail
```

## Test Isolation Mechanism

```cpp
// Each test runs in its own VM instance:
for (auto &test : tests) {
    VM vm{modulePaths};
    vm.registerNative("expect", expectNative);
    vm.registerNative("expectEq", expectEqNative);
    
    try {
        vm.run(bytecode);      // Compile and run
        vm.call(test.functionName, {});  // Call the test function
        test.status = PASS;
    } catch (const AssertionException &e) {
        test.status = FAIL;
        test.message = e.what();
    } catch (const std::exception &e) {
        test.status = ERROR;
        test.message = e.what();
    }
}
```

**Why per-test VM instances:**
- Complete isolation — no state leakage between tests
- No need for teardown between tests
- GC cycles between tests are naturally separated
- Thread-safe for parallel execution (future)

**Tradeoff:** Slower startup. Mitigated by caching compiled bytecode across tests in the same file.

## Benchmark Statistical Method

```cpp
struct BenchmarkResult {
    std::string name;
    Duration mean;
    Duration median;
    Duration min;
    Duration max;
    Duration stddev;
    uint32_t iterations;
};

class BenchmarkRunner {
public:
    // Runs the benchmark body multiple times and collects statistics
    auto run(const std::string &name, std::function<void()> body) -> BenchmarkResult {
        // Phase 1: Warm-up (3 iterations, discarded)
        for (int i = 0; i < 3; i++) { body(); }
        
        // Phase 2: Measurement (N iterations until stable)
        std::vector<Duration> samples;
        for (int i = 0; i < 100; i++) {
            auto start = high_resolution_clock::now();
            body();
            auto end = high_resolution_clock::now();
            samples.push_back(end - start);
        }
        
        // Phase 3: Statistical analysis
        return analyze(samples);
    }
};
```

### Benchmark Output Format

```
$ ng bench
Benchmarking 3 tests:
  sort_10000          ... 2.34 ms  (±0.12 ms, 100 iterations)
  hashmap_insert      ... 0.89 ms  (±0.05 ms, 100 iterations)
  json_parse_large    ... 15.67 ms (±1.23 ms, 100 iterations)
```

## `std.test` Module Functions

```ng
module std.test exports *;

// Register a test case
export fun test(name: string, body: () -> unit);

// Group tests
export fun describe(name: string, body: () -> unit);

// Assertions
export fun expect(condition: bool);
export fun expectEq<T>(actual: T, expected: T) where T: Eq;
export fun expectNe<T>(actual: T, expected: T) where T: Eq;
export fun expectError(body: () -> unit);
export fun expectApprox(actual: f64, expected: f64, epsilon: f64 = 1e-9);

// Hooks
export fun before(body: () -> unit);
export fun after(body: () -> unit);
```

### Implementation Note: `test` as a Special Form

Since NG doesn't have closures yet (see [Syntax Ergonomics Batch 2](gap-syntax-ergonomics.md)), `test "name" { body }` is implemented as a **special AST form** rather than a function call:

```cpp
// Parser recognizes:
// test "name" { ... }  → TestStatement AST node
// describe "name" { ... } → DescribeStatement AST node

struct TestStatement : Statement {
    ASTRef<StringValue> name;
    ASTRef<Statement> body;
};
```

These nodes are compiled by the ORGASM compiler but not executed during normal runs. They are only executed when run by the test runner (`ng test`).

## Acceptance Criteria

- `ng test` discovers and runs all `test "..." { }` blocks in `tests/*.ng`
- A passing test exits with code 0
- A failing test exits with code 1 and prints the failure location
- `expectEq(2+2, 4)` passes; `expectEq(2+2, 5)` fails with "expected 4, got 5"
- Tests in the same file are isolated (one test cannot affect another)
- `before`/`after` hooks run for each test
- `bench` produces timing output with mean, min, max
- Tests can be filtered by name pattern

## Effort Estimate

| Component | Effort |
|---|---|
| `test`/`describe` AST nodes + parser | 1 week |
| Test discovery (source scanning) | 0.5 week |
| Test runner (VM per test) | 1 week |
| Assertion functions (C++ native + NG) | 1 week |
| `before`/`after` hooks | 0.5 week |
| Benchmark runner | 1 week |
| CLI integration (`ng test`, `ng bench`) | 0.5 week |
| Tests for the test framework | 1 week |
| **Total** | **6.5 weeks** |