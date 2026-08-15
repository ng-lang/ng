# AGENTS.md

## Project Overview

**NG** is a statically-typed, multi-paradigm programming language implemented in modern C++ (C++23). The active pipeline is the **vNext** architecture (the legacy orgasm/interpreter pipeline has been removed):

**Lexer → Parser (syntax AST) → Resolver (HIR) → Type Checker → FlowIR → Bytecode → VM**

**Key directories and file patterns:**
- `src/vnext/syntax/` — lexer and parsers (`parser.cpp`, `module_parser.cpp`, `block_parser.cpp`, `type_parser.cpp`, `const_expr.cpp`)
- `src/vnext/hir.cpp` — name resolution: syntax AST → immutable `hir::Module` (functions, structs, enums, traits, impls, consts, opaque types)
- `src/vnext/typecheck.cpp` + `type_interner.cpp` — side-table style checker: expression types, call targets, trait/impl resolution, monomorphization, ownership (move/borrow/drop), const evaluation
- `src/vnext/flowir.cpp` — HIR → CFG (`flowir::Function`), incl. fold/map lowering, drop edges, trait-view dispatch
- `src/vnext/bytecode.cpp` + `bytecode/artifact.cpp` — FlowIR → verified bytecode + artifact codec
- `src/vnext/vm.cpp` + `vm/value_ops.cpp` — interpreter (`vm::VM`) over bytecode with native dispatch
- `src/vnext/const_eval.cpp` + `const_interp.cpp` — compile-time const evaluation / `const fun` interpreter
- `src/vnext/module_loader.cpp` — transitive `import` loading with per-module name visibility
- `src/vnext/driver.cpp` — `ngi` frontend: load → resolve → check → lower → compile → run, with registered natives
- `include/vnext/` — public vNext headers (`hir.hpp`, `typecheck.hpp`, `flowir.hpp`, `bytecode.hpp`, `value.hpp`, `syntax/*.hpp`, `driver.hpp`, ...)
- `example/vnext/*.ng` — runnable vNext examples; `lib/vnext_std/*.ng` — the vNext stdlib (prelude/io/string/seq/list/memory)
- `test/vnext/*.cpp` — Catch2 v3 test suites, one per feature area (`test/test.hpp` is the shared header)
- `docs/design/rearchitecture/` — vNext design decisions (D-0xx), the legacy example migration matrix, and the post-cutover roadmap

## Architecture & Patterns
- **Syntax:** `syntax::parseSourceUnit` / `Lexer` — a full-file lexer producing `Token` spans; `ModuleParser` handles module items, `ExpressionParser`/`BlockParser`/`TypeParser` handle nested forms. `>>` lexes as `ShiftRight` and is split by the generic-argument/type collectors.
- **HIR:** `hir::Resolver` resolves syntax into an immutable module. Expressions are `std::unique_ptr<hir::Expression>` nodes; types are `hir::Type` trees. Generic instances are produced by cloning + re-checking, not by AST mutation.
- **Type checking:** `typecheck::TypeChecker{}.check(module)` returns a `TypeCheckResult` of side tables (expression types, call targets, spread/fold positions, drop edges, trait-view tables, instances). Types are interned `typecheck::TypeId`s over `TypeDescriptor`s (builtins, arrays, tuples, structs/enums, refs, trait references, unions, type constructors/applications, opaque). Monomorphization re-checks cloned bodies under concrete substitutions.
- **Values:** `NG::vnext::Value` is a variant of int64/double/string/array/tuple/struct/enum/reference/trait-view/opaque/range (aggregates shared_ptr-backed). Copy-first deep-copy semantics; affine nominal types use `move`/`clone` + `impl Drop`; scoped `ref`/`ref mut` are non-returnable views. There is no GC (heap domains are deferred).
- **Lowering & VM:** FlowIR lowers to per-function CFGs with block parameters; bytecode is verified (operand/result type checks); the VM interprets with native dispatch keyed by function name (`native fun`).
- **Modules:** `import name;` / `import name (a, b);` merge transitively with per-module visible-name sets (own names, selective lists, exported surfaces with transitive re-export).
- **Testing:** each feature slice ships `test/vnext/<feature>_test.cpp` plus an `example/vnext/<feature>.ng` run end-to-end through `ngi`; the migration matrix (`docs/design/rearchitecture/05-legacy-example-migration-matrix.md`) records coverage of the removed legacy corpus.

## Build, Test, and Development Workflows
**Configure & build (Ninja):**

```bash
cmake -S . -B build -GNinja
cmake --build build -j
```
**Run tests:**

```bash
ctest --test-dir build -j
./build/ng_test --list-tests
./build/ng_test "vNext*"   # example filter
```
**Run interpreter:**

```bash
./build/ngi example/vnext/<example>.ng
```
**Format C++ code:**

```bash
clang-format -i src/vnext/**/*.cpp include/vnext/**/*.hpp
```

## Coding Style & Naming Conventions
- **C++23**; prefer RAII, `const` correctness, and explicit ownership
- **Types/classes:** PascalCase; **headers/sources:** snake_case (`type_interner.cpp`, `value.hpp`)
- **Tests:** `test/vnext/<topic>_test.cpp`; **examples:** `example/vnext/<topic>.ng` (NG code)
- **External dependencies:** vendored in `build/_deps/` and `vendored/`
- **Do not modify** files in `vendored/` or `build/_deps/`
- **AI-generated code** must be marked as such (see `CONTRIBUTING.md`)

## Testing Guidelines
- **Framework:** Catch2 v3 (vendored). Include `test/test.hpp` for the shared macros
- **Patterns:** one suite per feature slice; runtime behavior via `NG::vnext::runDriver({"--source", ...})`, examples via the shared `runExample("example/vnext/...")` helper
- **Keep the full suite green:** `./build/ng_test` must pass before committing; each feature round ends with a commit

## Commit & PR Guidelines
- **Prefer Conventional Commits** like history: `feat(vnext): ...`, `fix(parsing): ...`
- **Messages in imperative mood**
- **PRs must include:** clear description, linked issues, tests, docs/examples updates where relevant (e.g. `docs/design/rearchitecture/`, `example/vnext/`)
- **Disclose any AI-generated code** per `CONTRIBUTING.md`

## References
- [vNext design decisions](../docs/design/rearchitecture/04-language-decisions.md)
- [Legacy example migration matrix](../docs/design/rearchitecture/05-legacy-example-migration-matrix.md)
- [Post-cutover roadmap](../docs/design/rearchitecture/07-post-cutover-plan.md)
- [Contribution Guide](../CONTRIBUTING.md)

---
For any unclear conventions or missing documentation, consult the above references or ask in project discussions.
