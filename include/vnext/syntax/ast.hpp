// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace NG::vnext::syntax
{
  struct SourceSpan
  {
    size_t begin{};
    size_t end{};
  };

  enum class ExpressionKind
  {
    Identifier,
    IntegerLiteral,
    Prefix,
    Binary,
  };

  struct Expression
  {
    ExpressionKind kind;
    SourceSpan span;

    explicit Expression(ExpressionKind expressionKind, SourceSpan sourceSpan)
      : kind(expressionKind), span(sourceSpan)
    {
    }
    virtual ~Expression() = default;
  };

  using ExpressionPtr = std::unique_ptr<Expression>;

  struct IdentifierExpression final : Expression
  {
    std::string name;

    IdentifierExpression(std::string identifier, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Identifier, sourceSpan), name(std::move(identifier))
    {
    }
  };

  struct IntegerLiteralExpression final : Expression
  {
    std::string text;

    IntegerLiteralExpression(std::string literalText, SourceSpan sourceSpan)
      : Expression(ExpressionKind::IntegerLiteral, sourceSpan), text(std::move(literalText))
    {
    }
  };

  struct PrefixExpression final : Expression
  {
    std::string operatorText;
    ExpressionPtr operand;

    PrefixExpression(std::string op, ExpressionPtr value, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Prefix, sourceSpan), operatorText(std::move(op)), operand(std::move(value))
    {
    }
  };

  struct BinaryExpression final : Expression
  {
    std::string operatorText;
    ExpressionPtr left;
    ExpressionPtr right;

    BinaryExpression(std::string op, ExpressionPtr lhs, ExpressionPtr rhs, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Binary, sourceSpan), operatorText(std::move(op)), left(std::move(lhs)),
        right(std::move(rhs))
    {
    }
  };
} // namespace NG::vnext::syntax
