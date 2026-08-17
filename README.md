# NG Programming Language

[![build](https://github.com/ng-lang/ng/actions/workflows/build.yml/badge.svg)](https://github.com/ng-lang/ng/actions/workflows/build.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e72d75eb4dbf4a0e9617cbced2f4ec1e)](https://app.codacy.com/gh/ng-lang/ng/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Codacy Badge](https://app.codacy.com/project/badge/Coverage/e72d75eb4dbf4a0e9617cbced2f4ec1e)](https://app.codacy.com/gh/ng-lang/ng/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_coverage)
[![codecov](https://codecov.io/github/ng-lang/ng/graph/badge.svg?token=T5RV6EWVSG)](https://codecov.io/github/ng-lang/ng)

NG is a statically-typed, multi-paradigm programming language with a single clean pipeline:

**Lexer → Parser → Resolver (HIR) → Type Checker → FlowIR → Bytecode → VM**

## Features

- Fixed-width integers (`i8`–`i64`, `u8`–`u64`) and `f32`/`f64` with checked arithmetic, strings, arrays, tuples, structs, tagged unions, ranges, and union annotations
- Copy-first ownership: affine moves/clones, field-aware partial moves, `impl Drop`, and borrow-checked scoped references with non-lexical loans — no GC
- Traits with default methods, concrete and generic impls, bounds, auto traits, `derive`, and `ref<Trait>` dynamic views
- Generics (monomorphized), higher-kinded constructors, variadic packs, const generics
- Compile-time programming: `const if`, const declarations with pattern specialization, `const fun` (incl. generic const funs), const-capable native hosts
- A redesigned standard library (`lib/std`), a Dear ImGui binding over SDL3, and a minimal IDE written in NG
- The test suite sweeps every example end to end (1700+ assertions / 430+ cases)

## Getting Started

### Prerequisites

- A modern C++ compiler (macOS pins `clang`/`clang++` with libc++; Homebrew LLVM supplies `clang-tidy`/`clang-format`)
- CMake 3.25+ and Ninja

### Build and run

```bash
cmake -S . -B build -GNinja
cmake --build build -j
./build/ngi example/hello_world.ng                 # run an example
./build/ngi --source 'import prelude; fun main() { print("hi"); }'
./build/ngi --native --output hello example/hello.ng  # compile to native executable
./build/ngi_imgui example/ng_ide.ng --fuel 0       # the imgui IDE (GUI)
```

Run the tests:

```bash
./build/ng_test            # full suite
ctest --test-dir build -j  # or through CTest
```

## Documentation

- [Language Guide](./docs/guide/language_guide.md) (site: `docs/index.md` via VitePress)
- [Internals](./docs/ref/Internals.md) · [Memory](./docs/ref/Memory.md) · [C++ Compatibility](./docs/ref/cxx-compatibility.md)
- [Design decisions](./docs/design/rearchitecture/README.md) (the current authority) and the [post-cutover plan](./docs/design/rearchitecture/07-post-cutover-plan.md)

## Roadmap

### Done

- Pattern matching: enum-variant switches (exhaustive) and scalar literal-or switches
- Generics: type parameters, parameter packs, monomorphization, const generics, HKT
- Ownership: moves, partial moves, Drop, scoped refs, NLL borrow release
- Traits: static dispatch, default methods (through views too), generic impls, auto traits, derive, `ref<Trait>` views
- Compile-time: `const if`, const declarations, `const fun`, const-capable natives
- Modules and imports; redesigned stdlib (string/seq/list/memory/imgui); bytecode VM with verified artifacts
- Self-hosting `runNgi` and the NG IDE on the imgui binding

### Deferred (see the post-cutover plan for gating)

- `span<T>` views; in-place `pushBack`/array growth (runtime-session heap work)
- Tuple switch patterns; Self-typed trait-view methods
- Heap domains (`Box<T>`, `Gc`, `Arc`); `isize`/`usize`; declared C ABI/bindgen
- Concurrency (R10) and tooling (R11: formatter, LSP, debugger, package manager)

## Community

- **Discussions:** [GitHub Discussions](https://github.com/ng-lang/ng/discussions)
- **Issue Tracker:** [GitHub Issues](https://github.com/ng-lang/ng/issues)
- **Pull Requests:** [GitHub Pull Requests](https://github.com/ng-lang/ng/pulls)

## Contributing

Please read our [Contribution Guide](./CONTRIBUTING.md) to get started.
