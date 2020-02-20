# Steps to add new syntax:

- Check if requires new keyword:
    - `include/token.hpp` for token types
    - `src/lexer.cpp` for token represesntations
    - `src/reserved.inc` for reserved tokens
- Check if requires new AST:
    - `include/ast.hpp` for new AST type
    - `src/ast/ast.cpp` for method implementations, especially `accept()` and `~T()`
    - `include/ast.hpp` for new AST visitor method
    - `src/visitor.cpp` for implement AST visitor method
    - `src/ast/serializer` for AST type serialize/deserialize

Steps:

  1. Create new token type
  2. Add lexer method to lex the token
  3. Check if reserved tokens were used and update
  4. Add lexer test
  4. Add new AST type
  5. Implement the parser
  6. Add parser test
  7. Add new visitor method for the AST type
  8. Implement PrettyPrinter and Serializer
  9. Update examples for the new syntax
  10. Check PrettyPrinter output
  11. Update integration test to include new syntax example file
