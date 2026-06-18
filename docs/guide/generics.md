# Generics

Generics allow you to write code that works with multiple types while preserving static type safety.

## Generic Functions

### Basic Type Parameters

Place type parameters in angle brackets after the function name:

```ng
fun identity<T>(x: T) -> T {
    return x;
}

identity(42);        // T is inferred as i32
identity("hello");   // T is inferred as string
identity(true);      // T is inferred as bool
```

Type parameters are **inferred** from the arguments — you rarely need to specify them explicitly.

### Multiple Type Parameters

```ng
fun pair<A, B>(a: A, b: B) -> (A, B) {
    return (a, b);
}

val p = pair(1, "world");   // (i32, string)
```

### Type Constraints

You can constrain type parameters with a fixed set of allowed types:

```ng
fun add<T: i32 | f64>(a: T, b: T) -> T {
    return a + b;
}

add(1, 2);          // OK: i32
add(1.5, 2.5);      // OK: f64
// add("a", "b");   // ERROR: string not in constraint
```

## Parameter Packs

Parameter packs allow variable-arity generic functions. A pack parameter uses `...` suffix and receives a tuple:

```ng
fun count<T...>(args: T...) -> u32 {
    return args.size;
}

count(1, "two", 3.0, true);   // returns 4
```

### Destructuring Packs

```ng
fun printEach<T...>(args: T...) {
    if (args.size > 0) {
        val (head, ...tail) = args;
        print(head);
        next ...tail;    // recurse with remaining elements
    }
}

printEach(1, "hello", true);
```

### Constraints on Packs

```ng
fun sum<T: i32 | f64...>(args: T...) -> T {
    // Only works for numeric types
}
```

## Generic Types

### Generic Object Types

Types can be parameterized too:

```ng
type Box<T> {
    value: T;
}

val intBox = new Box<i32> { value: 42 };
val strBox = new Box<string> { value: "hello" };
```

### Generic Tagged Unions

```ng
type Option<T> = Some(value: T) | None;

fun unwrapOr<T>(opt: Option<T>, fallback: T) -> T {
    switch (opt) {
        case Some(v) { return v; }
        case None { return fallback; }
    }
}
```

### Generic Type Aliases

```ng
type Pair<T> = (T, T);
type Result<T, E> = Ok(value: T) | Err(error: E);
```

## Generic Wrapped Types

```ng
type Wrapper<T> wraps T;

val w = Wrapper<i32>(42);
```

## Generic Instance Mangling

Each generic instantiation gets a unique **mangled name** that encodes the module path and concrete type arguments. This ensures type safety across module boundaries:

```ng
// Module A
export fun foo<T>(x: T) -> T => x;

// Module B
import A;
val x = foo(42);    // calls A::foo<i32>
val y = foo("hi");  // calls A::foo<string> — different instance
```

## Const Generic Parameters

Type parameters can be constrained by compile-time constant values:

```ng
fun fixedArray<T, const N: i32>() -> array<T, N> {
    // Returns a fixed-size array of type array<T, N>
}

val arr: array<i32, 5> = fixedArray<i32, 5>();
```

## What's Next?

Continue to [References, Moves & Ownership](references-moves.md) to learn about NG's ownership model.

> **Try it:** `example/15.generics.ng` — Basic generics
> **Try it:** `example/52.const_array_vector_span.ng` — Const generic parameters