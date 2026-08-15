# Control Flow

Conditional execution, loops, pattern matching, and compile-time selection.

## `if` / `else`

```ng
if (score >= 60) {
    print("pass");
} else {
    print("fail");
}
```

## Loops: `loop` and `next`

`loop (i = 0)` declares loop bindings; `next (expr)` continues the next
iteration with new binding values, and falling off the body (or `return`)
exits the loop:

```ng
fun sumTo(limit: i64) -> i64 {
    let mut total = 0;
    loop (i = 0) {
        total := total + i;
        if (i + 1 == limit) { return total; }   // fall out via return
        next (i + 1);
    }
    return total;
}
```

To exit early, structure the loop so the body completes without `next`
(reach the end of the block), or `return`:

```ng
fun firstEven(values: array<i64>) -> i64 {
    loop (i = 0) {
        if (values[i] % 2 == 0) { return values[i]; }
        if (i + 1 == len(values)) { return -1; }   // exhausted
        next (i + 1);
    }
    return -1;
}
```

Tail recursion is also recognized and runs without growing the call stack.

## `switch` — enum variants

`switch` over enums destructures variants, with checked exhaustiveness or an
`otherwise` fallback:

```ng
enum Result<T> {
    Ok(value: T),
    Err(message: string),
}

fun describe(result: Result<i64> ref) -> string {
    switch (*result) {
        case Ok(value) {
            if (value > 0) { return "positive"; }
            return "ok";
        }
        case Err(message) { return "error: " + message; }
    }
}
```

Multi-field variants bind positionally (`case Cell(head, rest)`); recursive
payloads use `ref<Node<T>>` and pass the reference on.

## `switch` — literal patterns

Scalar switches match integer, bool, and string literals, including
or-patterns (`|`) and an optional `otherwise`:

```ng
fun classify(score: i64) -> string {
    switch (score) {
        case 0 | 1 | 2 { return "low"; }
        case 3 | 4 { return "mid"; }
        otherwise { return "high"; }
    }
}
```

String and bool switches work the same way (`case "a" | "b"`, `case true`).
Integer literals adopt the scrutinee's type with range checks; duplicates
and mixed pattern kinds are type errors.

## `const if`

`const if` folds at compile time. Conditions may use const predicates,
`const fun` calls, `T is Type` tests, and const-capable natives:

```ng
const if (is_ref<T>) {
    return 1;
} else {
    return 0;
}
```

Inside generic functions, per-instance `const if` evaluates against each
monomorphized instance's concrete bindings.

## `return`

```ng
fun main() -> i64 {
    return 42;
}
```

A body's tail expression is returned implicitly when the function has no
explicit `return` for that path.

Next: [Functions](/guide/functions).
