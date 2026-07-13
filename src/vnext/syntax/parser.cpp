// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/parser.hpp"

#include <cctype>
#include <format>
#include <utility>

namespace NG::vnext::syntax
{
  namespace
  {
    [[nodiscard]] auto isIdentifierStart(char character) -> bool
    {
      return std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_';
    }

    [[nodiscard]] auto isIdentifierContinue(char character) -> bool
    {
      return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
    }

    [[nodiscard]] auto tokenKindForSingleCharacter(char character) -> TokenKind
    {
      switch (character)
      {
      case '(': return TokenKind::LeftParen;
      case ')': return TokenKind::RightParen;
      case ',': return TokenKind::Comma;
      case '{': return TokenKind::LeftBrace;
      case '}': return TokenKind::RightBrace;
      case '=': return TokenKind::Equal;
      case ';': return TokenKind::Semicolon;
      case '+': return TokenKind::Plus;
      case '-': return TokenKind::Minus;
      case '*': return TokenKind::Star;
      case '/': return TokenKind::Slash;
      case '%': return TokenKind::Percent;
      case '<': return TokenKind::Less;
      case '>': return TokenKind::Greater;
      case '&': return TokenKind::Ampersand;
      case '^': return TokenKind::Caret;
      case '|': return TokenKind::Pipe;
      case '!': return TokenKind::Bang;
      default:  return TokenKind::End;
      }
    }

    [[nodiscard]] auto tokenKindForTwoCharacters(std::string_view text) -> TokenKind
    {
      if (text == "<<") return TokenKind::ShiftLeft;
      if (text == ">>") return TokenKind::ShiftRight;
      if (text == "<=") return TokenKind::LessEqual;
      if (text == ">=") return TokenKind::GreaterEqual;
      if (text == "==") return TokenKind::EqualEqual;
      if (text == "!=") return TokenKind::NotEqual;
      if (text == "&&") return TokenKind::AndAnd;
      if (text == "||") return TokenKind::OrOr;
      return TokenKind::End;
    }
  } // namespace

  ParseError::ParseError(std::string message, SourceSpan sourceSpan)
    : std::runtime_error(std::move(message)), sourceSpan_(sourceSpan)
  {
  }

  auto Lexer::lex(std::string_view source) const -> std::vector<Token>
  {
    std::vector<Token> tokens;
    size_t offset{};

    while (offset < source.size())
    {
      const char character = source[offset];
      if (std::isspace(static_cast<unsigned char>(character)) != 0)
      {
        ++offset;
        continue;
      }

      const size_t begin = offset;
      if (isIdentifierStart(character))
      {
        ++offset;
        while (offset < source.size() && isIdentifierContinue(source[offset]))
        {
          ++offset;
        }
        const std::string text{source.substr(begin, offset - begin)};
        const TokenKind kind = text == "fun" ? TokenKind::KeywordFun
                             : text == "let" ? TokenKind::KeywordLet
                             : text == "mut" ? TokenKind::KeywordMut
                                             : TokenKind::Identifier;
        tokens.push_back(Token{.kind = kind, .text = text, .span = SourceSpan{begin, offset}});
        continue;
      }

      if (std::isdigit(static_cast<unsigned char>(character)) != 0)
      {
        ++offset;
        while (offset < source.size() && std::isdigit(static_cast<unsigned char>(source[offset])) != 0)
        {
          ++offset;
        }
        tokens.push_back(Token{.kind = TokenKind::IntegerLiteral,
                               .text = std::string{source.substr(begin, offset - begin)},
                               .span = SourceSpan{begin, offset}});
        continue;
      }

      if (offset + 1 < source.size())
      {
        const auto twoCharacterKind = tokenKindForTwoCharacters(source.substr(offset, 2));
        if (twoCharacterKind != TokenKind::End)
        {
          tokens.push_back(Token{.kind = twoCharacterKind,
                                 .text = std::string{source.substr(offset, 2)},
                                 .span = SourceSpan{offset, offset + 2}});
          offset += 2;
          continue;
        }
      }

      const auto kind = tokenKindForSingleCharacter(character);
      if (kind == TokenKind::End)
      {
        throw ParseError(std::format("unexpected character `{}`", character), SourceSpan{offset, offset + 1});
      }
      tokens.push_back(Token{.kind = kind, .text = std::string{character}, .span = SourceSpan{offset, offset + 1}});
      ++offset;
    }

    tokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{source.size(), source.size()}});
    return tokens;
  }

  ExpressionParser::ExpressionParser(std::vector<Token> tokens) : tokens_(std::move(tokens))
  {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End)
    {
      throw std::invalid_argument("vNext expression parser requires an end token");
    }
  }

  auto ExpressionParser::parse() -> ExpressionPtr
  {
    auto expression = parseExpression(0);
    if (!isAtEnd())
    {
      throw ParseError(std::format("unexpected token `{}`", current().text), current().span);
    }
    return expression;
  }

  auto ExpressionParser::parseExpression(int minimumBindingPower) -> ExpressionPtr
  {
    auto left = parsePrefix();

    while (true)
    {
      const Token &operatorToken = current();
      const int bindingPower = infixBindingPower(operatorToken.kind);
      if (bindingPower < minimumBindingPower)
      {
        break;
      }

      static_cast<void>(consume());
      auto right = parseExpression(bindingPower + 1);
      const SourceSpan span{left->span.begin, right->span.end};
      left = std::make_unique<BinaryExpression>(operatorToken.text, std::move(left), std::move(right), span);
    }

    return left;
  }

  auto ExpressionParser::parsePrefix() -> ExpressionPtr
  {
    const Token token = consume();
    switch (token.kind)
    {
    case TokenKind::Identifier:
      return std::make_unique<IdentifierExpression>(token.text, token.span);
    case TokenKind::IntegerLiteral:
      return std::make_unique<IntegerLiteralExpression>(token.text, token.span);
    case TokenKind::LeftParen:
    {
      auto expression = parseExpression(0);
      if (current().kind != TokenKind::RightParen)
      {
        throw ParseError("expected `)`", current().span);
      }
      const Token close = consume();
      expression->span = SourceSpan{token.span.begin, close.span.end};
      return expression;
    }
    default:
      break;
    }

    const int bindingPower = prefixBindingPower(token.kind);
    if (bindingPower < 0)
    {
      throw ParseError(std::format("expected an expression, found `{}`", token.text), token.span);
    }
    auto operand = parseExpression(bindingPower);
    const SourceSpan span{token.span.begin, operand->span.end};
    return std::make_unique<PrefixExpression>(token.text, std::move(operand), span);
  }

  auto ExpressionParser::current() const -> const Token &
  {
    return tokens_[cursor_];
  }

  auto ExpressionParser::consume() -> Token
  {
    const Token token = current();
    if (!isAtEnd())
    {
      ++cursor_;
    }
    return token;
  }

  auto ExpressionParser::prefixBindingPower(TokenKind kind) const -> int
  {
    switch (kind)
    {
    case TokenKind::Plus:
    case TokenKind::Minus:
    case TokenKind::Bang: return 110;
    default:              return -1;
    }
  }

  auto ExpressionParser::infixBindingPower(TokenKind kind) const -> int
  {
    switch (kind)
    {
    case TokenKind::OrOr: return 10;
    case TokenKind::AndAnd: return 20;
    case TokenKind::Pipe: return 30;
    case TokenKind::Caret: return 40;
    case TokenKind::Ampersand: return 50;
    case TokenKind::EqualEqual:
    case TokenKind::NotEqual: return 60;
    case TokenKind::Less:
    case TokenKind::LessEqual:
    case TokenKind::Greater:
    case TokenKind::GreaterEqual: return 70;
    case TokenKind::ShiftLeft:
    case TokenKind::ShiftRight: return 80;
    case TokenKind::Plus:
    case TokenKind::Minus: return 90;
    case TokenKind::Star:
    case TokenKind::Slash:
    case TokenKind::Percent: return 100;
    default: return -1;
    }
  }

  auto ExpressionParser::isAtEnd() const -> bool
  {
    return current().kind == TokenKind::End;
  }

  auto parseExpression(std::string_view source) -> ExpressionPtr
  {
    return ExpressionParser{Lexer{}.lex(source)}.parse();
  }
} // namespace NG::vnext::syntax
