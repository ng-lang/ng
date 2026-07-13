// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/type_parser.hpp"

#include <utility>

namespace NG::vnext::syntax
{
  TypeParser::TypeParser(std::vector<Token> tokens) : tokens_(std::move(tokens))
  {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End)
    {
      throw std::invalid_argument("vNext type parser requires an end token");
    }
  }

  auto TypeParser::parse() -> TypeSyntaxPtr
  {
    if (current().kind != TokenKind::Identifier)
    {
      throw ParseError("expected a type name", current().span);
    }

    const Token name = consume();
    TypeSyntaxPtr type = std::make_unique<NamedTypeSyntax>(name.text, name.span);
    while (current().kind != TokenKind::End)
    {
      if (current().kind == TokenKind::KeywordRef)
      {
        static_cast<void>(consume());
        const bool isMutable = current().kind == TokenKind::KeywordMut;
        if (isMutable)
        {
          static_cast<void>(consume());
        }
        const SourceSpan span{type->span.begin, tokens_[cursor_ - 1].span.end};
        type = std::make_unique<ScopedReferenceTypeSyntax>(std::move(type), isMutable, span);
        continue;
      }

      if (current().kind == TokenKind::Star)
      {
        static_cast<void>(consume());
        bool isMutable{};
        if (current().kind == TokenKind::KeywordConst)
        {
          static_cast<void>(consume());
        }
        else if (current().kind == TokenKind::KeywordMut)
        {
          isMutable = true;
          static_cast<void>(consume());
        }
        else
        {
          throw ParseError("expected `const` or `mut` after `*` in raw pointer type", current().span);
        }
        const SourceSpan span{type->span.begin, tokens_[cursor_ - 1].span.end};
        type = std::make_unique<RawPointerTypeSyntax>(std::move(type), isMutable, span);
        continue;
      }

      throw ParseError("unexpected token in type", current().span);
    }
    return type;
  }

  auto TypeParser::current() const -> const Token &
  {
    return tokens_[cursor_];
  }

  auto TypeParser::consume() -> Token
  {
    const Token token = current();
    if (token.kind != TokenKind::End)
    {
      ++cursor_;
    }
    return token;
  }
} // namespace NG::vnext::syntax
