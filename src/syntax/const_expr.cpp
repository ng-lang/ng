// AI-generated code; reviewed for this repository's vNext rewrite.
#include "syntax/const_expr.hpp"

#include <format>
#include <utility>

namespace NG::syntax
{
  ConstExprParser::ConstExprParser(std::vector<Token> tokens, size_t startCursor, bool greaterTerminates)
    : tokens_(std::move(tokens)), cursor_(startCursor), greaterTerminates_(greaterTerminates)
  {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End)
    {
      throw std::invalid_argument("vNext const expression parser requires an end token");
    }
  }

  auto ConstExprParser::parse() -> ConstExprPtr
  {
    return parseLogical();
  }

  auto ConstExprParser::parseLogical() -> ConstExprPtr
  {
    ConstExprPtr expression = parseComparison();
    while (current().kind == TokenKind::AndAnd || current().kind == TokenKind::OrOr)
    {
      const Token op = consume();
      ConstExprPtr right = parseComparison();
      expression = std::make_unique<ConstBinaryExpr>(op.text, std::move(expression), std::move(right),
                                                     SourceSpan{expression->span.begin, right->span.end});
    }
    return expression;
  }

  auto ConstExprParser::parseComparison() -> ConstExprPtr
  {
    ConstExprPtr expression = parseAdditive();
    while (current().kind == TokenKind::EqualEqual || current().kind == TokenKind::NotEqual ||
           current().kind == TokenKind::Less || current().kind == TokenKind::LessEqual ||
           (!greaterTerminates_ &&
            (current().kind == TokenKind::Greater || current().kind == TokenKind::GreaterEqual)))
    {
      const Token op = consume();
      ConstExprPtr right = parseAdditive();
      expression = std::make_unique<ConstBinaryExpr>(op.text, std::move(expression), std::move(right),
                                                     SourceSpan{expression->span.begin, right->span.end});
    }
    return expression;
  }

  auto ConstExprParser::parseAdditive() -> ConstExprPtr
  {
    ConstExprPtr expression = parseMultiplicative();
    while (current().kind == TokenKind::Plus || current().kind == TokenKind::Minus)
    {
      const Token op = consume();
      ConstExprPtr right = parseMultiplicative();
      expression = std::make_unique<ConstBinaryExpr>(op.text, std::move(expression), std::move(right),
                                                     SourceSpan{expression->span.begin, right->span.end});
    }
    return expression;
  }

  auto ConstExprParser::parseMultiplicative() -> ConstExprPtr
  {
    ConstExprPtr expression = parseUnary();
    while (current().kind == TokenKind::Star || current().kind == TokenKind::Slash || current().kind == TokenKind::Percent)
    {
      const Token op = consume();
      ConstExprPtr right = parseUnary();
      expression = std::make_unique<ConstBinaryExpr>(op.text, std::move(expression), std::move(right),
                                                     SourceSpan{expression->span.begin, right->span.end});
    }
    return expression;
  }

  auto ConstExprParser::parseUnary() -> ConstExprPtr
  {
    if (current().kind == TokenKind::Plus || current().kind == TokenKind::Minus || current().kind == TokenKind::Bang)
    {
      const Token op = consume();
      ConstExprPtr operand = parseUnary();
      return std::make_unique<ConstUnaryExpr>(op.text, std::move(operand), SourceSpan{op.span.begin, operand->span.end});
    }
    return parsePrimary();
  }

  auto ConstExprParser::parsePrimary() -> ConstExprPtr
  {
    const Token token = current();
    if (token.kind == TokenKind::IntegerLiteral)
    {
      static_cast<void>(consume());
      return std::make_unique<ConstIntegerLiteral>(token.text, token.span);
    }
    if (token.kind == TokenKind::KeywordTrue || token.kind == TokenKind::KeywordFalse)
    {
      static_cast<void>(consume());
      return std::make_unique<ConstBoolLiteral>(token.kind == TokenKind::KeywordTrue, token.span);
    }
    if (token.kind == TokenKind::Identifier)
    {
      static_cast<void>(consume());
      return std::make_unique<ConstIdentifier>(token.text, token.span);
    }
    if (token.kind == TokenKind::LeftParen)
    {
      static_cast<void>(consume());
      ConstExprPtr inner = parseLogical();
      if (current().kind != TokenKind::RightParen)
      {
        throw ParseError("expected `)` after const expression", current().span);
      }
      static_cast<void>(consume());
      return inner;
    }
    throw ParseError(std::format("expected a const expression, found `{}`", token.text), token.span);
  }

  auto ConstExprParser::current() const -> const Token & { return tokens_[cursor_]; }

  auto ConstExprParser::consume() -> Token
  {
    const Token token = current();
    if (token.kind != TokenKind::End) ++cursor_;
    return token;
  }

  namespace
  {
    [[nodiscard]] auto clone(const ConstExpr &expression) -> ConstExprPtr
    {
      if (const auto *literal = dynamic_cast<const ConstIntegerLiteral *>(&expression))
        return std::make_unique<ConstIntegerLiteral>(literal->text, literal->span);
      if (const auto *boolean = dynamic_cast<const ConstBoolLiteral *>(&expression))
        return std::make_unique<ConstBoolLiteral>(boolean->value, boolean->span);
      if (const auto *identifier = dynamic_cast<const ConstIdentifier *>(&expression))
        return std::make_unique<ConstIdentifier>(identifier->name, identifier->span);
      if (const auto *unary = dynamic_cast<const ConstUnaryExpr *>(&expression))
        return std::make_unique<ConstUnaryExpr>(unary->operatorText, clone(*unary->operand), unary->span);
      if (const auto *binary = dynamic_cast<const ConstBinaryExpr *>(&expression))
        return std::make_unique<ConstBinaryExpr>(binary->operatorText, clone(*binary->left), clone(*binary->right), binary->span);
      throw std::logic_error("unknown const expression node");
    }
  } // namespace

  auto cloneConstExpr(const ConstExpr &expression) -> ConstExprPtr { return clone(expression); }

  auto renderConstExpr(const ConstExpr &expression) -> std::string
  {
    if (const auto *literal = dynamic_cast<const ConstIntegerLiteral *>(&expression)) return literal->text;
    if (const auto *boolean = dynamic_cast<const ConstBoolLiteral *>(&expression)) return boolean->value ? "true" : "false";
    if (const auto *identifier = dynamic_cast<const ConstIdentifier *>(&expression)) return identifier->name;
    if (const auto *unary = dynamic_cast<const ConstUnaryExpr *>(&expression))
      return unary->operatorText + renderConstExpr(*unary->operand);
    if (const auto *binary = dynamic_cast<const ConstBinaryExpr *>(&expression))
      return std::format("({} {} {})", renderConstExpr(*binary->left), binary->operatorText, renderConstExpr(*binary->right));
    throw std::logic_error("unknown const expression node");
  }
} // namespace NG::syntax
