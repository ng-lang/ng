# NG Internals

This document provides a detailed overview of the internal implementation of the NG programming language.

## 1. Compiler Pipeline

The NG compiler follows a traditional pipeline to process source code and execute it.

1.  **Lexical Analysis:** The source code is scanned and converted into a stream of tokens.
2.  **Parsing:** The token stream is parsed to build an Abstract Syntax Tree (AST).
3.  **Type Checking:** The AST is traversed to perform type checking and inference.
4.  **Interpretation:** The AST is directly executed by the interpreter.

## 2. Lexer

The lexer is responsible for converting the source code into a sequence of tokens. It is implemented in `src/parsing/Lexer.cpp`.

The lexer maintains a `LexState` struct, which keeps track of the current position in the source code.

```cpp
struct LexState
{
    Str source;
    size_t size;
    size_t index;
    size_t line;
    size_t col;
    // ...
};
```

The `Lexer::lex()` method iterates through the source code and produces a `std::vector<Token>`.

## 3. Parser

The parser takes the token stream from the lexer and builds an AST. The parser is implemented in `src/parsing/ParserImpl.cpp`.

It uses a recursive descent parsing strategy to parse the language grammar. The `ParserImpl` class contains methods for parsing different parts of the grammar, such as `funDef()`, `statement()`, and `expression()`.

## 4. Abstract Syntax Tree (AST)

The AST is a tree representation of the source code. The base class for all AST nodes is `ASTNode`, defined in `include/ast.hpp`.

```cpp
struct ASTNode : NonCopyable
{
    virtual void accept(AstVisitor *visitor) = 0;
    virtual auto astNodeType() const -> ASTNodeType = 0;
    // ...
};
```

NG uses the visitor pattern to traverse the AST. The `AstVisitor` interface defines a `visit` method for each type of AST node.

## 5. Type Checker

The type checker traverses the AST and verifies that the program is well-typed. The type checker is implemented in `src/typecheck/typecheck.cpp`.

It uses a `TypeChecker` class, which is an `AstVisitor`, to visit each node in the AST and infer its type. The type information is stored in a `TypeIndex`, which is a map from variable names to `TypeInfo` objects.

## 6. Interpreter

The interpreter executes the AST directly. The main interpreter logic is in `src/intp/stupid.cpp`.

The `Interpreter` class is also an `AstVisitor`. It traverses the AST and executes the code for each node.

### Runtime Environment

The runtime environment consists of the following components:

*   **`NGContext`:** Represents the execution context, which includes the call stack, local variables, and the current module.
*   **`NGObject`:** The base class for all runtime objects.
*   **`NGType`:** Represents a type in the runtime.
*   **`NGModule`:** Represents a module in the runtime.

### Memory Management

NG uses `std::shared_ptr` for automatic memory management of runtime objects. This means that memory is automatically deallocated when an object is no longer referenced.

## 7. Foreign Function Interface (FFI)

NG provides a simple FFI to call native C++ functions from NG code. Native functions are declared using the `= native` syntax.

```ng
fun my_native_function(arg: i32) -> unit = native;
```

These native functions must be registered with the interpreter using the `register_native_library` function.