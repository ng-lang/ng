// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/type_parser.hpp"

#include <utility>

namespace NG::vnext::syntax
{
  TypeParser::TypeParser(std::vector<Token> tokens) : tokens_(std::move(tokens))
  {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End)
      throw std::invalid_argument("vNext type parser requires an end token");
  }

  auto TypeParser::parse() -> TypeSyntaxPtr
  {
    TypeSyntaxPtr type = parsePrimary();
    while (current().kind != TokenKind::End)
    {
      if (current().kind == TokenKind::KeywordRef)
      {
        static_cast<void>(consume());
        const bool isMutable = current().kind == TokenKind::KeywordMut;
        if (isMutable) static_cast<void>(consume());
        type = std::make_unique<ScopedReferenceTypeSyntax>(std::move(type), isMutable,
                                                            SourceSpan{type->span.begin, tokens_[cursor_ - 1].span.end});
        continue;
      }
      if (current().kind == TokenKind::Star)
      {
        static_cast<void>(consume());
        bool isMutable{};
        if (current().kind == TokenKind::KeywordConst) static_cast<void>(consume());
        else if (current().kind == TokenKind::KeywordMut) { isMutable = true; static_cast<void>(consume()); }
        else throw ParseError("expected `const` or `mut` after `*` in raw pointer type", current().span);
        type = std::make_unique<RawPointerTypeSyntax>(std::move(type), isMutable,
                                                       SourceSpan{type->span.begin, tokens_[cursor_ - 1].span.end});
        continue;
      }
      throw ParseError("unexpected token in type", current().span);
    }
    return type;
  }

  auto TypeParser::parsePrimary() -> TypeSyntaxPtr
  {
    if (current().kind != TokenKind::Identifier) throw ParseError("expected a type name", current().span);
    const Token name = consume();
    TypeSyntaxPtr type = std::make_unique<NamedTypeSyntax>(name.text, name.span);
    if (current().kind != TokenKind::Less) return type;

    static_cast<void>(consume());
    std::vector<GenericArgumentSyntax> arguments;
    while (true)
    {
      if (current().kind == TokenKind::IntegerLiteral)
      {
        const Token value = consume();
        arguments.push_back(GenericArgumentSyntax{.kind = GenericArgumentKind::ConstInteger,
                                                   .type = nullptr,
                                                   .text = value.text,
                                                   .span = value.span});
      }
      else
      {
        auto argument = parsePrimary();
        const SourceSpan span = argument->span;
        arguments.push_back(GenericArgumentSyntax{.kind = GenericArgumentKind::Type,
                                                   .type = std::move(argument),
                                                   .text = {},
                                                   .span = span});
      }
      if (current().kind != TokenKind::Comma) break;
      static_cast<void>(consume());
      if (current().kind == TokenKind::Greater) throw ParseError("expected a generic argument after `,`", current().span);
    }
    if (current().kind != TokenKind::Greater) throw ParseError("expected `>` after generic arguments", current().span);
    const Token close = consume();
    return std::make_unique<AppliedTypeSyntax>(std::move(type), std::move(arguments), SourceSpan{name.span.begin, close.span.end});
  }

  auto TypeParser::current() const -> const Token & { return tokens_[cursor_]; }
  auto TypeParser::consume() -> Token
  {
    const Token token = current();
    if (token.kind != TokenKind::End) ++cursor_;
    return token;
  }
} // namespace NG::vnext::syntax
