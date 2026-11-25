# Advanced Generics

Higher-kinded constructors, variadic packs, folds, and tuple
introspection.

## Higher-kinded constructors (`F<_>`)

Type-constructor parameters of kind `* -> *` apply as `F<T>` in parameter
and return types:

```ng
struct Holder<T> {
    value: T,
}

fun accept<F<_>, T>(value: F<T> ref) -> i64 {
    return 7;
}

let boxed: Holder<i64> = Holder { value: 1 };
accept(ref boxed);              // F and T inferred
accept<Holder, i64>(ref boxed); // explicit, declaration order
```

Explicit arguments bind in declaration order and the count must match, so a
declaration with one constructor parameter plus one type parameter takes
two explicit arguments. A declaration with only the constructor parameter,
`fun accept<F<_>>(value: F<i64> ref) -> i64`, needs only one:
`accept<Holder>(ref boxed)`; `accept<Holder, i64>` is rejected with
*generic argument count mismatch: expected 1, got 2*.

The binding for a generic struct literal needs its type annotated
(`let boxed: Holder<i64> = ...`), otherwise the checker reports *cannot
infer generic arguments for struct literal `Holder`*.

Variadic constructor kinds (`F<_, ...>`) support parameterized opaque
templates (`type Variadic<Head, Tail...> = native;`), instantiated per
argument list:

```ng
type Variadic<Head, Tail...> = native;

fun acceptVariadicConstructor<F<_, ...>>() -> i64 {
    return 1;
}

let n = acceptVariadicConstructor<Variadic>();
```

## Variadic type packs

Heterogeneous packs flow through parameters, returns, and tuple literals:

```ng
fun gather<T...>(args: T...) -> tuple<T...> {
    return (args...,);
}

let packed: tuple<i64, string, bool> = gather(1, "two", true);
```

`(...args,)` is the same gather spelled with a leading spread. Call-site
tuple spreads flatten statically:

```ng
fun pickMiddle(left: i64, value: string, ok: bool) -> string {
    return value;
}

let spread = pickMiddle(...(7, "spread", true));
```

`sizeof_pack` takes the concrete argument types, not a pack name:

```ng
const if (sizeof_pack<i64, string, bool> == 3) { ... }
```

`sizeof_pack<T...>` inside a generic body is rejected — *unknown type `T`*.

## Tuple introspection

Tuple predicates take the `tuple<...>` wrapper; a bare parenthesized type
list `(i64, string)` is a parse error (*expected a const declaration
name*):

```ng
const if (is_tuple<tuple<i64, string>>) { ... }
const if (tuple_size<tuple<i64, string, bool>> == 3) { ... }
let first: tuple_element<tuple<i64, string, bool>, 0> = 7;
let joined: tuple_concat<tuple<i64, string>, tuple<bool, i64>> = (1, "x", true, 9);
```

## Folds

A fold calls a two-argument function once per element, threading the
accumulator through whichever parameter the seed occupies. The seed's
position in the call is what decides the shape, not the spread's:

```ng
let a = addAll([1, 2, 3]..., 0);   // seed last  -> f(x..., acc)
let b = addAll(0, [1, 2, 3]...);   // seed first -> f(acc, x...)
```

With `fun addAll(x: i64, acc: i64) -> i64` both spellings above are the
`f(x, acc)` direction; `addAll(1, [1, 2, 3]...)` yields `6` because every
element lands in the first parameter. To fold with the accumulator first,
declare the parameters that way (`fun addAll(acc: i64, x: i64)`).

Folds work over arrays, slices, and ranges, lowered to runtime loops:

```ng
let fromArray = foldRightStep(xs..., 0);
let fromRange = foldRightStep((0..4)..., 0);
let fromSpan  = foldLeftStep(0, xs[0..2]...);
```

## Map comprehensions

Array literals apply a function per element of an array or range, with an
optional `?` filter marker; leading and trailing elements mix freely:

```ng
fun inc(value: i64) -> i64 => value + 1;
fun even(value: i64) -> bool => (value % 2) == 0;

let doubled = [inc(1..4)...];      // [2, 3, 4]
let evens = [even(1..8)?...];      // [2, 4, 6, 8]
let mixed = [0, inc(1..4)..., 9];  // [0, 2, 3, 4, 9]
```

Next: [Standard Library](/guide/standard-library).
