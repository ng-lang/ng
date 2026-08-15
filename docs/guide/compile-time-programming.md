# Compile-Time Programming

Compile-time selection, predicates, evaluation, and generics.

## `const if`

`const if` folds at compile time; only the selected branch is checked and
lowered:

```ng
const if (is_ref<T>) {
    return 1;
} else {
    return 0;
}
```

Conditions may use const predicates, `const fun` calls, `T is Type`
tests, and const-capable natives. Inside generic functions, per-instance
`const if` evaluates against each monomorphized instance's bindings.

## Const declarations and pattern specialization

```ng
const is_ref<T>: bool = false;
const<T> is_ref<ref<T>>: bool = true;
```

Patterns specialize by exact match, then pattern/constructor match, then
the primary declaration; repeated parameters (`equal<T, T>`) and
`= delete` (negative declarations) refine the rules:

```ng
const equal<T, U>: bool = false;
const<T> equal<T, T>: bool = true;
const is_i64<i64>: bool = true;
const<T> is_i64<T>: bool = false;
```

Built-in const predicates include `is_trait<T>`, `is_abstract<T>`,
`is_tuple<T>`, `tuple_size<T>`, `sizeof_pack<T...>`, and tuple element /
concat constructors.

## `const fun`

`const fun` bodies are compile-time capable and runtime callable:

```ng
const fun fact(value: i64) -> i64 {
    if (value == 0) { return 1; }
    return value * fact(value - 1);
}

const if (fact(4) == 24) { print("folded"); }
let runtimeResult = fact(5);
```

The typed-HIR const interpreter supports locals, `if`/`const if`, loops,
`next`, recursion, tail recursion, and calls to other const funs, with
fuel and depth budgets.

### Generic const funs

Type-parameterized const funs evaluate per concrete type argument, with
where clauses checked concretely:

```ng
const fun is_showable<T>() -> bool where T: Show { return true; }

const if (is_showable<i64>()) { print("showable"); }
```

Inferred arguments reuse the runtime-instantiated body; abstract calls
inside generic bodies defer to the instance.

### Const-capable native hosts

Pure stdlib natives (`length`, `trim`, `regexMatch`, ...) evaluate at
compile time inside const contexts; impure natives (IO, GUI) fail with
`native `X` is not const-capable`.

## Const generics

```ng
fun requireLarge<const N: i64>() -> unit where is_large(N) { }
requireLarge<42>();
```

Const parameters flow into where clauses, fixed-array sizes, and
per-instance identity.

## Where clauses

```ng
fun describe<T>(value: T ref) -> i64 where T: Show, T is i64 { ... }
```

Supported forms: trait bounds (`T: Trait`), direct type tests (`T is
Type`), negation and `&&`, const-fun calls over const parameters, and
const predicates. Checks run per concrete instance.

Next: [Advanced Generics](/guide/advanced-generics).
