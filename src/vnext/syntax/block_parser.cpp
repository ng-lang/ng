// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/block_parser.hpp"

#include <format>
#include <utility>

namespace NG::vnext::syntax
{
  BlockParser::BlockParser(std::vector<Token> tokens) : tokens_(std::move(tokens))
  {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End)
    {
      throw std::invalid_argument("vNext block parser requires an end token");
    }
  }

  auto BlockParser::parse() -> Block
  {
    const Token open = current();
    expect(TokenKind::LeftBrace, "expected `{` to begin a block");

    Block block{.span = open.span};
    while (current().kind != TokenKind::RightBrace)
    {
      if (current().kind == TokenKind::End)
      {
        throw ParseError("expected `}` to close block", current().span);
      }

      if (current().kind == TokenKind::KeywordLet)
      {
        block.statements.push_back(parseLetStatement());
        continue;
      }

      auto expression = parseExpressionUntil(TokenKind::RightBrace);
      if (current().kind == TokenKind::Semicolon)
      {
        const Token semicolon = consume();
        block.statements.push_back(
            std::make_unique<ExpressionStatement>(std::move(expression), SourceSpan{expression->span.begin, semicolon.span.end}));
        continue;
      }

      block.tailExpression = std::move(expression);
      break;
    }

    const Token close = current();
    expect(TokenKind::RightBrace, "expected `}` to close block");
    block.span.end = close.span.end;
    return block;
  }

  auto BlockParser::parseLetStatement() -> StatementPtr
  {
    const Token letToken = consume();
    const bool isMutable = current().kind == TokenKind::KeywordMut;
    if (isMutable)
    {
      static_cast<void>(consume());
    }

    if (current().kind != TokenKind::Identifier)
    {
      throw ParseError("expected a binding name after `let`", current().span);
    }
    const Token name = consume();
    expect(TokenKind::Equal, "expected `=` after let binding name");

    auto initializer = parseExpressionUntil(TokenKind::Semicolon);
    const Token semicolon = current();
    expect(TokenKind::Semicolon, "expected `;` after let initializer");
    return std::make_unique<LetStatement>(name.text, isMutable, std::move(initializer),
                                          SourceSpan{letToken.span.begin, semicolon.span.end});
  }

  auto BlockParser::parseExpressionUntil(TokenKind terminator) -> ExpressionPtr
  {
    std::vector<Token> expressionTokens;
    while (current().kind != terminator && current().kind != TokenKind::Semicolon &&
           current().kind != TokenKind::RightBrace && current().kind != TokenKind::End)
    {
      expressionTokens.push_back(consume());
    }

    if (expressionTokens.empty())
    {
      throw ParseError("expected an expression", current().span);
    }

    const size_t end = expressionTokens.back().span.end;
    expressionTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{end, end}});
    return ExpressionParser{std::move(expressionTokens)}.parse();
  }

  auto BlockParser::current() const -> const Token &
  {
    return tokens_[cursor_];
  }

  auto BlockParser::consume() -> Token
  {
    const Token token = current();
    if (token.kind != TokenKind::End)
    {
      ++cursor_;
    }
    return token;
  }

  void BlockParser::expect(TokenKind kind, std::string_view message)
  {
    if (current().kind != kind)
    {
      throw ParseError(std::string{message}, current().span);
    }
    static_cast<void>(consume());
  }

  auto parseBlock(std::string_view source) -> Block
  {
    return BlockParser{Lexer{}.lex(source)}.parse();
  }
} // namespace NG::vnext::syntax
