// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "syntax/parser.hpp"

namespace NG::syntax
{
  /// Parses structural type syntax. Semantic type identity is assigned later
  /// by the TypeInterner; this parser never encodes applied types as names.
  class TypeParser final
  {
  public:
    explicit TypeParser(std::vector<Token> tokens);

    [[nodiscard]] auto parse() -> TypeSyntaxPtr;

  private:
    [[nodiscard]] auto parsePrimary() -> TypeSyntaxPtr;
    [[nodiscard]] auto isConstArgumentStart() const -> bool;
    [[nodiscard]] auto current() const -> const Token &;
    [[nodiscard]] auto consume() -> Token;

    std::vector<Token> tokens_;
    size_t cursor_{};
  };
} // namespace NG::syntax
