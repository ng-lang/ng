# Design Documents Index

This directory contains design proposals for NG. Designs are organized into three categories:

- **Active follow-ups** — features that have partial implementation but remaining work
- **Gap proposals** — proposals addressing missing features for NG as a general-purpose language
- **Archived** — fully implemented designs, kept for reference

---

## Active Follow-Ups

Designs with partial implementation, tracking remaining work:

1. [Enhanced Tuple Types](enhanced_tuples.md) — type-level tuple operations, remaining follow-ups
2. [Auto Traits And Derive Traits](auto_derive_traits.md) — future derived formatting traits, negative auto-trait controls
3. [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md) — range steps, multi-sequence zip, n-ary fold
4. [Symbol Import Aliases](symbol_import_aliases.md) — `import std.foo (bar as baz)` syntax

---

## Gap Proposals

15 proposals addressing critical gaps in NG as a general-purpose programming language. Each has been reviewed against the existing codebase architecture and refined for implementation readiness.

### Feasibility Ratings

| Rating | Meaning | Color |
|---|---|---|
| Feasible | Can be implemented with current architecture — additive changes only | 🟢 |
| Moderate | Needs targeted architectural additions (new files, no rewrites) | 🟡 |
| Redesigned | Proposal scope changed after architecture analysis | 🔴 |

### Codebase Quality Baseline

Before implementing: the existing 28,804-line codebase was audited. **Verdict: no rewrite needed.** The architecture (Visitor pattern, clean dependency direction), test coverage (695 tests, 3,327 assertions), and code quality (modern C++23, RAII, no circular dependencies) are solid. Three minor technical debts exist (10 global statics in typecheck.cpp, 800-line VM opcode switch, 39 repetitive numeral type patterns) but none block feature delivery. Estimated fix time: ~2 weeks total, can be done incrementally.

### Proposals

#### P0 — Foundation (Language Usability)

| # | Proposal | Effort | Feasibility | Summary |
|---|---|---|---|---|
| 1 | [Error Handling](gap-error-handling.md) | 1-2 wks | 🟢 Feasible | `Result<T,E>` + `?` operator. `?` desugars to existing `switch`/`return` — no new opcodes. Uses existing `QUERY` token. `try`/`catch`/`throw` deferred to Phase 2. |
| 2 | [Standard Library Expansion](gap-stdlib-expansion.md) | 12 wks parallelizable | 🟢 Feasible | `HashMap` + `Hash` trait, `JSON`, `DateTime`, `math`, `regex`, `test`. Each module is additive (new `.ng` file + optional C++ native). |
| 3 | [LSP Server and IDE Support](gap-lsp-ide.md) | 6-10 wks | 🟡 Moderate | Tree-sitter grammar + DAP-compatible server. Type checker integration via new `DocumentCache` wrapper (~200 lines, no changes to existing `typecheck.cpp`). |
| 4 | [Code Formatter](gap-formatter.md) | 5 wks | 🟢 Feasible | `ng fmt` as standalone tool. Hand-written AST pretty-printer (chosen over tree-sitter). Idempotent output, `--check`/`--diff` modes. |

#### P1 — Ecosystem (Developer Experience)

| # | Proposal | Effort | Feasibility | Summary |
|---|---|---|---|---|
| 5 | [Package Manager](gap-package-manager.md) | 6 wks | 🟢 Feasible | Git + path dependencies only (MVP). No registry. Sets `NG_MODULE_PATH` — leverages existing module resolution. |
| 6 | [Concurrency: Spawn/Wait](gap-concurrency.md) | 2.5 wks | 🔴 **Redesigned** | **Replaced** `async fun`/`await` (requires VM suspension — impossible without major rework) with `spawn`/`await` using C++ thread pool. Each task runs in its own VM instance — no GC thread-safety issues. |
| 7 | [Debugger (Phased)](gap-debugger.md) | 13 wks total | 🟡 Moderate | Phase 1: source maps + DAP skeleton. Phase 2: VM suspension + breakpoints (shared infrastructure with concurrency). Phase 3: variable inspection + stepping. |
| 8 | [C ABI / External FFI](gap-c-ffi.md) | 11 wks | 🟡 Moderate | `extern fun`, `*T` raw pointers, `unsafe` blocks. Uses `libffi` (not assembly trampolines) for cross-platform calling convention. GC safety: Phase 1 restricts `*T` to non-GC memory. |
| 9 | [Documentation Generator](gap-docgen.md) | 9 wks | 🟢 Feasible | `///` doc comments → HTML/Markdown. `ng doc --serve`, `ng doc --test`. Standalone tool using existing lexer + parser. |
| 10 | [Build System](gap-build-system.md) | 5.5 wks | 🟢 Feasible | `ng.toml` project manifest, `ng build`/`ng run`, incremental compilation with dependency graph + cache invalidation. |
| 11 | [Testing Framework](gap-test-framework.md) | 6.5 wks | 🟢 Feasible | `test "name" { }` as special AST node (like `const if`). Per-test VM isolation. Benchmark runner with statistical method. Property testing (`forAll`). |

#### P2 — Advanced Features

| # | Proposal | Effort | Feasibility | Summary |
|---|---|---|---|---|
| 12 | [Syntax Ergonomics (3 batches)](gap-syntax-ergonomics.md) | 6-8 wks total | 🟢 Feasible | Batch 1: string interpolation + `for`/`while` + `break`/`continue`. `for` reuses existing `LoopBindingType::LOOP_IN` — no new AST node. Batch 2: lambdas/closures. Batch 3: `match` expression + operator overloading. |
| 13 | [Type System Enhancements](gap-type-system-enhancements.md) | 2+3+8 wks | 🟡 Moderate | Sub-A: `never` type (~2w, 🟢). Sub-B: `impl Trait` return/argument position (~3w, 🟡). Sub-C: lexical borrow checker (~6-8w, reuses existing `movedBindings` infrastructure). **GATs deferred indefinitely** — requires lifetime system (doesn't exist). |
| 14 | [Runtime Optimization](gap-runtime-optimization.md) | 6 wks | 🟡 Moderate | **Embedding C API only** (highest value, lowest risk): `libng` shared library with `ng_vm_create()`/`ng_vm_run_file()`/`ng_vm_call()`. **LLVM AOT deferred to post-MVP** (GC stack maps are complex). **WASM via Emscripten** deferred. |
| 15 | [Community Infrastructure](gap-community-infrastructure.md) | Ongoing | 🟢 Feasible | Website (Hugo), playground (server-side ngi via Docker), RFC process, public roadmap. No code changes to the compiler. |

### Key Design Decisions

#### Conflicts Resolved

| Conflict | Resolution | Applies To |
|---|---|---|
| `for` keyword: `impl Trait for Type` vs `for i in range` | Parser disambiguates by context (statement-level vs impl-level). `for i in range` reuses existing `LoopBindingType::LOOP_IN` — no new `ForStatement` AST node. | syntax-ergonomics |
| `?` operator: new token vs existing `QUERY` token | Uses existing `QUERY` token (line 148 in `token.hpp`). Postfix vs ternary disambiguated by position. | error-handling |
| `yield` keyword vs existing `YIELD_STATEMENT` | Adds `KEYWORD_YIELD` token, reuses existing `YIELD_STATEMENT = 0x504` AST node. | concurrency |
| `try`/`catch`/`throw` vs `Result` + `?` | Phase 1: `Result` + `?` only (covers 95% of needs). Phase 2: `try`/`catch`/`throw` deferred until real-world usage shows the need. | error-handling |

#### Features Deferred

| Feature | Reason | Revisit Condition |
|---|---|---|
| GAT (Generic Associated Types) | Requires lifetime system (doesn't exist, 12+ months to build) | If/when NG gains lifetime annotations |
| Async/await state machines | VM cannot suspend; compiler cannot generate types | After VM has suspension infrastructure (post-MVP) |
| LLVM AOT compilation | GC stack maps for precise GC are very complex | When bytecode VM performance is measured as insufficient |
| JIT compilation | Too speculative; no clear use case | If benchmarks show a need |
| WASM direct compilation | Requires LLVM backend first | After LLVM AOT exists |

### Cross-Proposal Dependencies

```
Error Handling → Stdlib (Result-based APIs)
Error Handling + Stdlib → Concurrency (spawned tasks need error types)
Embedding API (libng) → Build System, Testing Framework, Package Manager
VM Suspension → Debugger Phases 2-3
```

### Effort Summary

| Phase | Proposals | Total Effort | Dependencies |
|---|---|---|---|
| Q3 2026 | 1-4: Error Handling, Stdlib, Formatter, Syntax Batch 1 | ~20 weeks | None on each other |
| Q4 2026 | 5-7, 9-11: Package Manager, Concurrency, LSP, Docgen, Build, Test | ~42 weeks | libng extraction |
| Q1-Q2 2027 | 8, 12-14: C FFI, Syntax Batch 2-3, Type System, Embedding | ~40 weeks | Previous phases |
| Deferred | LLVM AOT, GAT, Async/await, JIT | — | See conditions above |

**Estimated total: ~100 weeks of developer time (~2 developers for 1 year).**

---

## Archived Implemented Designs

Designs whose scoped implementation has fully landed:

- [Tuple Design](archive/tuples.md)
- [Enhanced Tuple Implemented Baseline](archive/enhanced_tuples_baseline.md)
- [Module System Redesign Overview](archive/module_system.md)
- [Module Artifact And Typechecker Integration](archive/module_artifact_typechecker.md)
- [Partial Move Semantics](archive/partial_move_semantics.md)
- [Generalized `= delete` Declarations](archive/generalized_delete.md)
- [Constant Generic Parameters](archive/constant_generic_parameters.md)
- [Native Module Artifacts](archive/native_module_artifacts.md)
- [`const fun` And Compile-Time Computation](archive/const_fun.md)
- [Bytecode Module Loading](archive/bytecode_module_loading.md)
- [Standard Library Modularization](archive/stdlib_modularization.md)
- [Auto Traits And Derive Traits Implemented Baseline](archive/auto_derive_traits_baseline.md)
- [Ranges, Slicing, Fold, And Pipeline Implemented Baseline](archive/ranges_slicing_pipeline_baseline.md)

## Policy

- New designs should stay in this directory until their scoped implementation has landed, then move to `archive/`.
- Each proposal must include: motivation, design, scope, dependencies, acceptance criteria, effort estimate, and feasibility rating.
- Design decisions and conflicts should be documented in each proposal's "Design Decisions" section.