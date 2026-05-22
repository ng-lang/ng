# Design Implementation Order

This directory is organized as implementation-sized design topics. The order below is the recommended Issue creation order. It is dependency-oriented, not alphabetical.

## Recommended Order

1. [Module Artifact And Typechecker Integration](module_artifact_typechecker.md)
2. [Partial Move Semantics](partial_move_semantics.md)
3. [Generalized `= delete` Declarations](generalized_delete.md)
4. [Constant Generic Parameters](constant_generic_parameters.md)
5. [Native Module Artifacts](native_module_artifacts.md)
6. [`const fun` And Compile-Time Computation](const_fun.md)
7. [Enhanced Tuple Types](enhanced_tuples.md)
8. [Auto Traits And Derive Traits](auto_derive_traits.md)
9. [Bytecode Module Loading](bytecode_module_loading.md)
10. [Standard Library Modularization](stdlib_modularization.md)
11. [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md)

## Dependency Summary

- Module artifact/typechecker integration is the root dependency for cross-module exports, trait impl evidence, native module descriptors, bytecode metadata, and standard library modularization.
- Partial move semantics should be implemented before generalized delete and enhanced tuples because both need deterministic initialized/uninitialized place rules.
- Generalized `= delete` is needed before designs rely on negative specialization or blocked declarations.
- Constant generic parameters are needed before type-level tuple element projection and tuple slicing bounds can be represented cleanly.
- Native module artifacts should follow the artifact model and precede stdlib modularization.
- `const fun` builds on const generics and enables compile-time arithmetic/predicates used by enhanced tuples and later constraints.
- Enhanced tuple types depend on generalized delete and const generics; its complete recursive/type-level form depends on `const fun`.
- Auto traits and derive traits depend on stable module-level impl visibility and generalized negative/block rules.
- Bytecode module loading should wait until artifact metadata, generic mangling, and native descriptors are stable enough to persist.
- Standard library modularization should wait until source, native, and bytecode module artifacts share one import/export model.
- Range/fold/pipeline syntax is intentionally last because it is mostly syntax and collection semantics, not a prerequisite for the generic/type-system foundation.

## Overview Documents

- [Tuple Design](tuples.md) records the current tuple implementation state and links to the enhanced tuple follow-up.
- [Module System Redesign](module_system.md) is a short overview that links to the module-related implementation documents.
