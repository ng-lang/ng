# Enhanced Tuple Follow-Ups

Implemented tuple and enhanced tuple behavior is archived in
[archive/tuples.md](archive/tuples.md) and
[archive/enhanced_tuples_baseline.md](archive/enhanced_tuples_baseline.md).

This active document tracks only remaining tuple work that still needs future
issues.

## Remaining Scope

- Richer tuple-rest slicing beyond unpack/rest binding.
- General sequence spread into calls from non-tuple runtime values in ORGASM
  lowering, if the language wants call-spread over arbitrary `Sequence<T>`.
- Function type interop derived from tuple types, such as converting
  `(A, B) -> R` and tuple argument carriers without relying on ad hoc call
  lowering.
- Better diagnostics for invalid type-level tuple projections after recursive
  `const fun` tuple utilities become expressive enough to replace intrinsics.

## Dependencies

- Implemented baseline: [Generalized `= delete`](archive/generalized_delete.md)
- Implemented baseline: [Constant Generic Parameters](archive/constant_generic_parameters.md)
- Implemented baseline: [`const fun`](archive/const_fun.md)
- Related active work: [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md)

## Acceptance Criteria

- Active follow-up issues must extend the archived tuple baseline without
  introducing a second tuple runtime representation.
- ORGASM should continue receiving fully monomorphized tuple shapes; unresolved
  generic tuple metadata remains out of scope for the current ORGASM level.
