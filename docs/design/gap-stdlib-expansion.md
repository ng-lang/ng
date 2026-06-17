# Standard Library Expansion

## Order

Recommended implementation order: **2** (stdlib is the primary interface developers interact with).

## Goal

Expand NG's standard library from its current minimal state (9 small modules, ~450 lines total) to a production-ready set covering collections, I/O, serialization, time, and testing.

## Current State

```
stdlib total: ~450 lines of NG + ~2,100 lines of C++ native glue
Active modules: prelude, io, string, array, list, seq, tuple, memory, imgui
```

Missing critical modules: JSON, HashMap, DateTime, Regex, network, testing, math.

## Proposed Module Map

### Tier 1 — Core (implement first)

| Module | Contents |
|---|---|
| `std.collections` | `HashMap<K, V>`, `HashSet<T>`, `Deque<T>`, `PriorityQueue<T>` |
| `std.json` | `parse(string) -> Result<JsonValue, JsonError>`, `stringify(JsonValue) -> string` |
| `std.time` | `DateTime`, `Duration`, `Instant`, `sleep`, `Timer` |
| `std.test` | `test` macro/block, `expect`, `expectEq`, `bench`, `describe`/`it` style |
| `std.math` | `sqrt`, `sin`, `cos`, `tan`, `abs`, `min`, `max`, `clamp`, `floor`, `ceil`, `round` |

### Tier 2 — I/O & Networking (implement next)

| Module | Contents |
|---|---|
| `std.net` | `TcpListener`, `TcpStream`, `UdpSocket`, `DnsResolver` |
| `std.http` | `Client`, `Request`, `Response`, `Server` |
| `std.fs` | `Path`, `DirIterator`, `FileType`, `Permissions`, `TempDir` |
| `std.process` | `Command`, `Output`, `Child`, `Env` |

### Tier 3 — Data & Formatting (implement later)

| Module | Contents |
|---|---|
| `std.regex` | `Regex` type, `match`, `find`, `replace`, `split`, capture groups |
| `std.crypto` | `sha256`, `randomBytes`, `uuid` |
| `std.serialize` | `BinWriter`, `BinReader`, `JsonSerializer` |
| `std.base64` | `encode`, `decode` |

### Refactoring Existing Modules

- `std.array` should expose `HashMap`, `HashSet` as well as `reverse`/`sort`.
- `std.string` should be unified with `regex`.
- `std.io` should return `Result` instead of crashing (see [Error Handling](gap-error-handling.md)).

## Dependencies

- [Error Handling](gap-error-handling.md): `Result<T, E>` required for non-crashing I/O.
- HashMap requires hashing — needs a `Hash` trait.
- JSON needs `string` → structured data mapping.
- DateTime needs `i64` timestamp representation.

## Scope

**In scope:**
- Each module as a separate `lib/std/` file with NG source
- C++ native backing for performance-critical operations
- Gradual migration: start with Tier 1, add Tier 2/3 over time
- `Hash` trait definition in prelude

**Out of scope:**
- Async I/O (defer to concurrency model, [Concurrency](gap-concurrency.md))
- Full Unicode tables (initial ASCII + UTF-8 validity only)
- WASM-specific stdlib variants

## Acceptance Criteria

- `HashMap` stores and retrieves values by key
- `JSON.parse` produces a traversable tree
- `DateTime` arithmetic (add days, compare, format)
- `test` module allows writing and running unit tests
- `math.sqrt` produces correct results for common inputs
- All stdlib functions return `Result` on error paths
- Benchmarks showing native-backed functions are not slower than hand-written C++
- ORGASM VM supports the new native functions

## Potential Challenges

- `HashMap` needs `Hash` trait + `Eq` trait — this interacts with the auto trait system.
- `JSON` parser in NG vs C++ tradeoff: C++ is faster but harder to maintain.
- `Regex` engine is complex; consider reusing an existing C library (PCRE2, RE2).
- DateTime timezone support is notoriously complex — start with UTC only.