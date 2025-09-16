# Copilot Instructions for the NG Codebase

## Project Overview
- **NG** is a statically-typed, multi-paradigm programming language implemented in modern C++ (C++23).
- The codebase is organized as a traditional compiler pipeline: **Lexer → Parser → AST → Type Checker → Interpreter**.
- Major components are in `src/` (core logic), `include/` (headers), `test/` (unit/integration tests), and `example/` (sample NG programs).
- The language and its internals are documented in `docs/guide/language_guide.md` and `docs/ref/Internals.md`.

## Key Architecture & Patterns
- **Lexer:** `src/parsing/Lexer.cpp` and `LexState` struct. Produces tokens from source code.
- **Parser:** `src/parsing/ParserImpl.cpp` (recursive descent, see `ParserImpl` class). Builds AST nodes defined in `include/ast.hpp`.
- **AST:** Visitor pattern (`AstVisitor`), base class `ASTNode`. See `include/ast.hpp` and `src/ast/`.
- **Type Checking:** `src/typecheck/` traverses AST for type inference and validation.
- **Interpreter:** `src/intp/` executes the AST directly.
- **Modules:** Each `.ng` file is a module. Use `export`/`import` for visibility (see `docs/guide/language_guide.md`).
- **Standard Library:** Minimal, in `lib/std.ng` and `lib/std/`.

## Developer Workflows
- **Build:**
  ```sh
  mkdir build && cd build
  cmake -GNinja ..
  ninja
  ```
- **Run Interpreter:**
  ```sh
  ./ngi ../example/01.id.ng
  ```
- **Run Tests:**
  ```sh
  ./ng_test
  ```
- **Coverage Report:**
  ```sh
  ./utils/coverage_report.sh
  # Output in build/reports/cov/
  ```
- **Format C++ Code:** Use `clang-format` (see `CONTRIBUTING.md`).
- **Test Patterns:** All tests use Catch2 (`test/`), e.g. `REQUIRE(ast != nullptr);`.

## Project Conventions
- **AI-generated code** must be marked as such (see `CONTRIBUTING.md`).
- **C++ style:** Enforced with `clang-format` and `clang-tidy`.
- **NG code style:** Follow examples in `example/`.
- **External dependencies:** Vendored in `build/_deps/` and `vendored/` (e.g., Catch2, SDL, imgui).
- **Do not modify** files in `vendored/` or `build/_deps/`.

## References
- [Language Guide](../docs/guide/language_guide.md)
- [Internals](../docs/ref/Internals.md)
- [Contribution Guide](../CONTRIBUTING.md)
- [README](../README.md)

---
For any unclear conventions or missing documentation, consult the above references or ask for clarification in the project discussions.
