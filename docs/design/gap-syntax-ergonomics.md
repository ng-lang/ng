# Syntax Ergonomics: String Interpolation, Lambdas, For Loops, And Operator Overloading

## Order

Recommended implementation order: **10**.

## Goal

Improve NG's syntax ergonomics to reduce boilerplate and align with modern language conventions, making NG more approachable and productive.

## Motivation

NG's syntax is functional but lacks many conveniences developers expect in 2026. Specific pain points:

| Pain Point | Current NG | Common Convention |
|---|---|---|
| String building | `"Hello, " + name + "!"` | `"Hello, {name}!"` |
| Simple lambdas | Must define a named `fun` | `\|x\| x + 1` |
| Loops | Only `loop` with `next` | `for`, `while`, `do-while` |
| Custom operators | No user-defined operators | `operator+`, `operator[]` |
| Match expression | `switch` as statement only | `match { ... }` as expression |
| Null safety | Manual `Option<T>` tagged union | Built-in `?` / `Optional<T>` |

## Proposed Syntax Additions

### 1. String Interpolation

```ng
val name = "Alice";
val age = 30;
print("Hello, {name}! You are {age} years old.");
print("Expression: {1 + 2}");                    // "Expression: 3"
print("Escape: \{\}");                            // "Escape: {}"
```

Desugars to concatenation at compile time:
```ng
"Hello, " + name + "! You are " + (age as string) + " years old."
```

### 2. Lambda / Closure Syntax

```ng
// Arrow lambda
val double = |x: i32| -> i32 => x * 2;
val result = double(5);                           // 10

// Multi-expression lambda
val process = |x: i32| -> i32 {
    val tmp = x * 2;
    return tmp + 1;
};

// Type inference
val add = |a, b| => a + b;                        // Types inferred

// Higher-order functions
fun map<T, U>(xs: [T], f: |T| -> U) -> [U] { ... }
```

### 3. `for` Loop (Range-Based)

```ng
// Basic range iteration
for i in 0..10 {
    print(i);
}

// Over arrays
for item in arr {
    print(item);
}

// With index
for (i, item) in enumerate(arr) {
    print(i, ": ", item);
}

// Reverse
for i in (10..0) {
    print(i);
}
```

### 4. `while` Loop

```ng
val x = 10;
while (x > 0) {
    x = x - 1;
}
```

### 5. `match` Expression

```ng
type Color = Red | Green | Blue;

val hex = match (color) {
    case Red => "#FF0000";
    case Green => "#00FF00";
    case Blue => "#0000FF";
};

// With guards
val description = match (value) {
    case Ok(v) if v > 100 => "large success";
    case Ok(v) => "success";
    case Err(m) => "failure: " + m;
};
```

### 6. Operator Overloading

```ng
type Complex {
    real: f64;
    imag: f64;
}

impl Add<Complex> for Complex {
    fun operator+(self: ref<Self>, other: ref<Self>) -> Complex {
        return Complex { real: self.real + other.real, imag: self.imag + other.imag };
    }
}

impl Index<i32> for Vector {
    fun operator[](self: ref<Self>, index: i32) -> ref<T> {
        // Return reference to element at index
    }
}
```

Overloadable operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `[]`, `()`, `<<`

### 7. `Optional<T>` As Sugar

```ng
// Desugars to the tagged union:
// type Option<T> = Some(value: T) | None

fun findUser(id: i32) -> Optional<User> {
    if (exists(id)) {
        return Some(loadUser(id));
    }
    return None;
}

val user = findUser(42)?;                         // Error on None
```

## Dependencies

- String interpolation: requires parser changes to string literal lexing.
- Lambdas: requires `Fn(T) -> U` callable trait in the type system.
- `for`/`while`: can desugar to existing `loop` AST nodes.
- `match`: can desugar to existing `switch`.
- Operator overloading: requires trait definitions for each operator.
- Unblocks: more idiomatic example code, lower barrier for new users.

## Scope

**In scope:**
- String interpolation (compile-time desugaring)
- Lambda/closure syntax
- `for` loop (desugar to `loop`)
- `while` loop (desugar to `loop`)
- `match` as expression (desugar to `switch`)
- Operator overloading via traits
- `Optional<T>` sugar

**Out of scope:**
- Custom infix operators (e.g., defining `+++` as a user operator)
- Pipeline operator `|>` (already exists as `|>`)
- Destructuring assignment syntax
- Pattern matching in function arguments

## Acceptance Criteria

- String interpolation produces correct concatenated strings
- Lambdas can be called directly and passed to higher-order functions
- `for i in 0..10` iterates 10 times with correct values
- `while (cond) { ... }` loops until condition is false
- `match` returns a value usable in assignments
- User-defined `+` operator on `Complex` works in arithmetic expressions
- `Optional<T>` behaves identically to the hand-written `Option<T>` tagged union
- All existing tests pass without modification
- STUPID and ORGASM backends both support all new syntax

## Potential Challenges

- String interpolation interacts with escape sequences — must be carefully parsed.
- Lambda closure capture semantics need design (by ref? by value? by move?).
- Operator overloading can lead to confusing code — Rust-style restrictions on custom operators may be warranted.
- `for` loop desugaring must handle early `return` and `break` correctly.
- `match` exhaustiveness checking must be preserved when used as an expression.
- The ORGASM compiler and VM must support all new constructs.