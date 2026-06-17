# Design Documents Index

## Active Implementation Designs

Designs whose scoped implementation has landed are archived in [archive/](archive/).

## Active Follow-Ups (Previously)

1. [Enhanced Tuple Types](enhanced_tuples.md)
2. [Auto Traits And Derive Traits](auto_derive_traits.md)
3. [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md)
4. [Symbol Import Aliases](symbol_import_aliases.md)

## Gap Analysis Proposals (2026-06)

The following 16 proposals address critical gaps in NG as a general-purpose programming language. Each includes motivation, design, scope, dependencies, and acceptance criteria.

### Tier 1 — Foundation (Language Usability)

| # | Proposal | Priority | Dependencies |
|---|---|---|---|
| 1 | [Error Handling: Result, ?, try/catch](gap-error-handling.md) | 🔴 P0 | None (standalone) |
| 2 | [Standard Library Expansion](gap-stdlib-expansion.md) | 🔴 P0 | Error handling (Result) |
| 3 | [LSP Server and IDE Support](gap-lsp-ide.md) | 🔴 P0 | Stable parser API |
| 4 | [Code Formatter (ng fmt)](gap-formatter.md) | 🔴 P0 | Tree-sitter or stable AST |
| 5 | [Package Manager](gap-package-manager.md) | 🟡 P1 | Module path resolution |
| 6 | [Concurrency Model (Async/Await)](gap-concurrency.md) | 🟡 P1 | Error handling, VM rework |
| 7 | [Debugger (DAP Adapter)](gap-debugger.md) | 🟡 P1 | Source maps, VM suspension |
| 8 | [C ABI / External FFI](gap-c-ffi.md) | 🟡 P1 | Raw pointer type, unsafe blocks |

### Tier 2 — Ecosystem (Developer Experience)

| # | Proposal | Priority | Dependencies |
|---|---|---|---|
| 9 | [Documentation Generator (ng doc)](gap-docgen.md) | 🟡 P1 | Stable parser, module resolution |
| 10 | [Syntax Ergonomics](gap-syntax-ergonomics.md) | 🔵 P2 | Parser changes |
| 11 | [Type System Enhancements](gap-type-system-enhancements.md) | 🔵 P2 | HKT infrastructure |
| 12 | [Build System & Project Config](gap-build-system.md) | 🟡 P1 | Package manager |
| 13 | [Runtime Optimization (AOT/WASM/Embed)](gap-runtime-optimization.md) | 🔵 P2 | LLVM, VM refactoring |
| 14 | [Testing Framework & Benchmarks](gap-test-framework.md) | 🟡 P1 | Error handling |
| 15 | [Community Infrastructure](gap-community-infrastructure.md) | 🔵 P2 | Documentation, website |
| — | (User-level exceptions integrated into gap-error-handling.md) | — | — |

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