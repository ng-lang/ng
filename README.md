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
- The test suite sweeps every example end to end (1700+ assertions / 430+ cases)

## Getting Started

### Prerequisites

- A modern C++ compiler (macOS pins `clang`/`clang++` with libc++; Homebrew LLVM supplies `clang-tidy`/`clang-format`)
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
- Modules and imports; redesigned stdlib (string/seq/list/memory/imgui); native-only AOT execution through QBE + libngrt
- AOT imgui stubs in libngrt; the NG IDE example compiles/runs headlessly (full ImGui backend pending)

### Deferred (see the post-cutover plan for gating)

- `span<T>` views; in-place `pushBack`/array growth (runtime-session heap work)
- Tuple switch patterns; Self-typed trait-view methods
- Heap domains (`Box<T>`, `Gc`, `Arc`); `isize`/`usize`; declared C ABI/bindgen
- Concurrency (R10) and tooling (R11: formatter, LSP, debugger, package manager)

### Increment plan progress

Executing the [implementation increment plan](./docs/design/rearchitecture/09-increment-plan.md)
top-down; status and verification baselines live there ([10-baselines.md](./docs/design/rearchitecture/10-baselines.md)).

- ✅ INC-0 — ASan/UBSan build variant + deterministic leak baseline
- ✅ INC-1 — lightweight seeded PBT harness (`test/pbt.hpp`)
- ✅ INC-2 — conformance corpus runner, layers 1+2 (`test/conformance/`)
- ✅ INC-3 — differential contract runner, Const ↔ QBE (N=500/run)
- ✅ INC-4 — unsigned ordering comparisons (F1①); `u64::MAX > 5` now true
- ✅ INC-5 — unsigned checked arithmetic (F1③); u64 wrap attempts now trap
- ✅ INC-6 — DEC-A landed: implicit mixed comparisons rejected; `toFloat`/`toSigned`
- ✅ ARCH — lowering.cpp split: runtime helpers assembled from `.ssa` files into
  `libngrt_helpers.a`; `FunctionLowerer` distributed across cohesive TUs;
  typecheck.cpp split into 5 cohesion TUs + file-size guard
- ✅ INC-7 — `toString`/`parseInt` (Option-style) natives; argument-position overload fix
- ✅ PBT 收敛 — challenge-review follow-up: shared width/oracle/batch runner in
  `test/pbt.hpp`; fixed false-green literal bug; ~200 lines duplicated code removed
- ✅ INC-8 — `derive(Eq)`: field-level structural `==`/`!=`; nested Eq structs; float fields rejected
- ✅ INC-B — core traits (`Copy`/`Clone`/`Eq`) declared in `lib/std/prelude.ng`, moved out of
  compiler injection; derive validation now queries registered traits
- ❌ INC-A — prelude auto-loading: skipped by decision (typechecker overload registration is
  visibility-blind; explicit `import prelude;` required, auto-load deferred)
- ✅ INC-9 — diagnostics stage one: error-code namespaces (`E-move-used`, `E-borrow-conflict`,
  `E-compare-mixed-sign`, …) on core move/borrow/type/parse diagnostics; resolver Levenshtein≤2
  `did you mean` suggestions for unknown names; negative-contract corpus upgraded to `code:` directives
- ✅ INC-10 + DEC-B — `while`/`continue`/`break` control flow: bare `next` captured only by binder
  `loop`s; FlowIR header-branch while lowering; desugar-equivalence PBT (while+continue ≡ hand-written
  loop on const+native); `example/while_loops.ng`
- ✅ INC-11 — `derive(Show)`: prelude `export trait Show`; checker field validation for showable
  scalar/string/nested-Show fields; FlowIR synthesizes `name{field: value, ...}` strings (research A4
  format) with native `toString` for scalars and recursion for nested derived structs; golden + property
  PBT + negative contracts; `example/derive.ng`
- ⬜ INC-12 … INC-36 — pending

## Community

- **Discussions:** [GitHub Discussions](https://github.com/ng-lang/ng/discussions)
- **Issue Tracker:** [GitHub Issues](https://github.com/ng-lang/ng/issues)
- **Pull Requests:** [GitHub Pull Requests](https://github.com/ng-lang/ng/pulls)

## Contributing

Please read our [Contribution Guide](./CONTRIBUTING.md) to get started.
