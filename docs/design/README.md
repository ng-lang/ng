# Design Documents Index

## Active Implementation Designs

Designs whose scoped implementation has landed are archived in [archive/](archive/).

## Active Follow-Ups (Previously)

1. [Enhanced Tuple Types](enhanced_tuples.md)
2. [Auto Traits And Derive Traits](auto_derive_traits.md)
3. [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md)
4. [Symbol Import Aliases](symbol_import_aliases.md)

## Gap Analysis Proposals (2026-06 — Revised After Feasibility Review)

The following 15 proposals address critical gaps in NG as a general-purpose programming language. Each includes motivation, design, scope, dependencies, acceptance criteria, effort estimates, and feasibility ratings.

### Feasibility Key

| Rating | Meaning |
|---|---|
| 🟢 Feasible | Can be implemented with current architecture |
| 🟡 Moderate | Needs architectural changes, but doable |
| 🔴 Redesigned | Proposal scope changed after feasibility analysis |

### Tier 1 — Foundation (Language Usability)

| # | Proposal | Priority | Feasibility | Effort |
|---|---|---|---|---|
| 1 | [Error Handling: Result, ?](gap-error-handling.md) | 🔴 P0 | 🟢 Feasible | 1-2 weeks |
| 2 | [Standard Library Expansion](gap-stdlib-expansion.md) | 🔴 P0 | 🟢 Feasible | 12 weeks (parallelizable) |
| 3 | [LSP Server and IDE Support](gap-lsp-ide.md) | 🔴 P0 | 🟡 Moderate | 6-10 weeks |
| 4 | [Code Formatter (ng fmt)](gap-formatter.md) | 🔴 P0 | 🟢 Feasible | 5 weeks |
| 5 | [Package Manager (git+path MVP)](gap-package-manager.md) | 🟡 P1 | 🟢 Feasible | 6 weeks |
| 6 | [Concurrency: Spawn/Wait](gap-concurrency.md) | 🟡 P1 | 🔴 **Redesigned** | 2.5 weeks |
| 7 | [Debugger (DAP — phased)](gap-debugger.md) | 🟡 P1 | 🟡 Moderate | 13 weeks (3 phases) |
| 8 | [C ABI / External FFI](gap-c-ffi.md) | 🟡 P1 | 🟡 Moderate (libffi) | 11 weeks |

### Tier 2 — Ecosystem (Developer Experience)

| # | Proposal | Priority | Feasibility | Effort |
|---|---|---|---|---|
| 9 | [Documentation Generator (ng doc)](gap-docgen.md) | 🟡 P1 | 🟢 Feasible | 9 weeks |
| 10 | [Syntax Ergonomics (3 batches)](gap-syntax-ergonomics.md) | 🔵 P2 | 🟢 Feasible | 6-8 weeks total |
| 11 | [Type System Enhancements](gap-type-system-enhancements.md) | 🔵 P2 | 🟡 Moderate | never: 2w, impl Trait: 3w, borrow: 6-8w |
| 12 | [Build System & Project Config](gap-build-system.md) | 🟡 P1 | 🟢 Feasible | 5.5 weeks |
| 13 | [Runtime Optimization (Embedding only)](gap-runtime-optimization.md) | 🔵 P2 | 🟡 Moderate | Embed: 6w; LLVM/WASM deferred |
| 14 | [Testing Framework & Benchmarks](gap-test-framework.md) | 🟡 P1 | 🟢 Feasible | 6.5 weeks |
| 15 | [Community Infrastructure](gap-community-infrastructure.md) | 🔵 P2 | 🟢 Feasible | Ongoing |

### Key Changes After Feasibility Review

| Proposal | Original | Revised | Reason |
|---|---|---|---|
| Concurrency | `async fun`/`await` (VM suspension) | `spawn`/`await` (thread pool) | VM cannot suspend; compiler cannot generate types |
| GAT | 4-6 months | **Deferred indefinitely** | No lifetime system exists |
| Borrow checker | Full Rust-style (3-4mo) | Lexical only (6-8w) | NLL is research-level complexity |
| LLVM AOT | 3-6 months | **Deferred to post-MVP** | GC stack maps are high complexity |
| C FFI | Assembly trampolines | Use `libffi` | Cross-platform, maintained |
| Debugger | Single phase | 3 phases | VM suspension shared with concurrency |
| Raise | `try`/`catch`/`throw` in Phase 1 | **Deferred to Phase 2** | `Result` + `?` covers 95% of needs |

### Priority Key

| Priority | Meaning | Target |
|---|---|---|
| 🔴 P0 | Blocking — language cannot be used without this | Q3 2026 |
| 🟡 P1 | Limiting — language is usable but severely constrained | Q4 2026 |
| 🔵 P2 | Enhancing — improves experience but not blocking | Q1-Q2 2027 |

## Archived Implemented Designs

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

## Dependency Notes

- The archived module artifact/typechecker work is the baseline for any future import/export or module metadata issue.
- The archived partial-move work is the baseline for future ownership, borrow-checking, and method-effect refinements.
- The archived const generics and const fun work are the baseline for future compile-time computation and type-level library work.
- New proposals in this directory should be moved to `archive/` once their scoped implementation has landed.