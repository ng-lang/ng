// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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

  enum class StatementKind
  {
    Let,
    Expression,
  };

  struct Statement
  {
    StatementKind kind;
    SourceSpan span;

    explicit Statement(StatementKind statementKind, SourceSpan sourceSpan) : kind(statementKind), span(sourceSpan) {}
    virtual ~Statement() = default;
  };

  using StatementPtr = std::unique_ptr<Statement>;

  struct LetStatement final : Statement
  {
    std::string name;
    bool isMutable;
    ExpressionPtr initializer;

    LetStatement(std::string bindingName, bool mutableBinding, ExpressionPtr value, SourceSpan sourceSpan)
      : Statement(StatementKind::Let, sourceSpan), name(std::move(bindingName)), isMutable(mutableBinding),
        initializer(std::move(value))
    {
    }
  };

  struct ExpressionStatement final : Statement
  {
    ExpressionPtr expression;

    ExpressionStatement(ExpressionPtr value, SourceSpan sourceSpan)
      : Statement(StatementKind::Expression, sourceSpan), expression(std::move(value))
    {
    }
  };

  struct Block final
  {
    SourceSpan span;
    std::vector<StatementPtr> statements;
    ExpressionPtr tailExpression;
  };

  enum class ModuleItemKind
  {
    Function,
  };

  struct ModuleItem
  {
    ModuleItemKind kind;
    SourceSpan span;

    explicit ModuleItem(ModuleItemKind itemKind, SourceSpan sourceSpan) : kind(itemKind), span(sourceSpan) {}
    virtual ~ModuleItem() = default;
  };

  using ModuleItemPtr = std::unique_ptr<ModuleItem>;

  struct FunctionDeclaration final : ModuleItem
  {
    std::string name;
    Block body;

    FunctionDeclaration(std::string functionName, Block functionBody, SourceSpan sourceSpan)
      : ModuleItem(ModuleItemKind::Function, sourceSpan), name(std::move(functionName)), body(std::move(functionBody))
    {
    }
  };

  struct SourceUnit final
  {
    SourceSpan span;
    std::vector<ModuleItemPtr> items;
  };
} // namespace NG::vnext::syntax
