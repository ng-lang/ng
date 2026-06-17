# Control Flow

This chapter covers conditional execution, loops, and pattern matching in NG.

## Conditional Execution: `if / else`

```ng
val x = 10;

if (x > 0) {
    print("positive");
} else if (x < 0) {
    print("negative");
} else {
    print("zero");
}
```

The condition must be a `bool` expression. Parentheses around the condition are required.

### Expression vs Statement

`if/else` is an expression that produces a value. You can use it in assignments:

```ng
val x = 10;
val sign = if (x > 0) { "positive" } else { "negative" };
print(sign);  // "positive"
```

When used as an expression, both branches must return the same type.

## Loops: `loop / next`

NG has a single loop construct called `loop` with explicit `next` for continuation.

### Basic Loop

```ng
loop {
    print("infinite loop");
    next;  // continue to next iteration
}
```

### Loop with Counter Variable

The loop variable is initialized and updated on each `next`:

```ng
fun sum(n: i32) -> i32 {
    val s = 0;
    loop i = 0 {
        s = s + i;
        if (i < n) {
            next i + 1;   // next iteration with new i
        }
    }
    return s;
}

print(sum(10));  // 45
```

### Loop with Multiple Variables

```ng
loop i = 0, j = 10 {
    if (i < j) {
        next i + 1, j - 1;
    }
}
```

### Loops Return Values

A loop terminates when `next` is not called — execution falls through to the statement after the loop body. The loop itself can also produce a value:

```ng
val result = loop i = 0 {
    if (i >= 10) {
        break i;   // exit with value
    }
    next i + 1;
};
// result == 10
```

> **Note:** `break` with a value is used to exit the loop early and produce a result.

### Max Loop Stack

NG protects against infinite recursion in loops via a configurable max stack depth. See `example/12.loop_max_stack.ng`.

## Pattern Matching: `switch`

The `switch` statement performs exhaustive pattern matching on tagged unions.

```ng
type Result = Ok(value: i32) | Err(msg: string);

val result = Ok(42);

switch (result) {
    case Ok(value) {
        print("Success:", value);
    }
    case Err(msg) {
        print("Failure:", msg);
    }
}
```

### The `otherwise` Branch

The `otherwise` branch catches any variant not explicitly handled:

```ng
switch (result) {
    case Ok(value) {
        print("Success:", value);
    }
    otherwise {
        print("Something else happened");
    }
}
```

### Switch Exhaustiveness

If you omit a variant and don't provide `otherwise`, the type checker reports an error. All variants must be covered.

### Member Access on Tagged Unions

You can inspect the active variant at runtime:

```ng
print(result.tag);    // "Ok"
print(result.index);  // 0
```

## Iteration with Ranges

Ranges provide a way to iterate over sequences without explicit loop variables:

```ng
// Inclusive range
loop i in 0..=5 {
    print(i);  // 0, 1, 2, 3, 4, 5
}

// Exclusive range
loop i in 0..5 {
    print(i);  // 0, 1, 2, 3, 4
}

// Descending range
loop i in 5..0 {
    print(i);  // 5, 4, 3, 2, 1
}
```

### Range Expressions

```ng
val r1 = 0..10;        // exclusive: 0 to 9
val r2 = 0..=10;       // inclusive: 0 to 10
val r3 = 10..0;        // descending
```

## Const If (Compile-Time)

NG supports compile-time conditional evaluation with `const if`:

```ng
const if (true) {
    val x = 42;        // Always compiled
} else {
    val x = 0;         // Eliminated at compile time
}
```

The condition must be evaluable at compile time (a literal or `typeof` query). The dead branch is **entirely removed** — no code is generated for it.

See [Compile-Time Programming](compile-time-programming.md) for more details.

## What's Next?

Continue to [Functions](functions.md) to learn how to define and use functions in NG.

> **Try it:** `example/10.loop.ng` — Loop basics
> **Try it:** `example/17.const_if.ng` — Compile-time branching
> **Try it:** `example/20.switch_otherwise.ng` — Switch with otherwise
> **Try it:** `example/57.ranges_slicing_pipeline.ng` — Range and slicing pipeline