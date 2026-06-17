# Advanced Generics

This chapter covers NG's more advanced generic programming features: higher-kinded types, type specialization, enhanced tuples, fold expressions, and the `delete` mechanism.

## Higher-Kinded Generics (HKT)

Higher-kinded generics allow abstraction over **type constructors** — types that take type parameters.

### Motivation

Without HKT, you need separate functions for `Box<i32>`, `Option<string>`, etc. With HKT, you can write one function over any container type.

### HKT Parameters

Use `F<_>` syntax to declare a type constructor parameter:

```ng
type Box<T> {
    value: T;
}

fun accept<F<_>, T>(value: ref<F<T>>) -> unit {
    print("accepted higher-kinded value");
}

val box = new Box<i32> { value: 42 };
accept<Box, i32>(box);   // explicit
accept(box);             // inferred
```

### HKT Kind Parameters

The `_` in `F<_>` indicates a type constructor expecting exactly one type argument. NG supports:

| Syntax | Description |
|---|---|
| `F<_>` | Unary type constructor (e.g., `Box<T>`, `Option<T>`) |
| `F<_, _>` | Binary type constructor (e.g., `Result<T, E>`) |
| `F<..._>` | Variadic kind placeholder |

### Variadic HKT

```ng
fun variadic<F<..._>, T...>(value: ref<F<T...>>) -> unit {
    print("variadic HKT");
}
```

### HKT Trait Parameters

```ng
trait Mappable<F<_>> {
    fun map<T, U>(self: ref<F<T>>, f: (T) -> U) -> F<U>;
}
```

### HKT Constraints

```ng
fun process<F<_>, T>(container: ref<F<T>>) where Mappable<F> {
    // container guaranteed to have a map method
}
```

## Type Specialization

Type specialization allows providing different implementations based on concrete type patterns.

### Partial Specialization

```ng
// Base definition
type Wrapper<T>;

// Specialization for i32
type Wrapper<i32> {
    value: i32;
    fun double(self: ref<Self>) -> i32 => self.value * 2;
}

// Specialization for string
type Wrapper<string> {
    value: string;
    fun shout(self: ref<Self>) -> string => self.value + "!!!";
}
```

### Where Predicates in Specialization

```ng
type Handler<T>;

type Handler<T> where is_numeric<T>() {
    fun handle(self: ref<Self>, value: T) => print("numeric:", value);
}

type Handler<string> {
    fun handle(self: ref<Self>, value: string) => print("string:", value);
}
```

### Most Specific Pattern Matching

The type checker selects the **most specific** matching pattern. If multiple patterns match, the most specific one wins:

```ng
// Generic
fun describe<T>(value: T) -> string => "unknown";

// More specific: references
fun describe<T>(value: ref<T>) -> string => "reference";

// Most specific: string
fun describe(value: string) -> string => "exact string";
```

### Abstract Type Aliases

Declare a type alias without a body — implementations must specialize it:

```ng
type ResultType<T>;

type ResultType<i32> { value: i32; }
type ResultType<string> { value: string; }
```

## Enhanced Tuple Types

NG's tuple system includes type-level operations.

### Tuple Type Predicates

```ng
import std.tuple;

const if (is_tuple<tuple>) {
    print("tuple type recognized");
}

const if (tuple_size<tuple> == 3) {
    print("tuple has 3 elements");
}
```

### Tuple Element Projection

```ng
import std.tuple;

val t = (1, "hello", true);
type Second = tuple_element<typeof(t), 1>;
// Second is string
```

### Tuple Concatenation

```ng
import std.tuple;

type Extended = tuple_concat<(i32, string), (bool, f64)>;
// Extended is (i32, string, bool, f64)
```

### Spread Expansion

Enhanced tuples support spread expansion in function calls:

```ng
fun sum(a: i32, b: i32, c: i32) -> i32 => a + b + c;

val args = (1, 2, 3);
val result = sum(...args);   // expands to sum(1, 2, 3)
```

### Pack-to-Tuple Return

Parameter packs can be returned as tuples:

```ng
fun packToTuple<T...>(args: T...) -> (T...) => args;
```

## Fold Expressions

Fold expressions allow reducing a parameter pack with a binary operator:

```ng
fun sumAll<T...>(args: T...) -> i32 {
    return (... + args);  // fold left: ((a + b) + c) + ...
}

print(sumAll(1, 2, 3, 4));  // 10
```

### Supported Fold Operators

| Operator | Meaning |
|---|---|
| `... + args` | Sum |
| `... * args` | Product |
| `... && args` | Logical AND |
| `... \|\| args` | Logical OR |

## Delete Mechanism

The `delete` keyword removes specific generic instantiations, preventing certain types from being used.

### Deleted Functions

```ng
// Accept all types
fun process<T>(value: T) -> string => "default";

// Delete string instantiation
delete fun process<string>(value: string);
```

### Deleted Type Aliases

```ng
type Container<T>;

delete type Container<ref<T>>;  // Reject references
```

### Priority Rule

More specific deletions take priority over less specific ones. A deleted specialization can coexist with a valid fallback:

```ng
fun handle<T>(value: T) -> string => "fallback";

// More specific deletion beats fallback
delete fun handle<ref<T>>(value: ref<T>);

handle(42);          // "fallback" (OK)
// handle(ref 42);   // COMPILE ERROR: deleted
```

## What's Next?

Continue to [Standard Library](standard-library.md) to explore NG's built-in library.

> **Try it:** `example/48.higher_kinded_generics.ng` — HKT basics
> **Try it:** `example/49.variadic_hkt_kind.ng` — Variadic HKT
> **Try it:** `example/44.type_specialization.ng` — Type specialization
> **Try it:** `example/43.const_specialization.ng` — Const specialization
> **Try it:** `example/54.enhanced_tuple_types.ng` — Enhanced tuples
> **Try it:** `example/58.fold_expressions.ng` — Fold expressions