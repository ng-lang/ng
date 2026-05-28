# Ranges, Slicing, Fold, And Pipeline Syntax

## Order

Recommended Issue order: 10.

## Goal

Define collection-oriented syntax that was originally discussed alongside tuples but is not a prerequisite for enhanced generics or tuple type introspection.

This design is intentionally last in the current dependency order because it adds surface syntax and collection semantics rather than foundational type/module infrastructure.

## Dependencies

Prerequisites:

- Current tuple and array value semantics.
- [Enhanced Tuple Types](../enhanced_tuples.md), if tuple slicing must preserve exact tuple element types.
- [Constant Generic Parameters](archive/constant_generic_parameters.md), for compile-time tuple slice bounds.

Optional:

- [Standard Library Modularization](archive/stdlib_modularization.md), for exposing
  sequence container APIs. Range, slice, and fold syntax is not implemented as
  stdlib helper calls.

## Scope

In scope:

- Range operator syntax: `a..b`, `a..=b`, `..b`, `a..`.
- From-end index syntax: `^n`.
- Array and tuple indexing with ranges.
- Tuple slicing type preservation for compile-time bounds.
- Postfix fold expressions for map/filter/fold over one sequence.
- Pipeline syntax as ordinary call composition, independent from fold syntax.

Out of scope:

- Tuple value basics.
- Generic pack mechanics.
- Module system changes.
- Multi-sequence zip/map2 and zip-with forms.
- N-ary fold over functions with more than one sequence input.
- General meaning for `?` outside the fixed `?...` filter marker.

## Current Baseline

Already exists:

- Lexer tokens for `..`, `..=`, `...`, and `^`.

Implemented in this phase:

- Range expressions as AST/typechecked/runtime values for integral bounds,
  producing `Range<T>` values. Unsuffixed integer bounds default to
  `Range<i32>`, while typed bounds such as `1u32..10u32` produce `Range<u32>`.
- End-exclusive `a..b` and end-inclusive `a..=b`.
- Open ranges inside index access: `xs[..b]` and `xs[a..]`.
- From-end index syntax `xs[^n]`.
- Vector/fixed-array slicing with `xs[a..b]`, producing a `span<T>` view.
- Tuple slicing with compile-time bounds and preserved tuple type shape.
- General pipeline syntax `value |> f` and `value |> f(args...)`, lowered to
  `f(value)` and `f(value, args...)`.
- Postfix fold expressions for single-sequence map/filter/fold using direct
  local non-generic functions:
  `[mapper(xs)...]`, `[predicate(xs)?...]`, `op(xs..., init)`, and
  `op(init, xs...)`.
- Mixed array literals containing map/filter fold segments:
  `[0, mapper(xs)..., 9]`.
- Range/span materialization through spread in array literals: `[...(0..4)]`
  and `[...xs[1..3]]`.
- `std.seq.Sequence<T>` exposes `size` and `get`; vector, fixed array, span,
  Range, and the NG-authored `std.list.List<T>` satisfy the protocol.
- `List<T>` can be materialized through spread in both STUPID and ORGASM via
  the same `Sequence<T>` `size`/`get` protocol.
- `listof<T>(args: T...) -> List<T>` uses homogeneous varargs: `T` is a single
  element type, while `T...` in the parameter annotation collects runtime
  arguments into a sequence value.
- `List<T>` has inherent `pushBack`, `append`, `size`, and `get` methods. The
  exported free `pushBack`/`append`/`get` wrappers remain available as ordinary
  stdlib functions for code that wants explicit ref-passing.

Still not implemented:

- Range step syntax.
- Multi-sequence zip/map2 and n-ary fold.
- Postfix fold expressions targeting imported, native, generic, or indirect
  function values.
- Richer `Sequence<T>` algorithms beyond the current `size`/`get` protocol.
- `std.list` is implemented in NG rather than as a native runtime container.

## Fold Expressions

Fold expressions use postfix `...` in value-expression contexts. This is
separate from existing prefix spread/unpack `...expr`, and separate from type
pack syntax such as `T...`.

```ng
val xs = [1, 2, 3];

val ys = [inc(xs)...];       // map: equivalent to map(inc, xs)
val evens = [even(xs)?...];  // filter: equivalent to filter(even, xs)
val total_r = plus(xs..., 0); // foldr: plus(x0, plus(x1, plus(x2, 0)))
val total_l = plus(0, xs...); // foldl: plus(plus(plus(0, x0), x1), x2)
```

Parser/typechecker rules:

- `...expr` keeps its existing unpack/spread meaning.
- `expr...` is a postfix fold expression only in value-expression contexts.
- `predicate(expr)?...` is the only filter marker form in this phase; `?` has
  no general expression meaning outside the fixed `?...` token sequence.
- Map and filter expressions must contain exactly one driver sequence
  expression in this phase. Multiple driver sequences are rejected instead of
  implicitly zipping.
- `seq...` in the first call-argument position with an initializer after it is
  a right fold.
- `seq...` in the last call-argument position with an initializer before it is
  a left fold.
- `seq...` in a middle argument position is rejected in this phase.
- A call expression may contain at most one postfix fold pack.
- Fold operators are checked as binary functions. For foldr, the function must
  accept `(element, accumulator)`; for foldl, it must accept
  `(accumulator, element)`.

Future plan:

- Add zip/map2 syntax for multiple driver sequences after the single-sequence
  rules are stable.
- Add n-ary fold only after the type checker can model multi-argument
  accumulator transitions explicitly.
- Decide whether `?` should gain any standalone expression meaning. Until then,
  only `?...` is reserved for filter-fold syntax.

## Design Questions

- Ranges with both bounds are first-class `Range<T>` values.
- Open ranges are index-only.
- `a..b` is end-exclusive and `a..=b` is end-inclusive.
- Descending ranges use the same `Range<T>` runtime representation; explicit
  step remains future work.
- Pipeline syntax is general call sugar, not collection-only sugar.
- Fold expressions use postfix `...`; `|>` never triggers map/filter/fold
  semantics by itself.

## Acceptance Criteria

- `range(0, 3)`, `slice(xs, 0, 2)`, and stdlib `fold(...)` helper calls are no
  longer exported compatibility APIs.
- `xs[1..3]` works for vectors/fixed arrays and returns `span<T>`.
- `[...(0..3)]` and `[...xs[1..3]]` materialize ranges/spans into vectors.
- `tuple[1..3]` works when bounds are compile-time constants and returns a tuple with preserved element types.
- `xs[^1]` indexes from the end when the collection has runtime length.
- Invalid tuple slice bounds fail during type checking.
- Fold/pipeline syntax has a separate parser/typechecker test suite before runtime lowering.
