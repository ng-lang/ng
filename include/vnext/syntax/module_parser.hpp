// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/syntax/block_parser.hpp"

namespace NG::vnext::syntax
{
  /// Parses source-unit items. This parser never accepts a block statement at
  /// module scope; declarations and local statements have distinct boundaries.
  class ModuleParser final
  {
  public:
    explicit ModuleParser(std::vector<Token> tokens);

    [[nodiscard]] auto parse() -> SourceUnit;

  private:
    [[nodiscard]] auto parseFunctionDeclaration() -> ModuleItemPtr;
    [[nodiscard]] auto parseTypeUntil(const std::vector<TokenKind> &terminators) -> TypeSyntaxPtr;
    [[nodiscard]] auto consumeBlockTokens() -> std::vector<Token>;
    [[nodiscard]] auto current() const -> const Token &;
    [[nodiscard]] auto consume() -> Token;
    void expect(TokenKind kind, std::string_view message);

    std::vector<Token> tokens_;
    size_t cursor_{};
  };

  [[nodiscard]] auto parseSourceUnit(std::string_view source) -> SourceUnit;
} // namespace NG::vnext::syntax
