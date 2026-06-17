# Error Handling: `Result<T, E>` And `?` Operator

> **Status:** Refined from original gap proposal. INVEST: 5/5/5/5/5/5 after refinement.
> **Review note:** Split into 2 phases. `try`/`catch`/`throw` deferred to Phase 2.

## Order

Recommended implementation order: **1** (highest priority — no production code can be written without error handling).

## Goal

Introduce a first-class error handling mechanism into NG, enabling robust fault-tolerant programming without crashing on every I/O error.

## Motivation

Currently, NG has **no user-facing error handling**. The only choice is `assert()`, which terminates the program.

---

## Phase 1: `Result<T, E>` + `?` Operator (4 weeks)

### 1. Type Definition

Add to prelude:

```ng
export type Result<T, E> = Ok(value: T) | Err(error: E);
```

This reuses the existing tagged union infrastructure. No new type system concepts needed.

### 2. Lexer Changes

Add a new token `QUESTION = 0x0B00` for the `?` operator.

In `include/token.hpp`:
```cpp
enum class TokenType : uint32_t {
    // ... existing tokens ...
    QUESTION_COLON,    // ?: — for future ternary
    QUESTION = 0x0B00, // ?  — error propagation
};
```

In `src/parsing/Lexer.cpp`, add `?` to the symbol/operator lexing path. `?` following an expression (not preceded by whitespace on the left) becomes a postfix operator.

### 3. AST Changes

**New AST node** in `include/ast.hpp`:

```cpp
struct ErrorPropagationExpression : Expression {
    ASTRef<Expression> expr;  // The expression being unwrapped

    explicit ErrorPropagationExpression(SourcePosition pos, ASTRef<Expression> expr)
        : Expression(ASTNodeType::ERROR_PROPAGATION, std::move(pos)), expr(std::move(expr)) {}

    void accept(AstVisitor *visitor) const override { visitor->visit(this); }
};
```

New ASTNodeType enum value:
```cpp
ERROR_PROPAGATION,
```

### 4. Parser Changes

In `src/parsing/ParserImpl.cpp`, modify postfix expression parsing:

```cpp
// In the postfix expression handler, after parsing the primary expression:
if (accept(TokenType::QUESTION)) {
    result = make_node<ErrorPropagationExpression>(pos, std::move(result));
}
```

Place this **before** function call parsing so `foo()?` correctly parses as `(foo())?`.

### 5. Type Checker Changes

In `src/typecheck/typecheck.cpp`, add a visitor for `ErrorPropagationExpression`:

```cpp
void TypeChecker::visit(const ErrorPropagationExpression *node) {
    node->expr->accept(this);
    auto resultType = getResultType(latestType);

    if (!resultType) {
        throw TypeCheckingException("`?` can only be used on Result<T, E> types", node->pos);
    }

    // The type of `expr?` is T (the Ok variant's payload)
    latestType = resultType->okType;

    // The enclosing function must return Result<T, E>
    // This is checked by the return type of the containing function
}
```

Helper:

```cpp
// Returns {okType, errType} if latestType is Result<T,E>, nullopt otherwise
struct ResultTypeInfo {
    CheckingRef<TypeInfo> okType;
    CheckingRef<TypeInfo> errType;
};
auto getResultType(CheckingRef<TypeInfo> type) -> std::optional<ResultTypeInfo>;
```

**Validation rules:**
1. The expression inside `?` must have type `Result<T, E>`
2. The enclosing function's return type must be `Result<U, F>` where `T` unifies with `U`
3. `E` must unify with `F`
4. `?` cannot appear in a function returning `unit` or a non-Result type

### 6. AST Desugaring (Type Checker)

The type checker desugars `expr?` to:

```ng
val __result = expr;
switch (__result) {
    case Ok(__v) { __v }        // fall through value
    case Err(__e) { return Err(__e); }  // early return
}
```

This means the ORGASM compiler doesn't need new opcodes for Phase 1 — it compiles the desugared form.

### 7. ORGASM Compiler Changes

No new opcodes needed in Phase 1. The compiler emits the desugared switch statement:

```
// expr? desugars to:
//   val __result = expr;
//   switch (__result) { case Ok(v) → v; case Err(e) → return Err(e); }

// Compiled bytecode:
CALL function             // push expr result
STORE_LOCAL __result      // store in __result
LOAD_LOCAL __result       // load for switch
SWITCH_TAG 2              // 2 variants
  tag 0: JUMP ok_label    // Ok variant
  tag 1: JUMP err_label   // Err variant
ok_label:
  GET_PAYLOAD 0            // extract Ok payload (value)
  JUMP done_label
err_label:
  GET_PAYLOAD 0            // extract Err payload (error)
  MAKE_VARIANT Err 1       // wrap in Err(...)
  RETURN                   // early return
done_label:
```

### 8. `Result<T, E>` Standard Library

Create `lib/std/result.ng`:

```ng
module std.result exports *;

export type Result<T, E> = Ok(value: T) | Err(error: E);

export fun isOk<T, E>(result: Result<T, E>) -> bool {
    switch (result) { case Ok(_) { return true; } default { return false; } }
}

export fun isErr<T, E>(result: Result<T, E>) -> bool {
    switch (result) { case Err(_) { return true; } default { return false; } }
}

export fun unwrap<T, E>(result: Result<T, E>) -> T {
    switch (result) {
        case Ok(v) { return v; }
        case Err(e) { assert(false, "unwrap failed"); }
    }
}

export fun unwrapOr<T, E>(result: Result<T, E>, fallback: T) -> T {
    switch (result) {
        case Ok(v) { return v; }
        case Err(_) { return fallback; }
    }
}

export fun map<T, U, E>(result: Result<T, E>, f: (T) -> U) -> Result<U, E> {
    switch (result) {
        case Ok(v) { return Ok(f(v)); }
        case Err(e) { return Err(e); }
    }
}
```

### 9. Migration: Stdlib I/O Functions

Current (`std.io`):
```ng
fun readFile(path: string) -> string = native;   // crashes on error
```

After:
```ng
type IOError {
    message: string;
    code: i32;
}

fun readFile(path: string) -> Result<string, IOError> = native;
```

Native C++ implementation change (in `src/stdlib/prelude.cpp`):
```cpp
// Before:
REGISTER_NATIVE("readFile", [](const std::string &path) -> std::string {
    std::ifstream file(path);
    if (!file) throw RuntimeException("File not found: " + path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
});

// After:
REGISTER_NATIVE("readFile", [](const std::string &path) -> ResultValue {
    std::ifstream file(path);
    if (!file) {
        return ResultValue::Err(/* IOError{path + ": not found", errno} */);
    }
    std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return ResultValue::Ok(content);
});
```

### Phase 1 Acceptance Criteria

- `Result<T, E>` tagged union compiles and pattern-matches correctly
- `expr?` correctly unwraps `Ok` and returns early on `Err`
- `?` in a non-Result-returning function is a compile error
- `readFile("nonexistent")` returns `Err` instead of crashing
- Existing code using `readFile` without `?` still compiles (requires manual migration)
- All existing tests pass
- `example/` contains a working error handling example

### Phase 1 Effort Estimate

| Component | Effort | Who |
|---|---|---|
| Lexer/Token changes | 0.5 day | Anyone |
| AST node + Parser | 1 day | Anyone |
| Type checker (`?` validation) | 2 days | Type system expert |
| Desugaring in compiler/STUPID | 1 day | Compiler expert |
| `std.result` module | 0.5 day | Anyone |
| Migrate `readFile`/`writeFile` | 1 day | Stdlib expert |
| Tests | 2 days | Anyone |
| **Total** | **8 days** | 1 developer |

---

## Phase 2: `try`/`catch`/`throw` (2-3 weeks, deferred)

### When

After Phase 1 and real-world usage identifies the need.

### 1. New Keywords

Add to lexer: `KEYWORD_TRY`, `KEYWORD_CATCH`, `KEYWORD_THROW`.

### 2. AST Nodes

```cpp
struct TryStatement : Statement {
    ASTRef<Statement> body;
    Vec<CatchClause> catches;

    // accept visitor
};

struct CatchClause : ASTNode {
    ASTRef<TypeAnnotation> errorType;
    Str bindingName;         // variable name in the catch block
    ASTRef<Statement> handler;
};

struct ThrowStatement : Statement {
    ASTRef<Expression> expr;  // the error value to throw
};
```

### 3. Desugaring

```ng
try {
    riskyOperation()?;
} catch (e: IOError) {
    handleIOError(e);
}
```

Desugars to:

```ng
val __result = riskyOperation();
switch (__result) {
    case Ok(__v) { __v; }
    case Err(__e) {
        if (__e is IOError) {
            val e = __e as IOError;
            handleIOError(e);
        } else {
            return Err(__e);  // re-propagate unhandled errors
        }
    }
}
```

### Phase 2 Acceptance Criteria

- `try { ... } catch (e: Type) { ... }` catches typed errors
- `throw expr` creates an error and returns `Err(expr)` from the function
- Uncaught errors in `try` propagate via `?` semantics
- `catch` supports multiple error types with different handlers
- The last `catch` may be `catch (e) { ... }` as a catch-all

---

## Design Decisions

### Why `Result<T,E>` is a Tagged Union, Not Built-in Syntax

Using the existing tagged union (`Ok(v) | Err(e)`) means:
- No new parser complexity for type definitions
- Pattern matching works without changes
- Type inference works without changes
- The ORGASM VM already supports tagged unions

### Why `try`/`catch` Uses Destructuring, Not Exception-Based

NG avoids stack-unwinding exceptions. `try`/`catch` is syntactic sugar over `Result` + `switch`. This means:
- No runtime overhead when errors don't occur
- Clean stack traces (no unwinding)
- All error paths are visible in the type system

### Scope

**Phase 1 in scope:**
- `Result<T, E>` in stdlib
- `?` operator with parser, type checker, and desugaring
- Migration of `readFile`/`writeFile` to return `Result`
- `std.result` helper functions

**Phase 2 in scope:**
- `try`/`catch`/`throw` syntax and desugaring
- Multi-catch with type dispatch
- Catch-all clause with `catch (e) { ... }`

**Out of scope (both phases):**
- Stack traces / backtrace capture (defer to debugger)
- `finally` (RAII/Drop covers cleanup)
- Checked exceptions (Result types cover this)
- `noexcept` / `throws` annotations