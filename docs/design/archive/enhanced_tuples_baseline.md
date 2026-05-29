# Enhanced Tuple Types

## Order

Recommended Issue order: 6.

## Goal

Define the next tuple milestone after the currently implemented value-level tuple support and enhanced generics.

The core goal is to make tuple shape available to the type checker and generic specialization system without adding runtime generic metadata to ORGASM. Tuple values should remain StorageCell-backed runtime values; tuple type operations should be resolved before ORGASM bytecode emission.

## Motivation

Current NG supports tuple values, tuple type annotations, tuple destructuring, tuple spread construction, and variadic generic packs. That is enough for common multiple-return and variadic function usage.

The remaining gap is type-level tuple manipulation:

```ng
type Args = (i32, string, bool);

const arity: u32 = tuple_size<Args>;
type First = tuple_element<Args, 0>;

fun consume(args: Args) -> unit {
  call_with_tuple(target, args);
}
```

The examples above should be resolved by the type checker and monomorphizer. ORGASM should receive fully lowered calls and concrete tuple layouts.

## Dependencies

Prerequisites:

- [Generalized `= delete` Declarations](archive/generalized_delete.md), for invalid projection fallbacks.
- [Constant Generic Parameters](archive/constant_generic_parameters.md), for tuple element indexes and compile-time bounds.
- [`const fun` And Compile-Time Computation](archive/const_fun.md), for the complete recursive/type-level form. Early intrinsic-only phases can start before full `const fun`.

Related:

- [Ranges, Slicing, Fold, And Pipeline Syntax](../ranges_slicing_pipeline.md), for tuple slice syntax.

## Current Baseline

Already implemented:

- Tuple literal: `(a, b, c)`.
- Tuple annotation: `(A, B, C)`.
- Value-level `tuple.size`.
- Value-level `tuple.N` and `tuple[N]`.
- Value-level assignment through tuple slots.
- Tuple and array spread inside tuple/array construction.
- Tuple and array destructuring with trailing rest bindings.
- Generic packs: `fun f<T...>(args: T...)`.
- Variadic HKT placeholders: `F<_, ...>`.
- Type alias specialization and const predicate specialization.

Still open:

- Tuple type slicing with range syntax.
- Recursive `tuple_element` implementation in `std.tuple` after const expressions in type arguments are available.
- First-class function type construction from tuple types.

## Design Rules

### Tuple Type Model

Tuple types remain structural type info:

```ng
(T0, T1, T2)
```

The canonical type identity is ordered and arity-sensitive:

```text
(i32, string) != (string, i32)
(i32) != i32
```

`unit` remains its own primitive type. Zero-length tuple syntax is not part of NG today; if it is ever introduced, `()` must be explicitly distinct from `unit`.

### `std.tuple` Compile-Time Predicates And Projections

Expose tuple type-level APIs from `std.tuple`, and import that module from `std.prelude`.
The compiler should provide only the minimum support needed by the standard-library
declarations: empty pack specialization, pack type arguments, tuple type patterns,
and tuple element projection.

```ng
const<> sizeof_pack<>: u32 = 0;
const<T, U...> sizeof_pack<T, U...>: u32 = 1 + sizeof_pack<U...>;

const is_tuple<T>: bool = false;
const<T...> is_tuple<(T...)>: bool = true;

const tuple_size<T>: u32 = delete;
const<T...> tuple_size<(T...)>: u32 = sizeof_pack<T...>;

type<T, const I: u32> tuple_element<T, I>;
```

`sizeof_pack`, `is_tuple`, and `tuple_size` are ordinary const specializations.
`tuple_element<T, I>` is currently a standard-library type API resolved by the
type checker because NG does not yet support const expressions such as `I - 1`
inside type argument lists. Once that lands, it can be lowered to recursive
standard-library specializations:

```ng
type<T, const I: u32> tuple_element<T, I> = delete;
type<T0, T...> tuple_element<(T0, T...), 0> = T0;
type<T0, T..., const I: u32> tuple_element<(T0, T...), I>
  : where I > 0 = tuple_element<(T...), I - 1>;
```

### Pack And Tuple Normalization

Generic packs and tuple types should interoperate through explicit normalization:

```ng
type<T...> pack_tuple<T...> = (T...);
type<T> tuple_pack<T> where is_tuple<T> = T...; // conceptual only
```

Implementation rule:

- A parameter pack can be materialized as a tuple type in return positions.
- A tuple type can satisfy a pack expansion only where the syntax explicitly asks for spread.
- No implicit flattening is allowed in ordinary generic matching.

Example:

```ng
fun collect<T...>(args: T...) -> (T...) {
  return args;
}
```

### Function Application From Tuple

General spread calls should be specified independently from pack calls:

```ng
fun consume(a: i32, b: string, c: bool) -> unit;

val args: (i32, string, bool) = (1, "x", true);
consume(...args);
```

Type checking rule:

- If a spread argument appears in a call argument list, the type checker expands its tuple or varargs element types into the call's argument vector.
- For fixed-arity functions, expanded arity must match required parameters after accounting for defaults.
- For pack functions, expanded elements are appended to the pack collection.
- Spread of arrays into calls is not allowed for fixed-arity type checking unless the callee is a pack function with a homogeneous array-compatible pack rule.

ORGASM rule:

- The compiler emits each expanded tuple slot in call order before `CALL`.
- If the tuple expression is not statically known as a tuple or varargs value, compilation must fail before bytecode emission.
- Method calls and trait-qualified calls use the same expansion after receiver insertion.

### Tuple Slicing

Tuple slicing should be type preserving:

```ng
val t: (i32, string, bool, f64) = ...;
val mid = t[1..3]; // (string, bool)
```

This depends on [Ranges, Slicing, Fold, And Pipeline Syntax](../ranges_slicing_pipeline.md). Enhanced tuple only defines the type rule:

- Slice bounds must be compile-time constants for tuple values.
- Result type is a tuple of the selected element types.
- From-end indexes are allowed only after the range design defines them.

### Tuple Concatenation

Tuple type concatenation is useful for spread construction and generic APIs:

```ng
type<A, B> tuple_concat<A, B> = delete;
type<A..., B...> tuple_concat<(A...), (B...)> = (A..., B...);
```

Value-level tuple spread already behaves like tuple concatenation. The type checker should expose this operation for signatures and specialization.

## Phased Implementation Plan

### Phase 1: Type Predicates And Projections

- Implemented: `std.tuple` with `sizeof_pack`, `is_tuple`, `tuple_size`, and `tuple_element`.
- Implemented: parser support for empty generic argument lists and `T...` in type arguments/tuple patterns.
- Implemented: typechecker support for tuple pattern matching and pack-tail const specialization.
- Implemented: `std.prelude` imports `std.tuple`, so entrypoint type checking sees tuple APIs by default.
- Remaining: move `tuple_element` from compiler-resolved projection to recursive stdlib specializations after const expressions in type arguments are supported.

### Phase 2: Pack-To-Tuple Type Syntax

- Implemented: `(T...)` in type annotations where `T...` is a generic pack.
- Implemented: `fun f<T...>(args: T...) -> (T...)`.
- Implemented: deterministic mangling for tuple-expanded pack types.
- Implemented: STUPID and ORGASM examples.

### Phase 3: General Spread Call Application

- Implemented: spread arguments expand in the type checker for fixed-arity calls.
- Implemented: ORGASM lowering for statically tuple-typed spread call arguments, including tuple literals, tuple-valued locals/globals, and tuple-return expressions whose type can be inferred before bytecode emission.
- Implemented: generic functions, methods, trait-qualified calls, imported calls, native calls, and `print` use the same spread-argument path.
- Remaining: general sequence spread into calls from non-tuple runtime values stays rejected in ORGASM lowering.

### Phase 4: Tuple Type Concatenation And Rest Types

- Implemented: `tuple_concat`.
- Implemented: pack-to-tuple result expansion through tuple-return annotations.
- Implemented: `val (head, ...tail)` and `(head, ...tail)` share the same type computation path.
- Remaining: richer tuple-rest slicing beyond unpack/rest binding.

### Phase 5: Tuple Slicing With Range Design

- Integrate with [Ranges, Slicing, Fold, And Pipeline Syntax](../ranges_slicing_pipeline.md).
- Require compile-time bounds for tuple slicing.
- Preserve tuple element types across slices.
- Add runtime lowering through slot reads and tuple construction.

## Acceptance Criteria

- `is_tuple<(i32, string)>` is `true`; `is_tuple<i32>` is `false`.
- `tuple_size<(i32, string, bool)>` evaluates to `3`.
- `tuple_element<(i32, string), 1>` resolves to `string`.
- `fun collect<T...>(args: T...) -> (T...)` type checks and runs.
- `consume(...(1, "x", true))` type checks and runs for a fixed-arity function.
- Spread calls work for generic functions, methods, trait-qualified calls, imported calls, native calls, and `print`.
- Invalid tuple element indexes fail during type checking.
- Tuple type mangling is stable and collision-free with existing generic mangling.

## Out Of Scope

- Runtime reflection over tuple types.
- Carrying unresolved generic tuple metadata in ORGASM.
- Fold/map/filter syntax.
- Pipeline syntax.
- Open-ended runtime tuple slicing without compile-time bounds.
