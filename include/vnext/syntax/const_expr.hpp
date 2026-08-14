// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/syntax/parser.hpp"

namespace NG::vnext::syntax
{
  /// Parses the restricted integer const-expression grammar used in type-level
  /// positions: integer literals, unary `+`/`-`, checked `+ - * / %`, and
  /// parentheses. Identifiers are preserved for const-parameter references and
  /// are resolved by the semantic const evaluator, never by this parser.
  class ConstExprParser final
  {
  public:
    /// When `greaterTerminates` is set, `>` / `>=` are not parsed as
    /// comparison operators: the caller (a generic-argument list) owns the
    /// closing `>`. Module-level const bodies leave it off.
    explicit ConstExprParser(std::vector<Token> tokens, size_t startCursor = 0, bool greaterTerminates = false);

    [[nodiscard]] auto parse() -> ConstExprPtr;
    [[nodiscard]] auto cursor() const -> size_t { return cursor_; }

  private:
    [[nodiscard]] auto parseComparison() -> ConstExprPtr;
    [[nodiscard]] auto parseAdditive() -> ConstExprPtr;
    [[nodiscard]] auto parseMultiplicative() -> ConstExprPtr;
    [[nodiscard]] auto parseUnary() -> ConstExprPtr;
    [[nodiscard]] auto parsePrimary() -> ConstExprPtr;
    [[nodiscard]] auto current() const -> const Token &;
    [[nodiscard]] auto consume() -> Token;

    std::vector<Token> tokens_;
    size_t cursor_{};
    bool greaterTerminates_{};
  };

  /// Deep-copies a parsed const expression. The HIR owns its lowered const
  /// expressions; syntax nodes are immutable and never shared into semantic
  /// storage by pointer.
  [[nodiscard]] auto cloneConstExpr(const ConstExpr &expression) -> ConstExprPtr;

  /// Renders a const expression back to its source-like spelling. This is
  /// diagnostic output only and is never used to compute identity.
  [[nodiscard]] auto renderConstExpr(const ConstExpr &expression) -> std::string;
} // namespace NG::vnext::syntax
