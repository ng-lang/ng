# Active Design Index

This directory now contains only designs that still need future issue tracking or
follow-up implementation. Designs whose scoped implementation has landed are
archived under [archive/](archive/).

## Active Follow-Ups

1. [Enhanced Tuple Types](enhanced_tuples.md)
   - Remaining tuple type-level follow-ups and any tuple-specific work not
     covered by the completed range/slicing implementation.
2. [Auto Traits And Derive Traits](auto_derive_traits.md)
   - Future derived formatting traits, negative auto-trait controls, and richer
     module artifact behavior.
3. [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md)
   - Future range steps, multi-sequence zip/map2, n-ary fold, richer sequence
     algorithms, and imported/generic fold target coverage.
4. [Symbol Import Aliases](symbol_import_aliases.md)
   - Future symbol-level import aliasing. Current conflict avoidance remains
     module-qualified imports.

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

- The archived module artifact/typechecker work is the baseline for any future
  import/export or module metadata issue.
- The archived partial-move work is the baseline for future ownership,
  borrow-checking, array partial move, and method-effect refinements.
- The archived const generics and const fun work are the baseline for future
  compile-time computation and type-level library work.
- New design docs should stay in this active directory until their scoped
  implementation has landed, then move to `archive/`.
