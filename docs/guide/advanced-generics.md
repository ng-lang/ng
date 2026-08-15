# Advanced Generics

Higher-kinded constructors, variadic packs, folds, and tuple
introspection.

## Higher-kinded constructors (`F<_>`)

Type-constructor parameters of kind `* -> *` apply as `F<T>` in parameter
and return types:

```ng
struct Box<T> {
    value: T,
}

fun accept<F<_>>(value: F<i64> ref) -> i64 {
    return 0;
}

let boxed = Box { value: 1 };
accept(ref boxed);            // F inferred as Box
accept<Box, i64>(ref boxed);  // explicit (declaration order)
```

Variadic constructor kinds (`F<_, ...>`) support parameterized opaque
templates (`type Variadic<Head, Tail...> = native;`), instantiated per
argument list.

## Variadic type packs

Heterogeneous packs flow through parameters, returns, and tuple literals:

```ng
fun gather<T...>(args: T...) -> tuple<T...> {
    return (args...,);
}

let pair = gather(1, "two", true);
```

Call-site tuple spreads flatten statically, and `sizeof_pack<T...>`
reports the pack length at compile time.

## Tuple introspection

```ng
const if (is_tuple<(i64, string)>) { ... }
const if (tuple_size<(i64, string, bool)> == 3) { ... }
let first: tuple_element<(i64, string, bool), 0> = 7;
let joined: tuple_concat<(i64, string), (bool, i64)> = (1, "x", true, 9);
```

## Folds

Folds call a two-argument function per element, threading the
accumulator:

```ng
let total = addAll([1, 2, 3]..., 0);   // left fold: f(acc, xs...)
let total = addAll(0, ...[1, 2, 3]);   // right fold: f(xs..., acc)
```

Folds work over arrays, slices, and ranges, lowered to runtime loops.

## Map comprehensions

Array literals apply a function per element of an array or range, with an
optional filter marker:

```ng
let doubled = [f(1..4)...];
let evens = [g(1..8)?...];
let mixed = [0, f(1..4)..., 9];
```

Next: [Standard Library](/guide/standard-library).
