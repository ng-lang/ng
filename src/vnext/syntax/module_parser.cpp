// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/module_parser.hpp"

#include <utility>

namespace NG::vnext::syntax
{
  ModuleParser::ModuleParser(std::vector<Token> tokens) : tokens_(std::move(tokens))
  {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End)
    {
      throw std::invalid_argument("vNext module parser requires an end token");
    }
  }

  auto ModuleParser::parse() -> SourceUnit
  {
    SourceUnit unit{.span = SourceSpan{0, 0}};
    while (current().kind != TokenKind::End)
    {
      if (current().kind != TokenKind::KeywordFun)
      {
        throw ParseError("expected a module declaration", current().span);
      }
      unit.items.push_back(parseFunctionDeclaration());
    }

    unit.span.end = current().span.end;
    if (!unit.items.empty())
    {
      unit.span.begin = unit.items.front()->span.begin;
    }
    return unit;
  }

  auto ModuleParser::parseFunctionDeclaration() -> ModuleItemPtr
  {
    const Token funToken = consume();
    if (current().kind != TokenKind::Identifier)
    {
      throw ParseError("expected a function name after `fun`", current().span);
    }
    const Token name = consume();
    expect(TokenKind::LeftParen, "expected `(` after function name");
    expect(TokenKind::RightParen, "vNext functions do not yet support parameters");

    auto blockTokens = consumeBlockTokens();
    Block body = BlockParser{std::move(blockTokens)}.parse();
    const SourceSpan span{funToken.span.begin, body.span.end};
    return std::make_unique<FunctionDeclaration>(name.text, std::move(body), span);
  }

  auto ModuleParser::consumeBlockTokens() -> std::vector<Token>
  {
    if (current().kind != TokenKind::LeftBrace)
    {
      throw ParseError("expected a function body block", current().span);
    }

    std::vector<Token> blockTokens;
    size_t depth{};
    do
    {
      const Token token = consume();
      if (token.kind == TokenKind::LeftBrace)
      {
        ++depth;
      }
      else if (token.kind == TokenKind::RightBrace)
      {
        --depth;
      }
      blockTokens.push_back(token);
      if (token.kind == TokenKind::End)
      {
        throw ParseError("expected `}` to close function body", token.span);
      }
    } while (depth != 0);

    const size_t end = blockTokens.back().span.end;
    blockTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{end, end}});
    return blockTokens;
  }

  auto ModuleParser::current() const -> const Token &
  {
    return tokens_[cursor_];
  }

  auto ModuleParser::consume() -> Token
  {
    const Token token = current();
    if (token.kind != TokenKind::End)
    {
      ++cursor_;
    }
    return token;
  }

  void ModuleParser::expect(TokenKind kind, std::string_view message)
  {
    if (current().kind != kind)
    {
      throw ParseError(std::string{message}, current().span);
    }
    static_cast<void>(consume());
  }

  auto parseSourceUnit(std::string_view source) -> SourceUnit
  {
    return ModuleParser{Lexer{}.lex(source)}.parse();
  }
} // namespace NG::vnext::syntax
