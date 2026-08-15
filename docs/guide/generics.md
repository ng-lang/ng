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
inner calls dispatch to the right impls.

## Generic types

```ng
struct Box<T> {
    value: T,
}

enum List<T> {
    Cons(head: T, tail: ref<List<T>>),
    Nil,
}
```

Generic structs and enums instantiate per concrete argument list
(`Box<i64>`, `List<string>`); recursive payloads go through
`ref<Self>`.

## Where clauses

Where clauses constrain parameters with trait bounds, type tests,
const-predicate calls, and negation:

```ng
fun describe<T>(value: T ref) -> i64 where T: Show {
    return 32;
}

fun exact<T>(value: T) -> i64 where T is i64 {
    return value;
}

fun requireLarge<const N: i64>() -> unit where is_large(N) { }
```

Checks run per concrete instance; abstract calls inside generic bodies
defer to monomorphization.

## Const generics

```ng
fun makeFixed<const N: i64>() -> array<i64, N> { ... }
```

Const parameters participate in instance identity (`array<T, N>` layouts,
const predicates), are compared by value, and can be passed explicitly
(`requireLarge<42>()`).

## Generic impls

Trait impls can be generic over their target (see
[Traits](/guide/traits)):

```ng
impl<T> Show for List<T> { ... }
impl<T> Show for array<T> { ... }
```

Next: [References, Moves & Ownership](/guide/references-moves).
