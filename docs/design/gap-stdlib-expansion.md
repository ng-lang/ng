# Standard Library Expansion

> **Status:** Refined with per-module API signatures. INVEST: 5/5/5/5/4/5 after refinement.

## Order

Recommended implementation order: **2** (must follow error handling for Result-based APIs).

## Goal

Expand NG's standard library from ~450 lines of NG to a production-ready set covering collections, JSON, time, math, and testing.

## Modules (Listed by Implementation Priority)

### Module A: `std.collections` — HashMap & HashSet (2 weeks)

Provides hash-based data structures. Requires a `Hash` trait.

#### `Hash` Trait (Prelude Addition)

```ng
// In prelude:
export trait Hash {
    fun hash(self: ref<Self>) -> u64;
}

// Auto-implement for primitives:
impl Hash for i32 { ... }
impl Hash for i64 { ... }
impl Hash for string { ... }
impl Hash for bool { ... }
impl Hash for u32 { ... }
impl Hash for u64 { ... }
impl Hash for f32 { ... }
impl Hash for f64 { ... }
```

#### HashMap

```ng
module std.collections exports *;

export type HashMap<K: Hash, V> = native;  // Opaque native type

export fun newHashMap<K: Hash, V>() -> HashMap<K, V> = native;
export fun insert<K: Hash, V>(map: ref<HashMap<K, V>>, key: K, value: V) -> Option<V> = native;
export fun get<K: Hash, V>(map: ref<HashMap<K, V>>, key: K) -> Option<V> = native;
export fun remove<K: Hash, V>(map: ref<HashMap<K, V>>, key: K) -> Option<V> = native;
export fun contains<K: Hash, V>(map: ref<HashMap<K, V>>, key: K) -> bool = native;
export fun len<K, V>(map: ref<HashMap<K, V>>) -> u32 = native;
export fun keys<K, V>(map: ref<HashMap<K, V>>) -> vector<K> = native;
export fun values<K, V>(map: ref<HashMap<K, V>>) -> vector<V> = native;
export fun clear<K, V>(map: ref<HashMap<K, V>>) -> unit = native;
```

#### HashSet

```ng
export type HashSet<T: Hash> = native;

export fun newHashSet<T: Hash>() -> HashSet<T> = native;
export fun insert<T: Hash>(set: ref<HashSet<T>>, value: T) -> bool = native;
export fun contains<T: Hash>(set: ref<HashSet<T>>, value: T) -> bool = native;
export fun remove<T: Hash>(set: ref<HashSet<T>>, value: T) -> bool = native;
export fun len<T: Hash>(set: ref<HashSet<T>>) -> u32 = native;
export fun toVector<T: Hash>(set: ref<HashSet<T>>) -> vector<T> = native;
```

#### C++ Backing Implementation

Use existing vendored dependencies or standard C++:
- `HashMap<K,V>` backs to `std::unordered_map` (or `ska::flat_hash_map` if vendored)
- `HashSet<T>` backs to `std::unordered_set`
- The `Hash` trait calls a C++ method on the NG value to produce `size_t`

---

### Module B: `std.json` — JSON Parser (2-3 weeks)

A DOM-style JSON parser that produces a traversable tree.

#### Types

```ng
module std.json exports *;

export type JsonValue = JsonNull | JsonBool(value: bool) | JsonInt(value: i64)
                      | JsonFloat(value: f64) | JsonString(value: string)
                      | JsonArray(items: vector<JsonValue>)
                      | JsonObject(fields: HashMap<string, JsonValue>);

export type JsonError {
    message: string;
    position: u32;
};

export type IOError = std.io.IOError;

export type JsonParseResult = Result<JsonValue, JsonError>;
```

#### Functions

```ng
// Parse string to JSON
export fun parse(text: string) -> JsonParseResult = native;

// Read and parse file
export fun parseFile(path: string) -> Result<JsonValue, JsonError | IOError> = native;

// Serialize JSON back to string
export fun stringify(value: JsonValue) -> string = native;

// Pretty-print with indentation
export fun stringifyPretty(value: JsonValue, indent: u32) -> string = native;

// Query helpers
export fun getField(obj: JsonValue, key: string) -> Option<JsonValue>;
export fun getIndex(arr: JsonValue, index: u32) -> Option<JsonValue>;

// Type checks (convenience)
export fun isNull(value: JsonValue) -> bool;
export fun asString(value: JsonValue) -> Option<string>;
export fun asInt(value: JsonValue) -> Option<i64>;
export fun asFloat(value: JsonValue) -> Option<f64>;
export fun asBool(value: JsonValue) -> Option<bool>;
export fun asArray(value: JsonValue) -> Option<vector<JsonValue>>;
export fun asObject(value: JsonValue) -> Option<HashMap<string, JsonValue>>;
```

#### C++ Backing

Use `simdjson` (vendored or CMake FetchContent) for high-performance JSON parsing. The `JsonValue` tagged union maps to a runtime discriminated value.

---

### Module C: `std.time` — DateTime & Duration (2 weeks)

UTC-only initially. No timezone support in MVP.

```ng
module std.time exports *;

export type Instant = i64;        // Nanoseconds since epoch
export type Duration = i64;       // Nanoseconds

// Creating values
export fun now() -> Instant = native;
export fun durationFromNanos(ns: i64) -> Duration;
export fun durationFromMicros(us: i64) -> Duration;
export fun durationFromMillis(ms: i64) -> Duration;
export fun durationFromSeconds(s: i64) -> Duration;

// Duration arithmetic
export fun durationNanos(d: Duration) -> i64;
export fun durationMicros(d: Duration) -> i64;
export fun durationMillis(d: Duration) -> i64;
export fun durationSeconds(d: Duration) -> i64;

// Instant arithmetic
export fun addDuration(instant: Instant, d: Duration) -> Instant;
export fun subDuration(instant: Instant, d: Duration) -> Instant;
export fun diff(a: Instant, b: Instant) -> Duration;

// Comparison
export fun compare(a: Instant, b: Instant) -> i32;  // -1, 0, 1

// Human-readable formatting
export fun instantToRFC3339(instant: Instant) -> string = native;

// Sleep (blocking)
export fun sleep(d: Duration) -> unit = native;
```

---

### Module D: `std.test` — Testing Framework (2 weeks)

*Note: Depends on `test` and `describe` being implemented as library functions taking closure arguments, OR as special AST nodes (see [Syntax Ergonomics Phase 2](gap-syntax-ergonomics.md)).*

```ng
module std.test exports *;

// --- Test Registration ---
// These are implemented as macro-like functions or special forms:
export fun test(name: string, body: () -> unit) -> unit;
export fun describe(name: string, body: () -> unit) -> unit;

// --- Assertions ---
export fun expect(condition: bool) -> unit;
export fun expectEq<T>(actual: T, expected: T) -> unit where T: Eq;
export fun expectNe<T>(actual: T, expected: T) -> unit where T: Eq;
export fun expectApprox(actual: f64, expected: f64, epsilon: f64 = 1e-9) -> unit;
export fun expectError(body: () -> unit) -> unit;
export fun expectType<T>(value: unknown) -> unit;

// --- Setup/Teardown ---
export fun before(body: () -> unit) -> unit;
export fun after(body: () -> unit) -> unit;
export fun beforeEach(body: () -> unit) -> unit;
export fun afterEach(body: () -> unit) -> unit;

// --- Test Runner API ---
export fun runTests() -> TestResults;

export type TestResults {
    total: u32;
    passed: u32;
    failed: u32;
    duration: Duration;
};

// --- Benchmark ---
export fun bench(name: string, body: () -> unit) -> unit;
export fun measure(body: () -> unit) -> Duration;
```

**Test discovery algorithm:**
1. The test runner parses all `.ng` files in `tests/`
2. Scans the top-level for `test(...)` and `describe(...)` calls
3. Executes matching tests in sequence
4. Each test runs in a fresh scope to prevent state leakage
5. A single `assert(false)` or uncaught error marks the test as failed

**Test isolation:**
- Each `test` block runs in its own VM frame
- Globals modified by one test are visible to subsequent tests (opt-in via `--no-isolate`)
- `before`/`after` hooks run in the same scope as their containing `describe`

---

### Module E: `std.math` — Math Functions (1 week)

```ng
module std.math exports *;

// Constants
export val PI: f64 = 3.14159265358979323846;
export val E: f64 = 2.71828182845904523536;

// Basic
export fun abs<T: i32 | i64 | f32 | f64>(x: T) -> T = native;
export fun min<T>(a: T, b: T) -> T = native;
export fun max<T>(a: T, b: T) -> T = native;
export fun clamp<T>(value: T, minVal: T, maxVal: T) -> T;
export fun lerp<T: f32 | f64>(a: T, b: T, t: T) -> T;
export fun sign<T: i32 | i64 | f32 | f64>(x: T) -> i32 = native;

// Power / Root
export fun sqrt(x: f64) -> f64 = native;
export fun cbrt(x: f64) -> f64 = native;
export fun pow(base: f64, exp: f64) -> f64 = native;
export fun exp(x: f64) -> f64 = native;
export fun ln(x: f64) -> f64 = native;
export fun log2(x: f64) -> f64 = native;
export fun log10(x: f64) -> f64 = native;
export fun hypot(a: f64, b: f64) -> f64 = native;

// Trigonometry
export fun sin(x: f64) -> f64 = native;
export fun cos(x: f64) -> f64 = native;
export fun tan(x: f64) -> f64 = native;
export fun asin(x: f64) -> f64 = native;
export fun acos(x: f64) -> f64 = native;
export fun atan(x: f64) -> f64 = native;
export fun atan2(y: f64, x: f64) -> f64 = native;

// Rounding
export fun floor(x: f64) -> f64 = native;
export fun ceil(x: f64) -> f64 = native;
export fun round(x: f64) -> f64 = native;
export fun trunc(x: f64) -> f64 = native;
```

---

### Module F: `std.regex` — Regular Expressions (2 weeks, deferred)

```ng
module std.regex exports *;

export type Regex = native;
export type RegexError { message: string; }
export type RegexMatch {
    start: u32;
    end: u32;
    groups: vector<(start: u32, end: u32)>;
};

export fun compile(pattern: string) -> Result<Regex, RegexError> = native;
export fun isMatch(regex: ref<Regex>, text: string) -> bool = native;
export fun find(regex: ref<Regex>, text: string) -> Option<RegexMatch> = native;
export fun findAll(regex: ref<Regex>, text: string) -> vector<RegexMatch> = native;
export fun replace(regex: ref<Regex>, text: string, replacement: string) -> string = native;
export fun split(regex: ref<Regex>, text: string) -> vector<string> = native;
```

Backed by `pcre2` or `std::regex` (pcre2 recommended for performance and Unicode support).

---

### Migration: Replacing Existing `std.array`

Current `lib/std/array.ng` (3 lines) should be expanded:

```ng
module std.array exports *;

// Existing
fun reverse<T>(xs: vector<T>) -> vector<T> = native;

// New additions
fun sort<T>(xs: vector<T>) -> vector<T> = native;
fun isEmpty<T>(xs: vector<T>) -> bool;
fun contains<T>(xs: vector<T>, value: T) -> bool where T: Eq;
fun first<T>(xs: vector<T>) -> Option<T>;
fun last<T>(xs: vector<T>) -> Option<T>;
fun slice<T>(xs: vector<T>, start: i32, end: i32) -> vector<T>;
fun concat<T>(a: vector<T>, b: vector<T>) -> vector<T>;
```

---

## Implementation Order

| Order | Module | Effort | Dependencies |
|---|---|---|---|
| 1 | `Hash` trait + `std.collections` (HashMap only) | 2 weeks | Prelude modifications |
| 2 | `std.math` | 1 week | None |
| 3 | `std.json` (MVP: parse + stringify) | 2 weeks | `std.collections` (HashMap) |
| 4 | `std.time` (UTC only) | 2 weeks | None |
| 5 | `std.test` (MVP: expect + test runner) | 2 weeks | Error handling |
| 6 | `std.array` expansion | 0.5 week | None |
| 7 | `std.regex` | 2 weeks | None |

**Total Phase 1 effort:** ~12 weeks (can be parallelized across 2-3 developers).

---

## Acceptance Criteria (Per Module)

### std.collections
- `HashMap` with 10,000 entries: insert, get, remove all complete in < 50ms
- `HashSet` deduplicates correctly
- `Hash` trait is auto-implemented for all primitive types
- Memory: 10,000 entries use < 2MB

### std.json
- `parse('{"a":1, "b":[2,3]}')` produces correct tree
- Nested objects 100 levels deep parse correctly
- Malformed JSON returns `Err` with position
- `stringify` round-trips: `stringify(parse(x)) == x`

### std.time
- `now()` returns a progressively increasing value
- `durationFromSeconds(5)` → `durationSeconds(d)` → 5
- `addDuration(instant, duration)` produces correct future/past

### std.test
- A test file with 2 passing and 1 failing test reports 2/3 passed
- `expectEq` failure shows expected vs actual values
- `before`/`after` hooks execute in the correct order

### std.math
- `sqrt(4.0) == 2.0`, `sqrt(-1.0)` returns NaN
- `sin(PI) ≈ 0` within 1e-10
- All functions handle edge cases (Infinity, NaN) without crashing

### std.regex
- `compile("[0-9]+")` matches "abc123def" → find returns "123"
- Replace works with backreferences: `replace(r"(\w+) (\w+)", "$2 $1")`
- Invalid pattern returns `Err`

---

## Dependencies

- [Error Handling](gap-error-handling.md): `Result<T, E>` required for non-crashing APIs.
- `Hash` trait definition must be added to prelude.
- Native (C++) backing for performance-critical functions.

## Potential Challenges

- `Hash` trait interacts with the existing auto trait system — manually implementing `Hash` for all primitives is tedious but mechanical.
- HashMap with `string` keys requires string hashing in C++ that matches NG string representation.
- JSON parser in C++ vs NG: C++ is faster (simdjson). NG is safer. Hybrid approach recommended (C++ for parsing, NG for traversal).
- Regex engine: `pcre2` is GPL-licensed. `std::regex` has poor performance. Recommend `RE2` (BSD-licensed) or `Oniguruma`.
- `std.time` timezone support is intentionally excluded — UTC-only simplifies everything enormously.