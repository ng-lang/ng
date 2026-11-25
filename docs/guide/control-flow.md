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

`sumTo(10)` is `45`.

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

## `switch` — enum variants

`switch` over enums destructures variants, with checked exhaustiveness or an
`otherwise` fallback:

```ng
enum Res<T> {
    Ok(value: T),
    Err(message: string),
}

fun describe(result: Res<i64> ref) -> string {
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
payloads use `ref<Node<T>>` and pass the reference on:

```ng
enum Node<T> {
    Cell(content: T, next: ref<Node<T>>),
    Empty,
}

fun count<T>(head: ref<Node<T>>) -> i64 {
    switch (*head) {
        case Empty { return 0; }
        case Cell(value, rest) { return 1 + count(rest); }
    }
}
```

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

String and bool switches work the same way (`case "north" | "east"`,
`case true` / `case false`). Integer literals adopt the scrutinee's type
with range checks; duplicates and mixed pattern kinds are type errors:

```ng
duplicate literal `2` in switch
literal `a` does not match switch type i64
```

## `switch` — tuple patterns

A tuple scrutinee matches `case (a, b)` (or more names) and binds each
element by position. The binding count must equal the tuple arity. One
matching pattern covers every value of that tuple type, so `otherwise` is
optional; without a tuple pattern, `otherwise` is required.

```ng
fun addPair(pair: tuple<i64, i64>) -> i64 {
    switch (pair) {
        case (a, b) { return a + b; }
    }
}
```

A one-name form `case (n: i64)` is a union type pattern. Tuple patterns need
at least two names, so the colon vs comma is unambiguous.

## `switch` — union members

A union annotation (`A | B`) switches on the active member: member literals
(`case 42`), type patterns (`case (n: i64)`), and `otherwise` for the
remaining members.

```ng
fun classify(value: i64 | string) -> i64 {
    switch (value) {
        case 42 { return 100; }
        case (n: i64) { return n; }
        otherwise { return 7; }
    }
}
```

Union values also compare and order against their member types
(`w == 42`, `w > 40` both work for `w: bool | i64 | f64`).

## `const if`

`const if` folds at compile time. Conditions may use const predicates
(`is_ref<i64>`), `const fun` calls, and const-capable natives:

```ng
const fun even(value: i64) -> bool { return (value % 2) == 0; }

fun main() -> unit {
    const if (even(4)) { print("yes"); } else { print("no"); }
    const if (is_ref<i64>) { print("ref"); } else { print("notref"); }
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
