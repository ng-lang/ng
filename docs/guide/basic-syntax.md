# Basic Syntax

This chapter covers NG's fundamental building blocks: comments, bindings,
types, literals, and operators.

## Comments

Line comments start with `//`; block comments use `/* ... */`:

```ng
// line comment
/* block
   comment */
```

## Bindings

Bindings are immutable by default; `let mut` makes them mutable. Assignment
uses `:=`.

```ng
let value = 1;            // immutable
let mut total = 0;        // mutable
total := total + 1;       // assign
```

`let` supports typed annotations (including generic type parameters):

```ng
let n: i64 = 1;
let xs: array<i64> = [1, 2, 3];
```

Tuple destructuring and rest patterns:

```ng
let pair = (1, "two", true);
let (first, ...rest) = pair;   // first == 1, rest == ("two", true)
```

Running that, `first` is `1`, `rest[0]` is `two`, and `rest[1]` is `true`.

## Builtin types

| Type | Meaning |
|---|---|
| `i8` `i16` `i32` `i64` | signed fixed-width integers |
| `u8` `u16` `u32` `u64` | unsigned fixed-width integers |
| `isize` `usize` | target-width signed/unsigned integers (64-bit on supported targets) |
| `f32` `f64` | IEEE-754 floats |
| `bool` | `true` / `false` |
| `string` | UTF-8-ish string values |
| `unit` | the no-value type (empty `{}` returns) |
| `array<T>` | dynamic array |
| `array<T, N>` | fixed-size array |
| `range<T>` | half-open range value (`1..5` = 1,2,3,4) |
| `T ref` / `T ref mut` | scoped shared/mutable reference (prefix `ref<T>` also works) |
| `ref<Trait>` | dynamic trait view |
| `A \| B` | union annotation |

References are created with `ref` / `ref mut` and dereferenced with `*`:

```ng
fun takes(r: i64 ref) -> unit { print(*r); }

fun main() -> unit {
    let x = 7;
    takes(ref x);
    let mut y = 8;
    let m: i64 ref mut = ref mut y;
    let p: ref<i64> = ref x;
    takes(p);
}
```

## Literals

```ng
let integer = 42;
let signed = -7;
let suffixed = 255u8;        // numeric suffixes: i8..i64, u8..u64, f32, f64
let floating = 1.5f32;
let text = "hello\nworld";   // escapes: \" \\ \n \t
let yes = true;
let tuple = (1, "two", true);
let list = [1, 2, 3];
```

Integer literals adopt the expected type and are range-checked per width:

```ng
let byte: u8 = 200;   // ok
let bad: u8 = 300;    // error
```

The out-of-range literal is rejected at compile time:

```text
type error E-type at bytes [35, 38): integer literal `300` is out of range for type u8
```

Mixed-width integer arithmetic is a type error (no implicit conversions);
equality and ordering compare across numeric widths:

```ng
let a: i32 = 1;
let b: i64 = 2;
print(a == b);   // false
print(a < b);    // true
let c = a + b;   // error
```

```text
type error E-type at bytes [126, 131): binary operands type mismatch: expected i32, got i64
```

Integer arithmetic is checked per width and traps when it overflows.

## Operators

- Arithmetic: `+ - * / %` (same-type; integers checked per width at
  runtime, float ops follow IEEE-754)
- Comparison/equality: `== != < <= > >=` (cross-width numeric)
- Logic: `&& || !` (and the prelude's `not(...)`)
- Bitwise: `& | ^ << >>` (integers)
- String: `+` concatenation
- Range: `..` (half-open)
- Array append: `xs << value` (produces a new array; integers keep `<<` shift)
- Prefix: `-`, `+`, `!`, `*` (deref), `move`, `clone`, `ref`, `ref mut`

```ng
let xs = [1, 2, 3];
let longer = xs << 4;   // longer == [1, 2, 3, 4]; xs unchanged
let shifted = 1 << 3;   // 8
```

There is no conditional (`?:`) operator — use `if` statements.

## Scope and shadowing

Blocks introduce scopes; bindings shadow outer names:

```ng
fun main() -> unit {
    let value = 1;
    if (true) {
        let value = "shadowed";   // different binding
        print(value);
    }
    print(value);
}
```

```text
shadowed
1
```

## Statements and `if`

Bodies are sequences of statements with an optional final `return`; `if` is a
statement (with `const if` for compile-time selection), not an expression:

```ng
fun classify(value: i64) -> i64 {
    if (value > 0) { return 1; } else { return 0; }
}
```

Using `if` where an expression is expected is a syntax error:

```text
syntax error E-parse at bytes [156, 158): expected an expression, found `if`
```

Next: [Control Flow](/guide/control-flow).
