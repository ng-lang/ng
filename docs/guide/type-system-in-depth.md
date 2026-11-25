# Type System in Depth

Type kinds, inference, numeric rules, and opaque types.

## Type kinds

Types are interned canonical `TypeId`s over descriptors:

- **Builtins** — `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `bool`, `string`,
  `unit`
- **Arrays** — dynamic `array<T>`, fixed `array<T, N>` (N is a const
  expression)
- **Tuples** — heterogeneous products, written `tuple<A, B>` in type
  position
- **Structs / enums** — nominal types, per-instantiation for generics
- **References** — `T ref`, `T ref mut` (non-returnable views)
- **Ranges** — `range<T>`
- **Unions** — `A | B` annotations
- **Trait references** — `ref<Trait>` dynamic views
- **Type parameters / constructors / applications** — generics and HKT
- **Opaque** — `type X = native;` handles and `type X;` abstract types

## Inference

- Bindings infer from initializers; annotations win and constrain the
  initializer (`inferExpected`).
- Integer literals keep their text until a contextual type selects them,
  with per-width range checks (`let a: i8 = 200;` reports *integer
  literal `200` is out of range for type i8*); float literals adopt
  `f32`/`f64`.
- Generic call arguments unify against parameter types; explicit
  arguments (`name<types>(...)`) bind in declaration order and must match
  the parameter count.
- Overload resolution is specificity-ordered (guarded against recursive
  types).

## Numeric rules

- Same-type arithmetic only — no implicit conversions: adding an `i32`
  to an `i64` reports *binary operands type mismatch: expected i32, got
  i64*.
- Equality/ordering compare across numeric widths (and integer/float
  pairs) at runtime, so `i32` and `i64` operands compare directly.
- Runtime arithmetic is checked per static width (overflow diagnostics);
  const evaluation checks the same way.

## Nominal vs structural

Structs and enums are nominal: a struct value is not accepted where a
different struct type with identical fields is expected (*call argument 1
type mismatch: expected Point, got Other*). Tuples and arrays are
structural. Wrapping a builtin in a struct gives a distinct type; there
is no `newtype` beyond a single-field struct.

A general alias is not a declaration form — `type MyInt = i64;` is
rejected with *expected `native` after `=` in opaque type declaration*.
Only opaque handles (`type X = native;`) and abstract types
(`type X;`) are declared with `type`.

## Unions

`A | B` accepts member values; the value keeps its member type at
runtime. Member-typed construction, equality/ordering narrowing, and
parameter flow are checked; no tag is stored:

```ng
let x: i64 | string = 1;
let y: i64 | string = "hello";
let z: bool | i64 | f64 = true;
```

Switch matches the active member with type patterns (`case (n: i64)`),
member literals (`case 42`), and `otherwise` for the remaining members.

## Opaque and native types

```ng
type Connection;             // abstract (no construction)
type NativeHandle = native;  // embedding-supplied handle
```

Built-in predicates classify them: `is_abstract<T>`, `is_trait<T>`.
Abstract types satisfy `is_abstract`; native handles do not. Native
handles are `Value`-level opaque tokens passed to `native fun`
hosts (the `memory` module's handles, imgui's binding).

## References and trait views

`ref<T>` is a scoped view. It can be stored in aggregates — including
`array<ref<T>>` — but not returned from a function unless the return
type declares an origin set. `ref<Trait>` erases the concrete type and
keeps the dispatch table:

```ng
let view: ref<Printer> = counter;          // view over a value
let views: array<ref<Printer>> = [counter, number];
views[0].show();                            // dynamic dispatch
```

Next: [Compile-Time Programming](/guide/compile-time-programming).
