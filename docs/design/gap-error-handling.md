# Error Handling: `Result<T, E>` And `?` Operator

## Order

Recommended implementation order: **1** (highest priority — the lack of error handling blocks all production use).

## Goal

Introduce a first-class error handling mechanism into NG, enabling robust fault-tolerant programming without crashing on every I/O error.

## Motivation

Currently, NG has **no user-facing error handling**. The only choice is `assert()`, which terminates the program. I/O functions (`readFile`, `writeFile`) crash on failure. There is no way to:
- Gracefully handle file-not-found
- Propagate errors up the call stack
- Distinguish between recoverable and unrecoverable errors

## Proposed Design

### 1. `Result<T, E>` As a Built-in Type

```ng
type Result<T, E> = Ok(value: T) | Err(error: E);
```

- `Option<T>` as a convenience alias: `type Option<T> = Ok(value: T) | Err(unit);`
- Standard library functions return `Result` instead of crashing:

```ng
fun readFile(path: string) -> Result<string, IOError>;
fun writeFile(path: string, content: string) -> Result<unit, IOError>;
```

### 2. `?` Error Propagation Operator

The `?` operator unwraps a `Result`, returning the `Err` variant to the caller on failure:

```ng
fun readConfig(path: string) -> Result<string, IOError> {
    val content = readFile(path)?;   // returns early on Err
    return Ok(process(content));
}
```

Desugars to:

```ng
val _tmp = readFile(path);
val content = switch (_tmp) {
    case Ok(v) { v }
    case Err(e) { return Err(e); }
};
```

### 3. `try`/`catch` Statements

For imperative error handling:

```ng
try {
    val content = readFile("data.txt")?;
    process(content);
} catch (e: IOError) {
    print("IO error:", e.message);
} catch (e: ParseError) {
    print("Parse error:", e.message);
}
```

### 4. `throw` Statement

For manual error creation:

```ng
fun divide(a: i32, b: i32) -> Result<i32, string> {
    if (b == 0) {
        throw "division by zero";
    }
    return Ok(a / b);
}
```

## Dependencies

- Requires tagged union pattern matching (already implemented).
- Unblocks: robust I/O, network libraries, parsing libraries.
- Unblocks: standard library functions returning `Result` instead of crashing.

## Scope

**In scope:**
- `Result<T, E>` as a tagged union in `std.result`
- `?` operator syntax and semantics
- `try`/`catch` statement
- `throw` statement  
- Migrating stdlib I/O to return `Result`
- Generic error type (`E`) dispatch in `catch`

**Out of scope:**
- Stack traces / backtrace capture (defer to debugger)
- `noexcept` / `throws` annotations
- `finally` clause (can be done with RAII/Drop)
- Checked exceptions à la Java

## Acceptance Criteria

- `readFile` returns `Err` on missing file instead of crashing
- `?` correctly propagates `Err` through multiple call levels
- `try`/`catch` catches typed errors
- `throw` creates errors from any expression
- All existing tests continue to pass
- Error handling examples exist in `example/`
- ORGASM VM supports the `?` operator natively (not just STUPID)

## Potential Challenges

- `?` interacts with move semantics: the `Err` value must be movable out of the switch.
- `try`/`catch` scoping rules need careful design to avoid resource leaks.
- ORGASM VM needs new opcodes: `TRY`, `CATCH`, `THROW`, `PROPAGATE`.
- Pre-existing code that assumes `readFile` always succeeds must be updated.
