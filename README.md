# NG Programming Language

[![build](https://github.com/ng-lang/ng/actions/workflows/build.yml/badge.svg)](https://github.com/ng-lang/ng/actions/workflows/build.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e72d75eb4dbf4a0e9617cbced2f4ec1e)](https://app.codacy.com/gh/ng-lang/ng/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Codacy Badge](https://app.codacy.com/project/badge/Coverage/e72d75eb4dbf4a0e9617cbced2f4ec1e)](https://app.codacy.com/gh/ng-lang/ng/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_coverage)
[![codecov](https://codecov.io/github/ng-lang/ng/graph/badge.svg?token=T5RV6EWVSG)](https://codecov.io/github/ng-lang/ng)

NG is a statically-typed, multi-paradigm programming language with a single clean pipeline:

**Lexer → Parser → Resolver (HIR) → Type Checker → FlowIR → QBE IL → Native executable**

## Features

- Fixed-width integers (`i8`–`i64`, `u8`–`u64`) and `f32`/`f64` with checked arithmetic, strings, arrays, tuples, structs, tagged unions, ranges, and union annotations
- Copy-first ownership: affine moves/clones, field-aware partial moves, `impl Drop`, and borrow-checked scoped references with non-lexical loans — no GC
- Traits with default methods, concrete and generic impls, bounds, auto traits, `derive`, and `ref<Trait>` dynamic views
- Generics (monomorphized), higher-kinded constructors, variadic packs, const generics
- Compile-time programming: `const if`, const declarations with pattern specialization, `const fun` (incl. generic const funs), const-capable native hosts
- A redesigned standard library (`lib/std`), a Dear ImGui binding over SDL3, and a minimal IDE written in NG
- The test suite sweeps every example end to end (4139 assertions / 662 cases)

## Getting Started

### Prerequisites

- A C++23 compiler (macOS pins `clang`/`clang++` with libc++; Homebrew LLVM supplies `clang-tidy`/`clang-format`)
- CMake 3.25+ and Ninja

### Build and run

```bash
cmake -S . -B build -GNinja
cmake --build build -j
./build/ngi example/stdlib_basics.ng               # run an example
./build/ngi --source 'import prelude; fun main() { print("hi"); }'
./build/ngi --output hello example/stdlib_basics.ng  # compile to native executable
./build/ngi --source 'import imgui; fun main() -> i64 { imguiInit(); if (imguiAborted()) { imguiCleanup(); return 1; } imguiCleanup(); return 0; }'  # headless imgui AOT stub
```

Run the tests:

```bash
./build/ng_test            # full suite
ctest --test-dir build -j  # or through CTest
```

## Documentation

User documentation (VitePress site) lives in [`docs/`](./docs/index.md):

- [Language Guide](./docs/guide/language_guide.md) — orientation and feature overview
- [Accepted Language Decisions](./docs/ref/language-decisions.md) — the normative rules
  enforced today
- [Memory](./docs/ref/Memory.md) · [C++ Compatibility](./docs/ref/cxx-compatibility.md)

## Implemented today

- Pattern matching: exhaustive enum-variant switches, scalar literal-or switches,
  and structural union switches (`case (x: T)`, member literals, `otherwise`)
- Generics: type parameters, monomorphization, const generics, higher-kinded
  constructors, variadic packs, `where` clauses
- Ownership: moves, field-aware partial moves, explicit `move`, `Drop`, scoped
  `ref`/`ref mut`, non-lexical loans, origin contracts
- Traits: static dispatch, default methods (including through views), generic
  impls, auto traits, `derive`, and `ref<Trait>` dynamic views
- Compile-time: `const if`, const declarations with pattern specialization,
  `const fun` (including generic const funs), const-capable native hosts
- Closures with `move`/copy/borrow captures, higher-order `seq` combinators
- Option/Result with combinators and the `?` operator
- Modules and imports; a redesigned stdlib (`prelude`, `io`, `string`, `seq`,
  `list`, `memory`, `system`, `imgui`); native-only execution through QBE + `libngrt`
- Generic `Box<T>` and `Cell<T>`; `span<T>` with in-place `pushBack`
- Verification tooling: ASan/UBSan variant, property-based tests, conformance
  corpus, and a Const ↔ QBE differential runner

## Community

- **Discussions:** [GitHub Discussions](https://github.com/ng-lang/ng/discussions)
- **Issue Tracker:** [GitHub Issues](https://github.com/ng-lang/ng/issues)
- **Pull Requests:** [GitHub Pull Requests](https://github.com/ng-lang/ng/pulls)

## Contributing

Please read our [Contribution Guide](./CONTRIBUTING.md) to get started.
