// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/const_expr.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/syntax/type_parser.hpp"

#include <algorithm>
#include <optional>
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
    std::vector<ModuleItemPtr> items;
    while (current().kind != TokenKind::End)
    {
      if (current().kind == TokenKind::KeywordFun)
      {
        items.push_back(parseFunctionDeclaration());
      }
      else if (current().kind == TokenKind::KeywordStruct)
      {
        items.push_back(parseStructDeclaration());
      }
      else if (current().kind == TokenKind::KeywordEnum)
      {
        items.push_back(parseEnumDeclaration());
      }
      else if (current().kind == TokenKind::KeywordConst)
      {
        if (peek(1).kind == TokenKind::KeywordFun) items.push_back(parseFunctionDeclaration(true));
        else items.push_back(parseConstDeclaration());
      }
      else if (current().kind == TokenKind::KeywordTrait)
      {
        items.push_back(parseTraitDeclaration());
      }
      else if (current().kind == TokenKind::KeywordImpl)
      {
        items.push_back(parseImplDeclaration());
      }
      else
      {
        throw ParseError("expected a module declaration", current().span);
      }
    }

    const size_t begin = items.empty() ? 0 : items.front()->span.begin;
    return SourceUnit{SourceSpan{begin, current().span.end}, std::move(items)};
  }

  auto ModuleParser::parseConstDeclaration() -> ModuleItemPtr
  {
    const Token constToken = consume();
    std::vector<GenericParameter> parameters;
    if (current().kind == TokenKind::Less)
    {
      static_cast<void>(consume());
      while (current().kind != TokenKind::Greater)
      {
        if (current().kind == TokenKind::KeywordConst)
          throw ParseError("const parameters on const declarations are not yet supported", current().span);
        if (current().kind != TokenKind::Identifier)
          throw ParseError("expected a const declaration generic parameter", current().span);
        const Token parameter = consume();
        parameters.push_back(GenericParameter{.kind = GenericParameterKind::Type,
                                              .name = parameter.text,
                                              .type = nullptr,
                                              .span = parameter.span});
        if (current().kind != TokenKind::Comma) break;
        static_cast<void>(consume());
      }
      expect(TokenKind::Greater, "expected `>` after const declaration generic parameters");
    }
    if (current().kind != TokenKind::Identifier) throw ParseError("expected a const declaration name", current().span);
    const Token name = consume();
    std::vector<TypeSyntaxPtr> patterns;
    bool listClosedByShiftRight{};
    if (current().kind == TokenKind::Less)
    {
      static_cast<void>(consume());
      while (true)
      {
        // Collect one pattern-argument chunk. A `>` closes the pattern list
        // only at depth zero, unless it is the inner closer of a nested
        // constructed type written adjacently (`ref<T>>`).
        std::vector<Token> chunk;
        size_t angleDepth{};
        while (true)
        {
          if (current().kind == TokenKind::End)
            throw ParseError("expected `>` after const declaration pattern", current().span);
          if (current().kind == TokenKind::Less)
          {
            ++angleDepth;
          }
          else if (current().kind == TokenKind::Greater)
          {
            if (angleDepth == 0)
            {
              if (peek(1).kind == TokenKind::Greater && peek(1).span.begin == current().span.end)
              {
                chunk.push_back(consume());
                continue;
              }
              break;
            }
            --angleDepth;
          }
          else if (current().kind == TokenKind::ShiftRight)
          {
            // `ref<T>>`: the first `>` closes the pattern argument, the second
            // closes the pattern list.
            const Token token = consume();
            const size_t middle = token.span.begin + 1;
            chunk.push_back(Token{.kind = TokenKind::Greater, .text = ">", .span = SourceSpan{token.span.begin, middle}});
            if (angleDepth <= 1)
            {
              // The second `>` of the shift token closes the pattern list.
              listClosedByShiftRight = true;
              break;
            }
            angleDepth -= 2;
            chunk.push_back(Token{.kind = TokenKind::Greater, .text = ">", .span = SourceSpan{middle, token.span.end}});
            continue;
          }
          else if (angleDepth == 0 && current().kind == TokenKind::Comma)
          {
            break;
          }
          chunk.push_back(consume());
        }
        if (chunk.empty()) throw ParseError("expected a const declaration pattern argument", current().span);
        const size_t chunkEnd = chunk.back().span.end;
        chunk.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{chunkEnd, chunkEnd}});
        patterns.push_back(TypeParser{std::move(chunk)}.parse());
        if (current().kind != TokenKind::Comma) break;
        static_cast<void>(consume());
      }
      if (!listClosedByShiftRight) expect(TokenKind::Greater, "expected `>` after const declaration pattern");
    }
    expect(TokenKind::Colon, "expected `:` after const declaration name");
    auto target = parseTypeUntil({TokenKind::Equal});
    expect(TokenKind::Equal, "expected `=` after const declaration type");

    ConstBodyKind bodyKind{ConstBodyKind::Expression};
    ConstExprPtr body;
    if (current().kind == TokenKind::KeywordNative)
    {
      static_cast<void>(consume());
      bodyKind = ConstBodyKind::Native;
    }
    else if (current().kind == TokenKind::KeywordDelete)
    {
      static_cast<void>(consume());
      bodyKind = ConstBodyKind::Delete;
    }
    else
    {
      std::vector<Token> bodyTokens;
      while (current().kind != TokenKind::Semicolon)
      {
        if (current().kind == TokenKind::End)
          throw ParseError("expected `;` after const declaration body", current().span);
        bodyTokens.push_back(consume());
      }
      if (bodyTokens.empty()) throw ParseError("expected a const declaration body", current().span);
      const size_t end = bodyTokens.back().span.end;
      bodyTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{end, end}});
      body = ConstExprParser{std::move(bodyTokens)}.parse();
    }
    const Token semicolon = current();
    expect(TokenKind::Semicolon, "expected `;` after const declaration");
    return std::make_unique<ConstDeclaration>(name.text, std::move(parameters), std::move(patterns), std::move(target),
                                              bodyKind, std::move(body), SourceSpan{constToken.span.begin, semicolon.span.end});
  }

  auto ModuleParser::parseEnumDeclaration() -> ModuleItemPtr
  {
    const Token enumToken = consume();
    if (current().kind != TokenKind::Identifier) throw ParseError("expected an enum name after `enum`", current().span);
    const Token name = consume();
    std::vector<std::string> genericParameters;
    if (current().kind == TokenKind::Less)
    {
      static_cast<void>(consume());
      while (current().kind != TokenKind::Greater)
      {
        if (current().kind != TokenKind::Identifier) throw ParseError("expected an enum generic parameter", current().span);
        genericParameters.push_back(consume().text);
        if (current().kind != TokenKind::Comma) break;
        static_cast<void>(consume());
      }
      expect(TokenKind::Greater, "expected `>` after enum generic parameters");
    }
    expect(TokenKind::LeftBrace, "expected `{` after enum name");
    std::vector<EnumVariantDeclaration> variants;
    while (current().kind != TokenKind::RightBrace)
    {
      if (current().kind != TokenKind::Identifier) throw ParseError("expected an enum variant name", current().span);
      const Token variant = consume();
      TypeSyntaxPtr payload;
      if (current().kind == TokenKind::LeftParen)
      {
        static_cast<void>(consume());
        if (current().kind == TokenKind::Identifier && peek(1).kind == TokenKind::Colon)
        {
          static_cast<void>(consume());
          static_cast<void>(consume());
        }
        payload = parseTypeUntil({TokenKind::RightParen});
        expect(TokenKind::RightParen, "expected `)` after enum variant payload");
      }
      variants.emplace_back(variant.text, std::move(payload), SourceSpan{variant.span.begin, current().span.begin});
      if (current().kind == TokenKind::Comma) static_cast<void>(consume());
      else if (current().kind != TokenKind::RightBrace) throw ParseError("expected `,` between enum variants", current().span);
    }
    const Token close = consume();
    return std::make_unique<EnumDeclaration>(name.text, std::move(genericParameters), std::move(variants),
                                              SourceSpan{enumToken.span.begin, close.span.end});
  }

  auto ModuleParser::parseStructDeclaration() -> ModuleItemPtr
  {
    const Token structToken = consume();
    if (current().kind != TokenKind::Identifier) throw ParseError("expected a struct name after `struct`", current().span);
    const Token name = consume();
    expect(TokenKind::LeftBrace, "expected `{` after struct name");
    std::vector<StructFieldDeclaration> fields;
    while (current().kind != TokenKind::RightBrace)
    {
      if (current().kind != TokenKind::Identifier) throw ParseError("expected a struct field name", current().span);
      const Token field = consume();
      expect(TokenKind::Colon, "expected `:` after struct field name");
      auto type = parseTypeUntil({TokenKind::Comma, TokenKind::RightBrace});
      fields.emplace_back(field.text, std::move(type), SourceSpan{field.span.begin, fields.empty() ? field.span.end : current().span.begin});
      if (current().kind == TokenKind::Comma) static_cast<void>(consume());
      else if (current().kind != TokenKind::RightBrace) throw ParseError("expected `,` between struct fields", current().span);
    }
    const Token close = consume();
    return std::make_unique<StructDeclaration>(name.text, std::move(fields), SourceSpan{structToken.span.begin, close.span.end});
  }

  auto ModuleParser::parseFunctionDeclaration(bool constFunction) -> ModuleItemPtr
  {
    const Token funToken = consume();
    if (constFunction)
    {
      expect(TokenKind::KeywordFun, "expected `fun` after `const`");
    }
    if (current().kind != TokenKind::Identifier)
    {
      throw ParseError("expected a function name after `fun`", current().span);
    }
    const Token name = consume();
    std::vector<GenericParameter> genericParameters;
    if (current().kind == TokenKind::Less)
    {
      static_cast<void>(consume());
      while (current().kind != TokenKind::Greater)
      {
        if (current().kind == TokenKind::KeywordConst)
        {
          const Token constToken = consume();
          if (current().kind != TokenKind::Identifier) throw ParseError("expected a const parameter name after `const`", current().span);
          const Token parameter = consume();
          expect(TokenKind::Colon, "expected `:` after const parameter name");
          auto type = parseTypeUntil({TokenKind::Comma, TokenKind::Greater});
          const SourceSpan parameterSpan{constToken.span.begin, type->span.end};
          genericParameters.push_back(GenericParameter{.kind = GenericParameterKind::Const,
                                                       .name = parameter.text,
                                                       .type = std::move(type),
                                                       .span = parameterSpan});
        }
        else
        {
          if (current().kind != TokenKind::Identifier) throw ParseError("expected a function generic parameter", current().span);
          const Token parameter = consume();
          std::vector<std::string> traitBounds;
          if (current().kind == TokenKind::Colon)
          {
            static_cast<void>(consume());
            while (true)
            {
              if (current().kind != TokenKind::Identifier) throw ParseError("expected a trait bound name", current().span);
              traitBounds.push_back(consume().text);
              if (current().kind != TokenKind::Plus) break;
              static_cast<void>(consume());
            }
          }
          genericParameters.push_back(GenericParameter{.kind = GenericParameterKind::Type,
                                                        .name = parameter.text,
                                                        .type = nullptr,
                                                        .traitBounds = std::move(traitBounds),
                                                        .span = parameter.span});
        }
        if (current().kind != TokenKind::Comma) break;
        static_cast<void>(consume());
        if (current().kind == TokenKind::Greater) throw ParseError("expected a generic parameter after `,`", current().span);
      }
      expect(TokenKind::Greater, "expected `>` after function generic parameters");
    }
    expect(TokenKind::LeftParen, "expected `(` after function name");

    std::vector<FunctionParameter> parameters;
    while (current().kind != TokenKind::RightParen)
    {
      if (current().kind != TokenKind::Identifier)
      {
        throw ParseError("expected a parameter name", current().span);
      }
      const Token parameterName = consume();
      expect(TokenKind::Colon, "expected `:` after parameter name");
      auto parameterType = parseTypeUntil({TokenKind::Comma, TokenKind::RightParen});
      const SourceSpan parameterSpan{parameterName.span.begin, parameterType->span.end};
      parameters.emplace_back(parameterName.text, std::move(parameterType), parameterSpan);

      if (current().kind != TokenKind::Comma)
      {
        break;
      }
      static_cast<void>(consume());
      if (current().kind == TokenKind::RightParen)
      {
        break;
      }
    }
    expect(TokenKind::RightParen, "expected `)` after function parameters");

    TypeSyntaxPtr returnType;
    if (current().kind == TokenKind::Arrow)
    {
      static_cast<void>(consume());
      returnType = parseTypeUntil({TokenKind::LeftBrace, TokenKind::FatArrow, TokenKind::KeywordWhere});
    }

    ExpressionPtr whereClause;
    if (current().kind == TokenKind::KeywordWhere)
    {
      static_cast<void>(consume());
      std::vector<Token> conditionTokens;
      while (current().kind != TokenKind::LeftBrace && current().kind != TokenKind::FatArrow &&
             current().kind != TokenKind::Equal && current().kind != TokenKind::Semicolon)
      {
        if (current().kind == TokenKind::End)
          throw ParseError("expected a where condition", current().span);
        conditionTokens.push_back(consume());
      }
      if (conditionTokens.empty()) throw ParseError("expected a where condition", current().span);
      const size_t end = conditionTokens.back().span.end;
      conditionTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{end, end}});
      whereClause = ExpressionParser{std::move(conditionTokens)}.parse();
    }

    std::optional<Block> body;
    if (current().kind == TokenKind::FatArrow)
    {
      // Expression body sugar: `=> expr;` becomes a block returning the
      // body value.
      static_cast<void>(consume());
      auto expression = parseExpressionUntil(TokenKind::Semicolon);
      const Token semicolon = current();
      expect(TokenKind::Semicolon, "expected `;` after expression body");
      std::vector<StatementPtr> statements;
      statements.push_back(std::make_unique<ReturnStatement>(std::move(expression),
                                                             SourceSpan{expression->span.begin, semicolon.span.end}));
      body.emplace(SourceSpan{statements.front()->span.begin, semicolon.span.end}, std::move(statements), nullptr);
    }
    else
    {
      auto blockTokens = consumeBlockTokens();
      body.emplace(BlockParser{std::move(blockTokens)}.parse());
    }
    const SourceSpan span{funToken.span.begin, body->span.end};
    return std::make_unique<FunctionDeclaration>(name.text, std::move(genericParameters), std::move(parameters), std::move(returnType),
                                                 std::move(*body), span, constFunction, std::move(whereClause));
  }

  auto ModuleParser::parseExpressionUntil(TokenKind terminator) -> ExpressionPtr
  {
    std::vector<Token> expressionTokens;
    size_t parenthesisDepth{};
    size_t squareDepth{};
    size_t braceDepth{};
    while (current().kind != TokenKind::End)
    {
      const TokenKind kind = current().kind;
      if (parenthesisDepth == 0 && squareDepth == 0 && braceDepth == 0 && kind == terminator) break;
      if (kind == TokenKind::LeftParen) ++parenthesisDepth;
      else if (kind == TokenKind::RightParen && parenthesisDepth != 0) --parenthesisDepth;
      else if (kind == TokenKind::LeftSquare) ++squareDepth;
      else if (kind == TokenKind::RightSquare && squareDepth != 0) --squareDepth;
      else if (kind == TokenKind::LeftBrace) ++braceDepth;
      else if (kind == TokenKind::RightBrace && braceDepth != 0) --braceDepth;
      expressionTokens.push_back(consume());
    }
    if (expressionTokens.empty()) throw ParseError("expected an expression body", current().span);
    const size_t end = expressionTokens.back().span.end;
    expressionTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{end, end}});
    return ExpressionParser{std::move(expressionTokens)}.parse();
  }

  auto ModuleParser::parseTraitDeclaration() -> ModuleItemPtr
  {
    const Token traitToken = consume();
    if (current().kind != TokenKind::Identifier) throw ParseError("expected a trait name after `trait`", current().span);
    const Token name = consume();
    std::vector<std::string> supertraits;
    if (current().kind == TokenKind::Colon)
    {
      static_cast<void>(consume());
      while (true)
      {
        if (current().kind != TokenKind::Identifier) throw ParseError("expected a supertrait name", current().span);
        supertraits.push_back(consume().text);
        if (current().kind != TokenKind::Plus) break;
        static_cast<void>(consume());
      }
    }
    expect(TokenKind::LeftBrace, "expected `{` after trait name");
    std::vector<TraitMethodDeclaration> methods;
    while (current().kind != TokenKind::RightBrace)
    {
      if (current().kind != TokenKind::KeywordFun) throw ParseError("expected a trait method", current().span);
      methods.push_back(parseTraitMethod());
    }
    const Token close = current();
    expect(TokenKind::RightBrace, "expected `}` to close trait");
    return std::make_unique<TraitDeclaration>(name.text, std::move(supertraits), std::move(methods),
                                              SourceSpan{traitToken.span.begin, close.span.end});
  }

  auto ModuleParser::parseImplDeclaration() -> ModuleItemPtr
  {
    const Token implToken = consume();
    if (current().kind != TokenKind::Identifier) throw ParseError("expected a trait name after `impl`", current().span);
    const Token trait = consume();
    expect(TokenKind::KeywordFor, "expected `for` after impl trait name");
    auto target = parseTypeUntil({TokenKind::LeftBrace});
    expect(TokenKind::LeftBrace, "expected `{` after impl target type");
    std::vector<TraitMethodDeclaration> methods;
    while (current().kind != TokenKind::RightBrace)
    {
      if (current().kind != TokenKind::KeywordFun) throw ParseError("expected an impl method", current().span);
      methods.push_back(parseTraitMethod());
    }
    const Token close = current();
    expect(TokenKind::RightBrace, "expected `}` to close impl");
    return std::make_unique<ImplDeclaration>(trait.text, std::move(target), std::move(methods),
                                             SourceSpan{implToken.span.begin, close.span.end});
  }

  auto ModuleParser::parseTraitMethod() -> TraitMethodDeclaration
  {
    const Token funToken = consume();
    if (current().kind != TokenKind::Identifier) throw ParseError("expected a method name after `fun`", current().span);
    const Token name = consume();
    expect(TokenKind::LeftParen, "expected `(` after method name");
    std::vector<FunctionParameter> parameters;
    while (current().kind != TokenKind::RightParen)
    {
      if (current().kind != TokenKind::Identifier) throw ParseError("expected a parameter name", current().span);
      const Token parameterName = consume();
      expect(TokenKind::Colon, "expected `:` after parameter name");
      auto parameterType = parseTypeUntil({TokenKind::Comma, TokenKind::RightParen});
      const SourceSpan parameterSpan{parameterName.span.begin, parameterType->span.end};
      parameters.emplace_back(parameterName.text, std::move(parameterType), parameterSpan);
      if (current().kind != TokenKind::Comma) break;
      static_cast<void>(consume());
      if (current().kind == TokenKind::RightParen) break;
    }
    expect(TokenKind::RightParen, "expected `)` after method parameters");
    TypeSyntaxPtr returnType;
    if (current().kind == TokenKind::Arrow)
    {
      static_cast<void>(consume());
      returnType = parseTypeUntil({TokenKind::LeftBrace, TokenKind::Semicolon});
    }
    std::optional<Block> body;
    if (current().kind == TokenKind::LeftBrace)
    {
      auto blockTokens = consumeBlockTokens();
      body.emplace(BlockParser{std::move(blockTokens)}.parse());
    }
    else
    {
      expect(TokenKind::Semicolon, "expected `;` or a body after method signature");
    }
    const size_t end = body.has_value() ? body->span.end : current().span.begin;
    return TraitMethodDeclaration{name.text, std::move(parameters), std::move(returnType), std::move(body),
                                  SourceSpan{funToken.span.begin, end}};
  }

  auto ModuleParser::parseTypeUntil(const std::vector<TokenKind> &terminators) -> TypeSyntaxPtr
  {
    std::vector<Token> typeTokens;
    size_t angleDepth{};
    while (current().kind != TokenKind::End)
    {
      if (current().kind == TokenKind::Less) ++angleDepth;
      else if (current().kind == TokenKind::Greater && angleDepth != 0) --angleDepth;
      else if (current().kind == TokenKind::ShiftRight)
      {
        // `>>` lexes as one token; split it into two closing brackets so
        // adjacent nested types like `Result<array<i64>>` parse.
        const Token token = consume();
        const size_t middle = token.span.begin + 1;
        typeTokens.push_back(Token{.kind = TokenKind::Greater, .text = ">", .span = SourceSpan{token.span.begin, middle}});
        typeTokens.push_back(Token{.kind = TokenKind::Greater, .text = ">", .span = SourceSpan{middle, token.span.end}});
        angleDepth = angleDepth > 2 ? angleDepth - 2 : 0;
        continue;
      }
      if (angleDepth == 0 && std::find(terminators.begin(), terminators.end(), current().kind) != terminators.end()) break;
      typeTokens.push_back(consume());
    }

    const size_t position = typeTokens.empty() ? current().span.begin : typeTokens.back().span.end;
    typeTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{position, position}});
    return TypeParser{std::move(typeTokens)}.parse();
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

  auto ModuleParser::peek(size_t offset) const -> const Token &
  {
    return tokens_[std::min(cursor_ + offset, tokens_.size() - 1)];
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
