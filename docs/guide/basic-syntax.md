# Basic Syntax

This chapter covers the fundamental building blocks of NG: comments, variables, data types, operators, and expressions.

## Comments

NG supports single-line comments only:

```ng
// This is a comment.
// Comments start with // and extend to the end of the line.
```

There is no multi-line `/* ... */` syntax.

## Variables and Mutability

Variables are declared with the `val` keyword. By default, variables are **mutable** — you can reassign them using the `:=` operator:

```ng
val x = 1;   // x is 1 (initial binding via =)
x := 2;      // now x is 2 (reassigned via :=)
```

### Immutability

The language currently has no `const` or `let` distinction — all `val` bindings are mutable. If you need a value that shouldn't change, it's a convention not to reassign it.

### Type Annotations

You can explicitly annotate the type:

```ng
val x: i32 = 1;
val name: string = "NG";
val flag: bool = true;
val pi: f64 = 3.14159;
```

If you omit the annotation, the type is **inferred** from the initializer. Numeric literals default to `i32` (integer) or `f32` (floating):

```ng
val a = 42;      // a is i32
val b = 3.14;    // b is f32
```

## Primitive Data Types

| Type | Description | Example |
|---|---|---|
| `i8`, `i16`, `i32`, `i64` | Signed integers | `-42`, `0xFF`, `0b1010` |
| `u8`, `u16`, `u32`, `u64` | Unsigned integers | `42u`, `0xFEu8` |
| `f32`, `f64` | Floating-point numbers | `3.14`, `1.0e-10f64` |
| `bool` | Boolean | `true`, `false` |
| `string` | UTF-8 string | `"hello"`, `'world'` |
| `unit` | No value | `unit` (like `void` in C) |

### Numeric Literals

```ng
val a = 42;          // i32
val b = 42u16;       // u16 (suffix)
val c = -128i8;      // explicit i8
val d = 0xFF;        // hexadecimal → i32
val e = 0b1010;      // binary → i32
val f = 3.14;        // f32 (default float)
val g = 3.14f64;     // f64
val h = 1.0e10;      // scientific notation
```

### String Literals

```ng
val s1 = "double quoted";
val s2 = 'single quoted';
val s3 = "escape sequences: \n \t \\ \"";
```

## Operators

### Arithmetic

| Operator | Description |
|---|---|
| `+` | Addition / string concatenation |
| `-` | Subtraction / negation |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo |

### Comparison

| Operator | Description |
|---|---|
| `==` | Equal |
| `!=` | Not equal |
| `>` | Greater than |
| `<` | Less than |
| `>=` | Greater than or equal |
| `<=` | Less than or equal |

### Logical

There are no `&&` or `||` operators. Use functions from the standard prelude:

```ng
val result = not(x > 0);   // logical NOT
```

### Other Operators

| Operator | Description | Example |
|---|---|---|
| `<<` | Array append | `arr << 6` |
| `is` | Type check | `x is i32` |
| `\|>` | Pipe forward | `value \|> transform` |
| `..` | Range (exclusive end) | `0..10` |
| `..=` | Range (inclusive end) | `0..=10` |
| `...` | Spread / pack expansion | `...args` |
| `.` | Property/method access | `obj.field` |
| `:=` | **Assignment / mutation** | `x := 42` |
| `=` | **Binding** (in `val` declaration) | `val x = 42` |
| `*ptr := value` | Deref assignment | mutate through a reference |

### Pipe Forward Operator

The `|>` operator pipes a value into a function call:

```ng
fun double(x: i32) -> i32 => x * 2;
fun inc(x: i32) -> i32 => x + 1;

val result = 5 |> inc |> double;  // 12 (same as double(inc(5)))
```

## Expressions and Statements

NG is **expression-oriented** — most constructs produce values. A **statement** is terminated by `;` and does not produce a value.

```ng
val x = 42;          // statement
val y = x + 1;       // expression (x + 1) assigned to y
```

## Blocks and Scope

A block `{ ... }` groups statements and creates a new scope. Variables declared inside a block are not visible outside it.

```ng
val outer = 1;
{
    val inner = 2;    // only visible inside this block
    outer := inner;    // OK: outer is in scope (mutation via :=)
}
// inner is not accessible here
```

## Unit Type

The `unit` type represents the absence of a value. It is analogous to `void` in C or `None` in Python. Functions that don't return a value implicitly return `unit`.

```ng
val nothing = unit;   // nothing has type unit
```

## What's Next?

Now that you understand the basics, move on to [Control Flow](control-flow.md) to learn about conditionals and loops.

> **Try it:** `example/02.many_defs.ng` — Multiple definitions and basic expressions
> **Try it:** `example/05.valdef.ng` — Value definitions and type annotations