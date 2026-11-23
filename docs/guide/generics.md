# Generics

Generic functions and types keep static safety across many concrete types.

## Generic functions

```ng
fun first<T>(values: array<T>) -> T {
    return values[0];
}
```

Type arguments are inferred from the arguments, or written explicitly:

```ng
let head = first([1, 2, 3]);
let typed = first<i64>([1, 2, 3]);
```

Each distinct concrete argument set produces one monomorphized instance:
the body is cloned, renumbered, and re-checked under concrete bindings, so
inner calls dispatch to the right impls. `--emit=ssa` shows the separate
instances — for `ident<T>` called with an `i64` and a `string` the IL
contains `$ident_0_2` and `$ident_1_3`.

## Generic types

```ng
struct Wrap<T> {
    value: T,
}

enum Chain<T> {
    Cons(head: T, tail: ref<Chain<T>>),
    Nil,
}
```

Generic structs and enums instantiate per concrete argument list
(`Wrap<i64>`, `Chain<string>`); recursive payloads go through
`ref<Self>`. Enum variants are named without type arguments:

```ng
let empty: Chain<i64> = Chain.Nil;
let first: Chain<i64> = Chain.Cons(1, ref empty);
```

A struct literal for a generic struct needs an annotated binding when the
argument cannot be inferred from context — `let boxed = Wrap { value: 1 };`
is rejected with *cannot infer generic arguments for struct literal `Wrap`;
annotate the binding type*.

## Where clauses

Where clauses constrain parameters with trait bounds, type tests,
const-predicate calls, and negation:

```ng
trait Show {
    fun show(self: Self ref) -> i64;
}

fun describe<T>(value: T ref) -> i64 where T: Show {
    return 32;
}

fun requireLarge<const N: i64>() -> i64 where is_large(N) {
    return 8;
}
```

`T is i64` is a *constraint*: it rejects instances whose argument is not
`i64`, but it does not narrow `T` inside the body. A body that returns
`value` still reports *return value type mismatch: expected i64, got T*, so
return a constant instead:

```ng
fun exact<T>(value: T) -> i64 where T is i64 {
    return 16;
}
```

Checks run per concrete instance; abstract calls inside generic bodies
defer to monomorphization.

## Const generics

Const parameters are declared `<const N: i64>` and are compared by value.
They are inferred from a fixed-array argument and can be passed explicitly:

```ng
fun take<const N: i64>(a: array<i64, N> ref) -> i64 {
    return 1;
}

let fixed: array<i64, 3> = [1, 2, 3];
let n = take(ref fixed);        // N inferred as 3
let m = requireLarge<42>();     // explicit const argument
```

A const parameter is not a runtime value and cannot drive a return type:
`fun makeFixed<const N: i64>() -> array<i64, N>` fails with *unresolved
const name `N`*, and reading `N` in the body fails with *const parameter
`N` is not a runtime value*.

## Generic impls

Trait impls can be generic over their target (see
[Traits](/guide/traits)):

```ng
impl<T> Show for List<T> { ... }
impl<T> Show for array<T> { ... }
```

Next: [References, Moves & Ownership](/guide/references-moves).
