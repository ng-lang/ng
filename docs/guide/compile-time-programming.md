# Compile-Time Programming

Compile-time selection, predicates, evaluation, and generics.

## `const if`

`const if` folds at compile time; only the selected branch is checked and
lowered:

```ng
const if (is_ref<ref<i64>>) { total := total + 2; }
```

Conditions may use const predicates, `const fun` calls, `T is Type`
tests, and const-capable natives.

A `const if` whose condition mentions a **type parameter** of the enclosing
generic function cannot be folded inside that generic body:

```ng
fun probe<T>(v: T) -> i64 {
    const if (is_ref<T>) { return 1; }   // rejected
    return 0;
}
```

```
type error E-type: cannot evaluate const declaration `is_ref` for abstract type parameter `T`
```

The condition is evaluated per **concrete** type argument, so `const if`
works when the type arguments are concrete — at a call site such as
`const if (is_ref<i64>)` or `const if (is_ref<ref<i64>>)`, and inside a
`const fun` applied to concrete arguments. `example/const_predicates.ng`
folds eight such concrete tests and returns their combined bitmask.

## Const declarations and pattern specialization

```ng
const is_ref<T>: bool = false;
const<T> is_ref<ref<T>>: bool = true;
```

Patterns specialize by exact match, then pattern/constructor match, then
the primary declaration; repeated parameters (`equal<T, T>`) refine the
rules:

```ng
const equal<T, U>: bool = false;
const<T> equal<T, T>: bool = true;
const is_i64<i64>: bool = true;
const<T> is_i64<T>: bool = false;
```

A declaration may be deleted for specific type arguments with `= delete`;
using such a specialization is an error:

```ng
const banned<i64>: bool = delete;
```

```
type error E-type: const declaration `banned` is deleted for these type arguments
```

Built-in const predicates include `is_trait<T>`, `is_abstract<T>`,
`is_tuple<T>`, `tuple_size<T>` (returns the arity, so compare it against a
number), and `sizeof_pack<T...>` (also returns a count):

```ng
const if (is_trait<Marker>) { total := total + 1; }
const if (is_abstract<AbstractHandle>) { total := total + 4; }
const if (is_tuple<tuple<i64, string>>) { total := total + 8; }
const if (tuple_size<tuple<i64, string, bool>> == 3) { total := total + 16; }
const if (sizeof_pack<i64, string, bool> == 3) { total := total + 32; }
```

`tuple_size<T>` applied to a non-tuple is rejected with
`tuple_size<T> expects a tuple type, got i64`.

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

That prints `folded`, and `runtimeResult` is `120` (`example/const_fun.ng`).

The const interpreter supports locals, `if`/`const if`, loops, `next`,
recursion, and calls to other const funs.

### Generic const funs

Type-parameterized const funs evaluate per concrete type argument, with
where clauses checked concretely:

```ng
const fun is_showable<T>() -> bool where T: RenderText { return true; }

const if (is_showable<i64>()) { print("showable"); }
```

(`example/generic_const_fun.ng`.) Calling one on a type that does not
implement the bound is an error:

```
type error E-type: call to `is_showable` does not satisfy its where clause
```

Generic const funs stay runtime-callable: `assert(identity("ok") == "ok")`.

### Const-capable native hosts

Pure stdlib natives (`length`, `trim`, `toUpper`, `regexMatch`, ...) evaluate at
compile time inside const contexts; impure natives (IO, GUI) are rejected:

```ng
const if (sanitized("  ng  ") == "NG") { print("trim folded"); }
```

```ng
const if (readLine() == "x") { print("y"); }
```

```
type error E-type: native `readLine` is not const-capable
```

## Const generics

```ng
const fun is_large(value: i64) -> bool { return value > 10; }

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
