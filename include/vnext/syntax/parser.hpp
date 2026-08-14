// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/syntax/ast.hpp"
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace NG::vnext::syntax
{
  enum class TokenKind
  {
    End,
    Identifier,
    IntegerLiteral,
    StringLiteral,
    KeywordConst,
    KeywordDelete,
    KeywordNative,
    KeywordElse,
    KeywordEnum,
    KeywordFun,
    KeywordIf,
    KeywordLet,
    KeywordLoop,
    KeywordMut,
    KeywordNext,
    KeywordRef,
    KeywordReturn,
    KeywordStruct,
    KeywordSwitch,
    KeywordCase,
    KeywordOtherwise,
    KeywordWhere,
    KeywordIs,
    KeywordTrait,
    KeywordImpl,
    KeywordFor,
    KeywordImport,
    KeywordExport,
    KeywordMove,
    KeywordClone,
    KeywordTrue,
    KeywordFalse,
    LeftParen,
    RightParen,
    LeftSquare,
    RightSquare,
    Comma,
    Dot,
    Colon,
    Assign,
    Arrow,
    FatArrow,
    LeftBrace,
    RightBrace,
    Equal,
    Semicolon,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    ShiftLeft,
    ShiftRight,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    EqualEqual,
    NotEqual,
    Ellipsis,
    DotDot,
    Ampersand,
    Caret,
    Pipe,
    AndAnd,
    OrOr,
    Bang,
  };

  struct Token
  {
    TokenKind kind;
    std::string text;
    SourceSpan span;
  };

  class ParseError : public std::runtime_error
  {
  public:
    ParseError(std::string message, SourceSpan sourceSpan);

    [[nodiscard]] auto span() const noexcept -> SourceSpan { return sourceSpan_; }

  private:
    SourceSpan sourceSpan_;
  };

  class Lexer final
  {
  public:
    [[nodiscard]] auto lex(std::string_view source) const -> std::vector<Token>;
  };

  class ExpressionParser final
  {
  public:
    explicit ExpressionParser(std::vector<Token> tokens);

    [[nodiscard]] auto parse() -> ExpressionPtr;

  private:
    [[nodiscard]] auto parseExpression(int minimumBindingPower) -> ExpressionPtr;
    [[nodiscard]] auto parsePrefix() -> ExpressionPtr;
    [[nodiscard]] auto parsePostfix(ExpressionPtr expression) -> ExpressionPtr;
    [[nodiscard]] auto current() const -> const Token &;
    [[nodiscard]] auto consume() -> Token;
    [[nodiscard]] auto prefixBindingPower(TokenKind kind) const -> int;
    [[nodiscard]] auto infixBindingPower(TokenKind kind) const -> int;
    [[nodiscard]] auto isAtEnd() const -> bool;

    std::vector<Token> tokens_;
    size_t cursor_{};
  };

  [[nodiscard]] auto parseExpression(std::string_view source) -> ExpressionPtr;
} // namespace NG::vnext::syntax
