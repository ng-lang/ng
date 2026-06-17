# Functions

Functions are first-class citizens in NG. This chapter covers defining, calling, and composing functions.

## Defining Functions

Functions are defined with the `fun` keyword:

```ng
fun add(x: i32, y: i32) -> i32 {
    return x + y;
}
```

- `fun` — keyword
- `add` — function name
- `(x: i32, y: i32)` — parameters with types
- `-> i32` — return type
- `{ ... }` — function body

### Shorthand Syntax

Single-expression functions can use the `=>` arrow:

```ng
fun add(x: i32, y: i32) -> i32 => x + y;
```

## Calling Functions

```ng
val result = add(3, 4);  // 7
```

## Return Values

The `return` keyword exits the function with a value. If omitted, the function returns `unit`:

```ng
fun greet(name: string) {
    print("Hello, ", name, "!");  // implicit return of unit
}
```

The last expression in a block is **not** automatically returned — you must use `return`:

```ng
fun double(x: i32) -> i32 {
    return x * 2;   // The return is required here
}
```

## Parameters

### Required Type Annotations

All parameters must have explicit type annotations:

```ng
fun valid(x: i32, y: string) -> bool { ... }
```

### Default Arguments

Parameters can have default values. Defaults are evaluated at each call site and can reference earlier parameters:

```ng
fun greet(greeting: string, name: string = "World") -> string {
    return greeting + ", " + name + "!";
}

print(greet("Hello"));          // "Hello, World!"
print(greet("Hi", "NG"));       // "Hi, NG!"
```

## Recursion

Functions can call themselves:

```ng
fun factorial(n: i32) -> i32 {
    if (n == 0) {
        return 1;
    }
    return n * factorial(n - 1);
}

print(factorial(5));  // 120
```

### Tail Recursion

NG optimizes tail-recursive calls. A function is tail-recursive when the recursive call is the last operation before returning:

```ng
fun sum_tail(n: i32, acc: i32 = 0) -> i32 {
    if (n == 0) {
        return acc;
    }
    return sum_tail(n - 1, acc + n);  // tail call
}
```

## Native Functions

Functions can be implemented in C++ via the `= native;` declaration:

```ng
fun my_native(arg: i32) -> bool = native;
```

This tells the compiler that the function body is provided by the host runtime. The ORGASM VM uses `vm.register_native(...)` to bind the implementation.

## Member Functions (Methods)

Types can have member functions that receive `self`:

```ng
type Counter {
    property value: i32;

    fun increment(self: ref<Self>, delta: i32) {
        self.value = self.value + delta;
    }

    fun get(self: ref<Self>) -> i32 {
        return self.value;
    }
}

val c = new Counter { value: 0 };
c.increment(5);
print(c.get());  // 5
```

The `Self` type refers to the enclosing type. Methods use `ref<Self>` receiver parameters to allow mutation.

## Higher-Order Functions

Functions can accept other functions as parameters. This is typically done through trait objects (see [Traits](traits.md)), but simple function references work too:

```ng
fun apply_twice(f: (i32) -> i32, x: i32) -> i32 {
    return f(f(x));
}

fun double(x: i32) -> i32 => x * 2;
print(apply_twice(double, 5));  // 20 (5*2*2)
```

## Function Types

Function types are written as `(ParamType1, ParamType2) -> ReturnType`:

```ng
type Op = (i32, i32) -> i32;

fun execute(op: Op, a: i32, b: i32) -> i32 {
    return op(a, b);
}
```

## Overloading

Multiple functions can share the same name if they have different parameter types. The type checker selects the best match through overload resolution:

```ng
fun print_value(x: i32) { print("int:", x); }
fun print_value(x: string) { print("string:", x); }

print_value(42);       // calls first overload
print_value("hello");  // calls second overload
```

## What's Next?

Continue to [Data Structures](data-structures.md) to learn about arrays, tuples, objects, and tagged unions.

> **Try it:** `example/03.funcall_and_idexpr.ng` — Function calls and index expressions
> **Try it:** `example/09.scope.ng` — Scope and closures