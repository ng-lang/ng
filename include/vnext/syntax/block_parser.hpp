// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/syntax/parser.hpp"

namespace NG::vnext::syntax
{
  /// Parses a lexical block. Declarations deliberately do not enter this
  /// grammar: `let` is a block statement and module declarations are parsed by
  /// the future ModuleParser boundary.
  class BlockParser final
  {
  public:
    explicit BlockParser(std::vector<Token> tokens);

    [[nodiscard]] auto parse() -> Block;

  private:
    [[nodiscard]] auto parseLetStatement() -> StatementPtr;
    [[nodiscard]] auto parseAssignStatement() -> StatementPtr;
    [[nodiscard]] auto parseReturnStatement() -> StatementPtr;
    [[nodiscard]] auto parseIfStatement() -> StatementPtr;
    [[nodiscard]] auto parseLoopStatement() -> StatementPtr;
    [[nodiscard]] auto parseNextStatement() -> StatementPtr;
    [[nodiscard]] auto parseNestedBlock() -> Block;
    [[nodiscard]] auto parseExpressionUntil(TokenKind terminator) -> ExpressionPtr;
    [[nodiscard]] auto parseExpressionUntilAny(const std::vector<TokenKind> &terminators) -> ExpressionPtr;
    [[nodiscard]] auto current() const -> const Token &;
    [[nodiscard]] auto peek(size_t offset) const -> const Token &;
    [[nodiscard]] auto consume() -> Token;
    void expect(TokenKind kind, std::string_view message);

    std::vector<Token> tokens_;
    size_t cursor_{};
  };

  [[nodiscard]] auto parseBlock(std::string_view source) -> Block;
} // namespace NG::vnext::syntax
