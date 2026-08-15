# Type System in Depth

Type kinds, inference, numeric rules, and opaque types.

## Type kinds

Types are interned canonical `TypeId`s over descriptors:

- **Builtins** — `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `bool`, `string`,
  `unit`
- **Arrays** — dynamic `array<T>`, fixed `array<T, N>` (N is a const
  parameter), dependent arrays in const-generic signatures
- **Tuples** — heterogeneous products
- **Structs / enums** — nominal types, per-instantiation for generics
- **References** — `T ref`, `T ref mut` (non-returnable views)
- **Raw pointers** — `T *const` / `T *mut` behind the unsafe boundary
- **Ranges** — `range<T>`
- **Unions** — `A | B` annotations
- **Trait references** — `ref<Trait>` dynamic views
- **Type parameters / constructors / applications** — generics and HKT
- **Opaque** — `type X = native;` handles and `type X;` abstract types

## Inference

- Bindings infer from initializers; annotations win and constrain the
  initializer (`inferExpected`).
- Integer literals keep their text until a contextual type selects them,
  with per-width range checks; float literals adopt `f32`/`f64`.
- Generic call arguments unify against parameter types; explicit
  arguments (`name<types>(...)`) bind in declaration order.
- Overload resolution is specificity-ordered (guarded against recursive
  types).

## Numeric rules

- Same-type arithmetic only — no implicit conversions.
- Equality/ordering compare across numeric widths (and integer/float
  pairs) at runtime.
- Runtime arithmetic is checked per static width (overflow diagnostics);
  const evaluation checks the same way.

## Nominal vs structural

Structs and enums are nominal (`Point` ≠ any other struct); tuples and
arrays are structural. `type X = T;` aliases share the underlying type's
identity; `newtype`-style wrappers use structs.

## Unions

`A | B` accepts member values; the value keeps its member type at
runtime. Member-typed construction, equality/ordering narrowing, and
parameter flow are checked; no tag is stored.

## Opaque and native types

```ng
type Connection;             // abstract (no construction)
type NativeHandle = native;  // embedding-supplied handle
```

Built-in predicates classify them: `is_abstract<T>`, `is_trait<T>`.
Native handles are `Value`-level opaque tokens passed to `native fun`
hosts (the `memory` module's handles, imgui's binding).

## References and trait views

`ref<T>` is a scoped view, never a first-class value in aggregates or
returns. `ref<Trait>` erases the concrete type but keeps the dispatch
table; its method set is the trait's declaration order.

Next: [Compile-Time Programming](/guide/compile-time-programming).
