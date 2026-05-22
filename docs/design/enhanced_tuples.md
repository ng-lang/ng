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

- [Generalized `= delete` Declarations](generalized_delete.md), for invalid projection fallbacks.
- [Constant Generic Parameters](constant_generic_parameters.md), for tuple element indexes and compile-time bounds.
- [`const fun` And Compile-Time Computation](const_fun.md), for the complete recursive/type-level form. Early intrinsic-only phases can start before full `const fun`.

Related:

- [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md), for tuple slice syntax.

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

Not yet complete:

- Tuple type arity as a compile-time constant.
- Tuple type element projection.
- Tuple type slicing and concatenation.
- General spread call application for all callable forms.
- Tuple shape constraints in `where` clauses.
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
() is not unit
```

`unit` remains its own primitive type. If zero-length tuple syntax is ever introduced, it must be explicitly distinct from `unit`; this design does not require adding `()`.

### Built-In Compile-Time Predicates And Projections

Add built-in compile-time constants and type aliases, using the same specialization machinery as enhanced generics:

```ng
const<T> is_tuple<T>: bool = false;
const<T...> is_tuple<(T...)>: bool = true;

const<T...> tuple_size<(T...)>: u32 = sizeof_pack<T...>;

type<T, const I: u32> tuple_element<T, I> = delete;
type<T0, T...> tuple_element<(T0, T...), 0> = T0;
type<T0, T...> tuple_element<(T0, T...), I> where I > 0 = tuple_element<(T...), I - 1>;
```

The syntax above is directional. It depends on const generic arithmetic and `const fun` for the recursive `I - 1` form. Before const arithmetic lands, the compiler may provide `tuple_element` as an intrinsic.

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

This depends on [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md). Enhanced tuple only defines the type rule:

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

### Phase 1: Type Predicates And Intrinsics

- Add `is_tuple<T>`.
- Add intrinsic `tuple_size<T>` for tuple types.
- Add intrinsic `tuple_element<T, I>`.
- Reject non-tuple input with clear diagnostics.
- Add parser/typechecker tests for nested tuples and invalid indexes.

### Phase 2: Pack-To-Tuple Type Syntax

- Support `(T...)` in type annotations where `T...` is a generic pack.
- Type check `fun f<T...>(args: T...) -> (T...)`.
- Ensure mangling encodes tuple-expanded pack types deterministically.
- Add STUPID and ORGASM examples.

### Phase 3: General Spread Call Application

- Expand spread arguments in the type checker for fixed-arity calls.
- Lower expanded tuple slots in ORGASM.
- Cover default parameters, generic functions, methods, trait-qualified calls, and native calls.
- Reject ambiguous array spread into fixed-arity calls.

### Phase 4: Tuple Type Concatenation And Rest Types

- Add `tuple_concat`.
- Expose rest binding result type through tuple slice logic.
- Ensure `val (head, ...tail)` and `(head, ...tail)` share the same type computation path.

### Phase 5: Tuple Slicing With Range Design

- Integrate with [Ranges, Slicing, Fold, And Pipeline Syntax](ranges_slicing_pipeline.md).
- Require compile-time bounds for tuple slicing.
- Preserve tuple element types across slices.
- Add runtime lowering through slot reads and tuple construction.

## Acceptance Criteria

- `is_tuple<(i32, string)>` is `true`; `is_tuple<i32>` is `false`.
- `tuple_size<(i32, string, bool)>` evaluates to `3`.
- `tuple_element<(i32, string), 1>` resolves to `string`.
- `fun collect<T...>(args: T...) -> (T...)` type checks and runs.
- `consume(...(1, "x", true))` type checks and runs for a fixed-arity function.
- Spread calls work for generic functions, methods, and trait-qualified calls.
- Invalid tuple element indexes fail during type checking.
- Tuple type mangling is stable and collision-free with existing generic mangling.

## Out Of Scope

- Runtime reflection over tuple types.
- Carrying unresolved generic tuple metadata in ORGASM.
- Fold/map/filter syntax.
- Pipeline syntax.
- Open-ended runtime tuple slicing without compile-time bounds.
