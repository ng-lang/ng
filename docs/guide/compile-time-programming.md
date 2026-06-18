# Compile-Time Programming

NG offers powerful compile-time metaprogramming features: `const if`, `typeof`, **const predicates**, **const functions**, and **const generic parameters**. These allow you to write code that executes during compilation.

## Const If

`const if` evaluates a condition at compile time and **eliminates the dead branch entirely**. No code is generated for the eliminated branch.

### Basic Usage

```ng
const if (true) {
    val x = 42;      // Always compiled
    assert(x == 42);
} else {
    val x = 0;       // Never compiled — OK to be invalid
}

const if (false) {
    val x = 999;     // Eliminated
} else {
    val x = 7;
    assert(x == 7);
}
```

### Without Else

```ng
const if (true) {
    print("This always runs");
}
```

### Negation and Composition

```ng
const if (!false) {
    print("Negation works");
}
```

### In Generic Functions

`const if` is especially useful inside generic functions for type-dependent behavior:

```ng
fun process<T>(value: T) {
    const if (is_ref<T>) {
        print("Processing reference type");
    } else {
        print("Processing value type");
    }
}

val x = 42;
process(x);                // "Processing value type"
process(ref x);            // "Processing reference type"
```

## Typeof Queries

`typeof` provides compile-time type information. Combined with `const if`, it enables type-dependent code generation:

```ng
fun describe<T>(value: T) {
    const if (is_ref<T>) {
        print("T is a reference");
    } else {
        print("T is a value type");
    }
}
```

## Const Predicates

Const predicates are `const fun` declarations that return `bool` and can be used in `where` clauses for compile-time type filtering.

### Defining a Const Predicate

```ng
// In prelude or your module
const fun is_integral<T>() -> bool = native;

fun onlyIntegral<T: is_integral<T>()>(value: T) {
    print("Integral:", value);
}
```

### Using Const Predicates in Where Clauses

```ng
fun constrained<T>(value: T) where is_ref<T>() {
    // Only available when T is a reference type
}
```

## Const Specialization

Compose const predicates and where clauses for **conditional compilation**:

```ng
const fun is_numeric<T>() -> bool = native;

fun add<T>(a: T, b: T) -> T where is_numeric<T>() {
    return a + b;  // Only compiles for numeric types
}
```

Specialization patterns allow multiple definitions with different constraints — the compiler picks the most specific match:

```ng
// Base implementation
fun describe<T>(value: T) -> string {
    return "unknown";
}

// Specialization for numeric types
fun describe<T>(value: T) -> string where is_numeric<T>() {
    return "numeric";
}

// Specialization for strings
fun describe<T>(value: T) -> string where T is string {
    return "string";
}
```

## Const Functions

`const fun` declares a function that can be **evaluated at compile time**:

```ng
const fun factorial(n: i32) -> i32 {
    if (n == 0) {
        return 1;
    }
    return n * factorial(n - 1);
}

// The result is computed at compile time
val result: i32 = factorial(5);   // computed during compilation
```

### Const Functions in Const If

```ng
const fun isPositive(n: i32) -> bool => n > 0;

fun example() {
    const if (isPositive(42)) {
        print("42 is positive");  // always compiled
    }
}
```

### Recursive Const Functions

Const functions support recursive evaluation at compile time:

```ng
const fun fibonacci(n: i32) -> i32 {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

val fib10 = fibonacci(10);  // computed at compile time
```

### Limitations

- Const functions can only call other const functions
- They cannot call native functions
- They cannot use runtime I/O or side effects
- They are evaluated by the STUPID interpreter during compilation

## The `is_ref<T>` Predicate

Built into the prelude, `is_ref<T>` checks whether a type is a reference:

```ng
fun showType<T>(value: T) {
    const if (is_ref<T>) {
        print("T is a reference type");
    } else {
        print("T is a value type");
    }
}

val num = 42;
showType(num);          // "T is a value type"
showType(ref num);      // "T is a reference type"
```

## Const Generic Parameters

Type parameters can accept compile-time constant values:

```ng
fun makeArray<T, const N: i32>() -> array<T, N> {
    val result: vector<T> = [];
    loop i = 0 {
        if (i < N) {
            result << T();
            next i + 1;
        }
    }
    return result;
}

val arr: array<i32, 5> = makeArray<i32, 5>();
```

This is used for fixed-size array types and dimension-dependent code.

## Delete Specialization

The `delete` keyword eliminates specific generic instantiations:

```ng
// Accept all types
fun process<T>(value: T) { print("default"); }

// Reject strings
delete fun process<string>(value: string);
```

Deleted specializations produce compile-time errors for matching calls.

## Putting It All Together

Here's a complete example combining const if, typeof, const predicates, and specialization:

```ng
const fun is_printable<T>() -> bool = native;

fun prettyPrint<T>(value: T) where is_printable<T>() {
    const if (is_ref<T>) {
        print("(ref) ", *value);
    } else {
        print("(val) ", value);
    }
}

val x = 42;
prettyPrint(x);          // "(val) 42"
prettyPrint(ref x);      // "(ref) 42"
```

## What's Next?

Continue to [Advanced Generics](advanced-generics.md) for higher-kinded types, type specialization, and enhanced tuples.

> **Try it:** `example/17.const_if.ng` — Const if basics
> **Try it:** `example/42.const_type_predicate.ng` — Const type predicates
> **Try it:** `example/43.const_specialization.ng` — Const specialization
> **Try it:** `example/45.native_constraints.ng` — Native constraints
> **Try it:** `example/46.const_trait_constraints.ng` — Const trait constraints
> **Try it:** `example/47.const_generic_instances.ng` — Const generic instances
> **Try it:** `example/53.const_fun.ng` — Const functions