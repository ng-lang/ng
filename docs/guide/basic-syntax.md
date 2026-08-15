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

Bindings are immutable by default; `let mut` (or `:=` assignment) makes them
mutable. Rebinding uses `:=`.

```ng
let value = 1;            // immutable
let mut total = 0;        // mutable
total := total + 1;       // rebind/assign
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

## Builtin types

| Type | Meaning |
|---|---|
| `i8` `i16` `i32` `i64` | signed fixed-width integers |
| `u8` `u16` `u32` `u64` | unsigned fixed-width integers |
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
let bad: u8 = 300;    // type error: out of range for type u8
```

Mixed-width integer arithmetic is a type error (no implicit conversions);
equality and ordering compare across numeric widths.

## Operators

- Arithmetic: `+ - * / %` (same-type; integers checked per width at
  runtime, float ops follow IEEE-754)
- Comparison/equality: `== != < <= > >=` (cross-width numeric)
- Logic: `&& || !` (and the prelude's `not(...)`)
- Bitwise: `& | ^ << >>` (integers)
- String: `+` concatenation
- Range: `..` (half-open)
- Array append: `xs << value` (value semantics; integers keep `<<` shift)
- Prefix: `-`, `+`, `!`, `*` (deref), `move`, `clone`, `ref`, `ref mut`

## Scope and shadowing

Blocks introduce scopes; bindings shadow outer names:

```ng
fun main() -> unit {
    let value = 1;
    if (true) {
        let value = "shadowed";   // different binding
        print(value);
    }
    print(value == 1 ? "still one" : "changed");
}
```

## Expression statements and `if` expressions

Bodies use statements and an optional tail expression; `if` is a statement
(with `const if` for compile-time selection):

```ng
fun classify(value: i64) -> i64 {
    if (value > 0) { return 1; }
    return 0;
}
```

Next: [Control Flow](/guide/control-flow).
