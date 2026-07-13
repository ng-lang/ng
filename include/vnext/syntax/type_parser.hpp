// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/syntax/parser.hpp"

namespace NG::vnext::syntax
{
  /// Parses canonical postfix type forms. The first slice deliberately keeps
  /// named types nominal and delegates generic/tuple/array forms to later R2
  /// slices.
  class TypeParser final
  {
  public:
    explicit TypeParser(std::vector<Token> tokens);

    [[nodiscard]] auto parse() -> TypeSyntaxPtr;

  private:
    [[nodiscard]] auto current() const -> const Token &;
    [[nodiscard]] auto consume() -> Token;

    std::vector<Token> tokens_;
    size_t cursor_{};
  };
} // namespace NG::vnext::syntax
