# Syntax Ergonomics

> **Status:** Restructured from original 7-feature monolith into 3 sequential batches.
> **Review note:** Each batch is an independent proposal. Implement in order.

---

## Batch 1: String Interpolation + `for`/`while` Loops (2 weeks)

### 1. String Interpolation

#### Lexer Changes

In `src/parsing/Lexer.cpp`, modify string literal lexing:

When the lexer encounters `{` inside a `"` string literal, it:
1. Closes the current string segment
2. Enters a new state: `InterpolationState`
3. Lexes expressions until matching `}`
4. Returns to string literal state

Token sequence for `"Hello, {name}!"`:
```
STRING_START("Hello, ")
EXPR_START
IDENTIFIER(name)
EXPR_END
STRING_END("!")
```

#### New AST Node

```cpp
struct InterpolatedString : Expression {
    // A sequence of string parts and expressions
    struct Part {
        std::variant<std::string, ASTRef<Expression>> content;
    };
    Vec<Part> parts;

    explicit InterpolatedString(SourcePosition pos, Vec<Part> parts)
        : Expression(ASTNodeType::INTERPOLATED_STRING, std::move(pos)), parts(std::move(parts)) {}
};
```

#### Desugaring (Compiler)

```ng
"Hello, {name}! You are {age} years old."
```
Desugars to:
```ng
"Hello, " + (name as string) + "! You are " + (age as string) + " years old."
```

#### Type Checker

The type of an interpolated string is always `string`. Each expression part is coerced to string via:
1. If the expression is already `string` → use as-is
2. Otherwise → emit `(expr as string)`

### 2. `for` Loop — Reuse Existing Infrastructure

#### Existing Infrastructure (Already Present)

The codebase already has:
- `KEYWORD_FOR` token (line 58 in `include/token.hpp`)
- `KEYWORD_IN` token (line 48 in `include/token.hpp`)
- `KEYWORD_BREAK` / `KEYWORD_CONTINUE` tokens (lines 46-47)
- `LoopBindingType::LOOP_IN = 1` (line 475 in `include/ast.hpp`) — defined but **never parsed**
- `LoopStatement` with `Vec<LoopBinding> bindings` supporting `LOOP_IN`

#### No New AST Node Needed

Instead of creating a new `ForStatement`, `for i in range` creates a `LoopStatement` with `LOOP_IN` bindings:

```cpp
// Parser change: when KEYWORD_FOR appears at statement-start position
// (not inside impl block), parse as loop:
auto parseForStatement() -> ASTRef<Statement> {
    accept(TokenType::KEYWORD_FOR);
    auto loopStmt = createNode<LoopStatement>();

    LoopBinding binding;
    binding.name = identifier();
    accept(TokenType::KEYWORD_IN);
    binding.target = expression();
    binding.type = LoopBindingType::LOOP_IN;
    loopStmt->bindings.push_back(binding);

    accept(TokenType::LEFT_CURLY);
    loopStmt->loopBody = parseBlock();
    accept(TokenType::RIGHT_CURLY);

    return loopStmt;
}
```

#### Desugaring in Interpreter/Compiler

The `LOOP_IN` handler iterates over the target:

```cpp
case LoopBindingType::LOOP_IN: {
    // For Range: iterate start..end
    // For Array: iterate 0..len
    val iter = evaluate(binding.target);
    loop {
        val next = iter.next();
        if (next is None) { break; }
        setVariable(binding.name, next);
        execute(body);
        next;
    }
}
```

#### `break`/`continue`

`KEYWORD_BREAK` and `KEYWORD_CONTINUE` tokens already exist but have no AST nodes. Add:
```cpp
struct BreakStatement : Statement {
    ASTRef<Expression> value = nullptr;  // Optional break value (for loop-as-expression)
};
struct ContinueStatement : Statement {};
```

#### `KEYWORD_FOR` Disambiguation

The parser distinguishes:
- `for` at **statement level** (after `;` or `{` or at top level) → loop
- `for` after `impl` → trait implementation

#### `break`/`continue` Acceptance Criteria

- `break` inside a loop exits the loop
- `continue` inside a loop jumps to the next iteration
- `break` outside a loop is a compile error
- `continue` outside a loop is a compile error

---

## Batch 2: Lambda/Closure Syntax (2-3 weeks)

### Syntax

```ng
// Arrow lambda:
val double = |x: i32| -> i32 => x * 2;

// Multi-expression:
val process = |x: i32| -> i32 {
    val tmp = x * 2;
    return tmp + 1;
};

// Type inference:
val add = |a, b| => a + b;

// Higher-order:
fun map<T, U>(xs: vector<T>, f: (T) -> U) -> vector<U> { ... }
```

### New AST Node

```cpp
struct LambdaExpression : Expression {
    Vec<Param> params;
    ASTRef<Expression> body;         // for single-expression (=>)
    ASTRef<Statement> blockBody;     // for multi-expression ({})
    bool isShort;                    // true if => syntax
    
    // Generated closure capture info:
    Vec<Str> capturedVars;           // variables captured from enclosing scope
};
```

### Closure Semantics

- **By-value capture by default**: captured variables are copied into the closure
- **By-ref capture**: `|ref x| x + 1` captures `x` by reference
- **By-move capture**: `|move x| x + 1` moves `x` into the closure

### Type

A lambda `|x: i32| -> string => ...` has type `(i32) -> string`.

The type checker produces an anonymous function type.

### Desugaring

```ng
val double = |x: i32| -> i32 => x * 2;

// Desugars to a local function definition:
fun __lambda_1(x: i32) -> i32 {
    return x * 2;
}
val double = __lambda_1;
```

With capture:
```ng
val factor = 2;
val multiply = |x: i32| -> i32 => x * factor;

// Desugars to:
fun __lambda_1(x: i32, __capture_factor: i32) -> i32 {
    return x * __capture_factor;
}
val multiply = __lambda_1(?, factor);  // partial application (future)
```

### Batch 2 Acceptance Criteria

- `|x: i32| -> i32 => x * 2` can be called and returns correct value
- Type inference: `|a, b| => a + b` infers types from context
- Multi-expression lambdas compile and execute
- Lambdas capture variables from enclosing scope
- `map([1,2,3], |x| => x * 2)` works with higher-order functions

---

## Batch 3: `match` Expression + Operator Overloading (3-4 weeks)

### 1. `match` as Expression

#### Syntax

```ng
val hex = match (color) {
    case Red => "#FF0000";
    case Green => "#00FF00";
    case Blue => "#0000FF";
};
```

#### AST Changes

`MatchExpression` reuses the existing `SwitchStatement` infrastructure but returns a value:

```cpp
struct MatchExpression : Expression {
    ASTRef<Expression> scrutinee;
    Vec<MatchArm> arms;
};

struct MatchArm : ASTNode {
    ASTRef<Pattern> pattern;
    ASTRef<Expression> value;    // => value
};
```

#### Type Checker

- All arms must return the same type (or be unified)
- Exhaustiveness checking: same rules as `switch`
- `match` can be used in any expression context

### 2. Operator Overloading

#### Trait Definitions (Prelude)

```ng
export trait Add<Rhs = Self> {
    fun operator+(self: ref<Self>, rhs: ref<Rhs>) -> Self;
}

export trait Sub<Rhs = Self> {
    fun operator-(self: ref<Self>, rhs: ref<Rhs>) -> Self;
}

export trait Mul<Rhs = Self> { ... }
export trait Div<Rhs = Self> { ... }
export trait Neg { fun operator-(self: ref<Self>) -> Self; }
export trait Eq<Rhs = Self> { fun operator==(self: ref<Self>, rhs: ref<Rhs>) -> bool; }
export trait Index<Idx> { fun operator[](self: ref<Self>, index: Idx) -> ref<T>; }
```

#### Usage

```ng
type Complex {
    real: f64;
    imag: f64;
}

impl Add for Complex {
    fun operator+(self: ref<Self>, rhs: ref<Self>) -> Complex {
        return Complex { real: self.real + rhs.real, imag: self.imag + rhs.imag };
    }
}

val a = Complex { real: 1.0, imag: 2.0 };
val b = Complex { real: 3.0, imag: 4.0 };
val c = a + b;  // Calls Complex::operator+
```

#### Type Checker Changes

- `a + b` first looks for an inherent `operator+`, then trait `Add`
- Desugars to `a.operator+(b)` or `Add::operator+(a, b)`
- Operator resolution follows trait resolution rules (inherent wins over trait)

### Batch 3 Acceptance Criteria

- `match (x) { case A => 1; case B => 2; }` returns a usable value
- All arms must have the same type (compile error on mismatch)
- `Complex + Complex` works via operator overloading
- `Complex == Complex` works via operator overloading
- Built-in operators (i32 + i32) are unaffected

---

## Implementation Priority

| Batch | Deliverable | Effort | Dependencies |
|---|---|---|---|
| 1a | String interpolation | 1 week | None |
| 1b | `for`/`while` loops | 1 week | None |
| 2 | Lambdas/closures | 2-3 weeks | None |
| 3a | `match` expression | 1 week | None |
| 3b | Operator overloading | 2-3 weeks | Trait system (exists) |