# Ranges, Slicing, Fold, And Pipeline Follow-Ups

The implemented range, slicing, fold, pipeline, and `Sequence<T>` baseline is
archived in [archive/ranges_slicing_pipeline_baseline.md](archive/ranges_slicing_pipeline_baseline.md).

This active document tracks only future collection-syntax work.

## Remaining Scope

- Range step syntax and semantics.
- Multi-sequence zip/map2 and zip-with forms.
- N-ary fold over functions with more than one sequence input.
- Postfix fold expressions targeting imported, native, generic, or indirect
  function values.
- A richer `Sequence<T>` standard-library algorithm layer beyond the current
  `size`/`get` protocol.
- A final decision for standalone `?` syntax outside the fixed `?...` filter
  marker.

## Dependencies

- Implemented baseline: [Constant Generic Parameters](archive/constant_generic_parameters.md)
- Implemented baseline: [Standard Library Modularization](archive/stdlib_modularization.md)
- Related active work: [Enhanced Tuple Follow-Ups](enhanced_tuples.md)

## Acceptance Criteria

- Future range/fold extensions must preserve the existing distinction:
  `|>` is pipeline call composition, while postfix `...` drives fold/map/filter.
- Multi-sequence forms must reject length or shape ambiguity explicitly rather
  than silently truncating unless a later design specifies truncation.
- Imported/generic fold targets need STUPID and ORGASM parity tests before the
  feature is considered complete.
