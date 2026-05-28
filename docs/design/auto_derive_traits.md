# Auto Traits And Derive Traits Follow-Ups

The implemented `auto trait`, `derive(Copy)`, and `derive(Clone)` baseline is
archived in [archive/auto_derive_traits_baseline.md](archive/auto_derive_traits_baseline.md).

This active document tracks future derive/auto-trait work only.

## Remaining Scope

- Debug/Show-like derived traits with deterministic field-order formatting.
- Negative auto-trait controls or blocked auto-trait behavior, building on the
  archived generalized `= delete` baseline.
- Richer module artifact export/import behavior for derived impl evidence if
  future module formats need more than the current impl metadata.
- Diagnostics that report the exact field path preventing an auto trait or
  derived trait from applying.
- Validation that synthetic clone/drop behavior remains sound with future
  ownership and partial-move extensions.

## Dependencies

- Implemented baseline: [Module Artifact And Typechecker Integration](archive/module_artifact_typechecker.md)
- Implemented baseline: [Generalized `= delete`](archive/generalized_delete.md)
- Implemented baseline: [Standard Library Modularization](archive/stdlib_modularization.md)

## Acceptance Criteria

- Future derived formatting traits must work in both STUPID and ORGASM.
- Negative or blocked auto-trait behavior must be specified before
  implementation, including coherence and diagnostics.
- Any new derived impl evidence must remain compatible with source and bytecode
  module imports.
