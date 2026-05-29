# Tuple Design

Original discussion: <https://github.com/ng-lang/ng/discussions/21>

This document records the current tuple design after the StorageCell runtime, ORGASM compiler, and enhanced generics work. It supersedes the older staged checklist in the discussion body while keeping the same motivation: tuples are heterogeneous positional product values used for multiple returns, multiple bindings, spread/unpack, and variadic generic packs.

## Current Semantics

A tuple is a value with fixed arity and per-slot element types.

```ng
val x = (1, "a", false);
val y: (i32, string, bool) = x;
val (a, b, c) = y;
```

Runtime representation:

- Tuples are StorageCell-backed runtime values with slot-backed elements.
- Tuple elements are addressed by zero-based numeric members and indexes.
- Tuple values support copying, equality, display, GC tracing, and slot mutation through the same runtime value access layer as arrays and structural objects.
- `unit` remains the unit value/type. It is not treated as a zero-length tuple.

Supported value operations:

```ng
val tup = (1, false, "hello");

assert(tup.size == 3);
assert(tup.0 == 1);
assert(tup[1] == false);

tup.1 := true;
assert(tup[1] == true);
```

Supported unpacking and spread construction:

```ng
val x = (1, false, "hello");
val y = (0, ...x, 4);

val (head, ...tail) = y;
assert(head == 0);
assert(tail.size == 4);

val arr = [1, 2, 3];
val [first, ...rest] = arr;
val arr2 = [0, ...arr, 4];
```

Supported variadic generic packs:

```ng
fun count<T...>(args: T...) -> u32 {
  return args.size;
}

assert(count(1, "two", true) == 3);
```

Pack parameters are materialized as tuple-like varargs values in type checking and as runtime tuples when invoked. `next ...tail` is supported for recursive pack processing.

## Discussion #21 Status

The linked discussion proposed four broad areas. Their current status is:

- Tuple literals and tuple type annotations are implemented.
- Tuple `.size`, numeric member access, index access, and element assignment are implemented for runtime values.
- Tuple and array spread construction are implemented.
- Tuple and array destructuring with optional trailing rest binding are implemented.
- Multiple returns are represented as returning tuple values.
- Generic parameter packs are implemented and cover the main variadic function use case better than the original native-only variadic proposal.
- Prelude `print` and `assert` are backed by variadic native handling; user-defined `fun<T...>(args: T...)` is supported.
- Range expressions (`a..b`, `a..=b`), from-end indexing (`^n`), and slice syntax
  are implemented. The old `range(...)` and `slice(...)` helper APIs have been
  removed.
- Tuple type-level operations such as `<tuple>.size` as a compile-time type expression and `<tuple>[N]` as an element type projection are not implemented.
- Fold/map/filter postfix syntax and pipeline syntax from the discussion are not implemented.

## Covered By Enhanced Generics

Enhanced generics already cover several capabilities that the original tuple proposal expected from "enhanced tuple type support":

- Heterogeneous variadic arguments are modeled with generic packs: `T...`.
- Pack parameters are type checked as `VarargsType` and can be destructured like tuples.
- Type specialization and const predicates can select behavior based on reified generic types.
- Variadic higher-kinded generic parameters can describe constructors with a variable type tail, e.g. `F<_, ...>`.
- ORGASM compilation monomorphizes generic instances instead of carrying unresolved generic tuple metadata at runtime.

These features reduce the need for tuple-specific metaprogramming in ordinary APIs. For example, a function that once needed `typeof(args).size` can usually be written as a pack function whose body uses `args.size` at value level.

Enhanced generics do not replace tuple type introspection. APIs that need compile-time arity, element type projection, tuple concatenation at type level, or function signatures derived from tuple types still need explicit enhanced tuple rules.

## Remaining Gaps

The remaining work should be split from the original broad tuple discussion into smaller designs:

- Enhanced tuple types: compile-time tuple arity, element projection, pack-to-tuple normalization, tuple concatenation, and function type interop.
- Range operators: `a..b`, `a..=b`, open ranges, and from-end indexes; see [ranges_slicing_pipeline.md](../ranges_slicing_pipeline.md).
- Tuple and array slicing syntax: `xs[a..b]`, `xs[^n]`, and tuple slice type preservation; see [enhanced_tuples.md](../enhanced_tuples.md) for tuple type rules.
- General function application via spread: make `f(...tuple)` consistently type checked and compiled for non-pack functions, default parameters, and trait/object method calls.
- Fold/pipeline syntax: should be redesigned separately over traits or prelude functions instead of overloading tuple spread semantics further.

The follow-up enhanced tuple plan is in [enhanced_tuples.md](../enhanced_tuples.md).

## Acceptance Baseline

The current implementation is expected to keep these examples working in both STUPID and ORGASM unless explicitly noted by tests:

- `example/14.tuple.ng`
- Generic packs in `example/15.generics.ng`
- Tuple spread and rest binding in `test/typecheck/typecheck_tuple_test.cpp`
- Runtime tuple layout and indexed access tests in `test/runtime/runtime_value_ops_test.cpp`
- ORGASM tuple example coverage in `test/orgasm/examples_test.cpp`

Any future tuple work should extend this baseline rather than introduce a separate tuple runtime path.
