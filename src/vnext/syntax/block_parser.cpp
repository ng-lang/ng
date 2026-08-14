// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/block_parser.hpp"
#include "vnext/syntax/type_parser.hpp"

#include <algorithm>
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

    std::vector<StatementPtr> statements;
    ExpressionPtr tailExpression;
    while (current().kind != TokenKind::RightBrace)
    {
      if (current().kind == TokenKind::End)
      {
        throw ParseError("expected `}` to close block", current().span);
      }

      if (current().kind == TokenKind::KeywordLet)
      {
        statements.push_back(parseLetStatement());
        continue;
      }
      if ((current().kind == TokenKind::Identifier || current().kind == TokenKind::Star ||
           current().kind == TokenKind::LeftParen) &&
          hasTopLevelAssignment())
      {
        statements.push_back(parseAssignStatement());
        continue;
      }
      if (current().kind == TokenKind::KeywordFun)
      {
        throw ParseError("module declarations are not permitted in a block", current().span);
      }
      if (current().kind == TokenKind::KeywordReturn)
      {
        statements.push_back(parseReturnStatement());
        continue;
      }
      if (current().kind == TokenKind::KeywordIf)
      {
        statements.push_back(parseIfStatement());
        continue;
      }
      if (current().kind == TokenKind::KeywordConst && peek(1).kind == TokenKind::KeywordIf)
      {
        statements.push_back(parseConstIfStatement());
        continue;
      }
      if (current().kind == TokenKind::KeywordLoop)
      {
        statements.push_back(parseLoopStatement());
        continue;
      }
      if (current().kind == TokenKind::KeywordNext)
      {
        statements.push_back(parseNextStatement());
        continue;
      }
      if (current().kind == TokenKind::KeywordSwitch)
      {
        statements.push_back(parseSwitchStatement());
        continue;
      }

      auto expression = parseExpressionUntil(TokenKind::RightBrace);
      if (current().kind == TokenKind::Semicolon)
      {
        const Token semicolon = consume();
        statements.push_back(
            std::make_unique<ExpressionStatement>(std::move(expression), SourceSpan{expression->span.begin, semicolon.span.end}));
        continue;
      }

      tailExpression = std::move(expression);
      break;
    }

    const Token close = current();
    expect(TokenKind::RightBrace, "expected `}` to close block");
    return Block{SourceSpan{open.span.begin, close.span.end}, std::move(statements), std::move(tailExpression)};
  }

  auto BlockParser::parseLetStatement() -> StatementPtr
  {
    const Token letToken = consume();
    const bool isMutable = current().kind == TokenKind::KeywordMut;
    if (isMutable)
    {
      static_cast<void>(consume());
    }

    std::vector<std::string> destructuredNames;
    Token name{.kind = TokenKind::Identifier, .text = {}, .span = current().span};
    if (current().kind == TokenKind::LeftParen)
    {
      static_cast<void>(consume());
      if (current().kind == TokenKind::RightParen) throw ParseError("tuple binding cannot be empty", current().span);
      while (true)
      {
        if (current().kind != TokenKind::Identifier) throw ParseError("expected a binding name in tuple pattern", current().span);
        destructuredNames.push_back(consume().text);
        if (current().kind != TokenKind::Comma) break;
        static_cast<void>(consume());
      }
      expect(TokenKind::RightParen, "expected `)` after tuple binding");
      name.text = destructuredNames.front();
    }
    else
    {
      if (current().kind != TokenKind::Identifier)
        throw ParseError("expected a binding name after `let`", current().span);
      name = consume();
    }
    std::shared_ptr<TypeSyntax> annotation;
    if (current().kind == TokenKind::Colon)
    {
      static_cast<void>(consume());
      annotation = std::shared_ptr<TypeSyntax>{parseTypeUntil(TokenKind::Equal).release()};
    }
    expect(TokenKind::Equal, "expected `=` after let binding name");

    auto initializer = parseExpressionUntil(TokenKind::Semicolon);
    const Token semicolon = current();
    expect(TokenKind::Semicolon, "expected `;` after let initializer");
    return std::make_unique<LetStatement>(name.text, isMutable, std::move(annotation), std::move(initializer),
                                          SourceSpan{letToken.span.begin, semicolon.span.end},
                                          std::move(destructuredNames));
  }

  auto BlockParser::parseAssignStatement() -> StatementPtr
  {
    auto target = parseExpressionUntil(TokenKind::Assign);
    expect(TokenKind::Assign, "expected `:=` after assignment target");
    auto value = parseExpressionUntil(TokenKind::Semicolon);
    const Token semicolon = current();
    expect(TokenKind::Semicolon, "expected `;` after assignment value");
    const size_t begin = target->span.begin;
    return std::make_unique<AssignStatement>(std::move(target), std::move(value), SourceSpan{begin, semicolon.span.end});
  }

  auto BlockParser::parseReturnStatement() -> StatementPtr
  {
    const Token returnToken = consume();
    if (current().kind == TokenKind::Semicolon)
    {
      const Token semicolon = consume();
      return std::make_unique<ReturnStatement>(nullptr, SourceSpan{returnToken.span.begin, semicolon.span.end});
    }

    auto value = parseExpressionUntil(TokenKind::Semicolon);
    const Token semicolon = current();
    expect(TokenKind::Semicolon, "expected `;` after return value");
    return std::make_unique<ReturnStatement>(std::move(value), SourceSpan{returnToken.span.begin, semicolon.span.end});
  }

  auto BlockParser::parseLoopStatement() -> StatementPtr
  {
    const Token loopToken = consume();
    expect(TokenKind::LeftParen, "expected `(` after `loop`");

    std::vector<LoopBinding> bindings;
    while (current().kind != TokenKind::RightParen)
    {
      if (current().kind != TokenKind::Identifier)
      {
        throw ParseError("expected a loop binding name", current().span);
      }
      const Token name = consume();
      expect(TokenKind::Equal, "expected `=` after loop binding name");
      auto initializer = parseExpressionUntilAny({TokenKind::Comma, TokenKind::RightParen});
      const SourceSpan span{name.span.begin, initializer->span.end};
      bindings.emplace_back(name.text, std::move(initializer), span);

      if (current().kind != TokenKind::Comma)
      {
        break;
      }
      static_cast<void>(consume());
      if (current().kind == TokenKind::RightParen)
      {
        throw ParseError("expected a loop binding after `,`", current().span);
      }
    }
    expect(TokenKind::RightParen, "expected `)` after loop bindings");
    Block body = parseNestedBlock();
    const SourceSpan span{loopToken.span.begin, body.span.end};
    return std::make_unique<LoopStatement>(std::move(bindings), std::move(body), span);
  }

  auto BlockParser::parseNextStatement() -> StatementPtr
  {
    const Token nextToken = consume();
    expect(TokenKind::LeftParen, "expected `(` after `next`");

    std::vector<ExpressionPtr> arguments;
    if (current().kind != TokenKind::RightParen)
    {
      do
      {
        arguments.push_back(parseExpressionUntilAny({TokenKind::Comma, TokenKind::RightParen}));
        if (current().kind != TokenKind::Comma)
        {
          break;
        }
        static_cast<void>(consume());
        if (current().kind == TokenKind::RightParen)
        {
          throw ParseError("expected a next argument after `,`", current().span);
        }
      } while (current().kind != TokenKind::RightParen);
    }
    expect(TokenKind::RightParen, "expected `)` after next arguments");
    const Token semicolon = current();
    expect(TokenKind::Semicolon, "expected `;` after next arguments");
    return std::make_unique<NextStatement>(std::move(arguments), SourceSpan{nextToken.span.begin, semicolon.span.end});
  }

  auto BlockParser::parseIfStatement() -> StatementPtr
  {
    const Token ifToken = consume();
    auto condition = parseExpressionUntil(TokenKind::LeftBrace);
    Block consequence = parseNestedBlock();

    std::unique_ptr<Block> alternative;
    size_t end = consequence.span.end;
    if (current().kind == TokenKind::KeywordElse)
    {
      static_cast<void>(consume());
      if (current().kind == TokenKind::KeywordIf)
      {
        auto nestedIf = parseIfStatement();
        end = nestedIf->span.end;
        std::vector<StatementPtr> statements;
        statements.push_back(std::move(nestedIf));
        alternative = std::make_unique<Block>(SourceSpan{statements.front()->span.begin, end}, std::move(statements), nullptr);
      }
      else
      {
        Block elseBlock = parseNestedBlock();
        end = elseBlock.span.end;
        alternative = std::make_unique<Block>(std::move(elseBlock));
      }
    }

    return std::make_unique<IfStatement>(std::move(condition), std::move(consequence), std::move(alternative),
                                         SourceSpan{ifToken.span.begin, end});
  }

  auto BlockParser::parseConstIfStatement() -> StatementPtr
  {
    const Token constToken = consume();
    expect(TokenKind::KeywordIf, "expected `if` after `const`");
    auto condition = parseExpressionUntil(TokenKind::LeftBrace);
    Block consequence = parseNestedBlock();

    std::unique_ptr<Block> alternative;
    size_t end = consequence.span.end;
    if (current().kind == TokenKind::KeywordElse)
    {
      static_cast<void>(consume());
      if (current().kind == TokenKind::KeywordConst && peek(1).kind == TokenKind::KeywordIf)
      {
        auto nestedConstIf = parseConstIfStatement();
        end = nestedConstIf->span.end;
        std::vector<StatementPtr> statements;
        statements.push_back(std::move(nestedConstIf));
        alternative = std::make_unique<Block>(SourceSpan{statements.front()->span.begin, end}, std::move(statements), nullptr);
      }
      else
      {
        Block elseBlock = parseNestedBlock();
        end = elseBlock.span.end;
        alternative = std::make_unique<Block>(std::move(elseBlock));
      }
    }

    return std::make_unique<ConstIfStatement>(std::move(condition), std::move(consequence), std::move(alternative),
                                              SourceSpan{constToken.span.begin, end});
  }

  auto BlockParser::parseSwitchStatement() -> StatementPtr
  {
    const Token switchToken = consume();
    expect(TokenKind::LeftParen, "expected `(` after `switch`");
    auto value = parseExpressionUntil(TokenKind::RightParen);
    expect(TokenKind::RightParen, "expected `)` after switch value");
    expect(TokenKind::LeftBrace, "expected `{` after switch value");

    std::vector<SwitchCase> cases;
    std::unique_ptr<Block> otherwise;
    while (current().kind != TokenKind::RightBrace)
    {
      if (current().kind == TokenKind::End)
      {
        throw ParseError("expected `}` to close switch", current().span);
      }
      if (current().kind == TokenKind::KeywordOtherwise)
      {
        static_cast<void>(consume());
        Block fallback = parseNestedBlock();
        otherwise = std::make_unique<Block>(std::move(fallback));
        continue;
      }
      if (current().kind != TokenKind::KeywordCase)
      {
        throw ParseError("expected `case` or `otherwise` in switch", current().span);
      }
      const Token caseToken = consume();
      if (current().kind != TokenKind::Identifier)
      {
        throw ParseError("expected a variant name after `case`", current().span);
      }
      const Token variant = consume();
      std::optional<std::string> bindingName;
      std::vector<std::string> bindingNames;
      if (current().kind == TokenKind::LeftParen)
      {
        static_cast<void>(consume());
        if (current().kind != TokenKind::Identifier)
        {
          throw ParseError("expected a payload binding name", current().span);
        }
        bindingName = consume().text;
        while (current().kind == TokenKind::Comma)
        {
          static_cast<void>(consume());
          if (current().kind != TokenKind::Identifier)
          {
            throw ParseError("expected a payload binding name after `,`", current().span);
          }
          bindingNames.push_back(consume().text);
        }
        expect(TokenKind::RightParen, "expected `)` after payload binding");
      }
      Block body = parseNestedBlock();
      cases.push_back(SwitchCase{SwitchCasePattern{variant.text, std::move(bindingName), std::move(bindingNames),
                                                   SourceSpan{caseToken.span.begin, body.span.end}},
                                 std::move(body)});
    }
    const Token close = current();
    expect(TokenKind::RightBrace, "expected `}` to close switch");
    return std::make_unique<SwitchStatement>(std::move(value), std::move(cases), std::move(otherwise),
                                             SourceSpan{switchToken.span.begin, close.span.end});
  }

  auto BlockParser::parseNestedBlock() -> Block
  {
    if (current().kind != TokenKind::LeftBrace)
    {
      throw ParseError("expected `{` to begin an if branch", current().span);
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
      else if (token.kind == TokenKind::End)
      {
        throw ParseError("expected `}` to close if branch", token.span);
      }
      blockTokens.push_back(token);
    } while (depth != 0);

    const size_t end = blockTokens.back().span.end;
    blockTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{end, end}});
    return BlockParser{std::move(blockTokens)}.parse();
  }

  auto BlockParser::parseExpressionUntil(TokenKind terminator) -> ExpressionPtr
  {
    return parseExpressionUntilAny({terminator});
  }

  auto BlockParser::parseExpressionUntilAny(const std::vector<TokenKind> &terminators) -> ExpressionPtr
  {
    std::vector<Token> expressionTokens;
    size_t parenthesisDepth{};
    size_t squareDepth{};
    size_t braceDepth{};
    while (current().kind != TokenKind::End)
    {
      const TokenKind kind = current().kind;
      if (parenthesisDepth == 0 && squareDepth == 0 &&
          std::find(terminators.begin(), terminators.end(), kind) != terminators.end())
      {
        break;
      }
      if (parenthesisDepth == 0 && squareDepth == 0 && braceDepth == 0 &&
          (kind == TokenKind::Semicolon || kind == TokenKind::RightBrace))
      {
        break;
      }

      if (kind == TokenKind::LeftParen)
      {
        ++parenthesisDepth;
      }
      else if (kind == TokenKind::RightParen && parenthesisDepth != 0)
      {
        --parenthesisDepth;
      }
      else if (kind == TokenKind::LeftSquare)
      {
        ++squareDepth;
      }
      else if (kind == TokenKind::RightSquare && squareDepth != 0)
      {
        --squareDepth;
      }
      else if (kind == TokenKind::LeftBrace)
      {
        ++braceDepth;
      }
      else if (kind == TokenKind::RightBrace && braceDepth != 0)
      {
        --braceDepth;
      }
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

  auto BlockParser::parseTypeUntil(TokenKind terminator) -> TypeSyntaxPtr
  {
    std::vector<Token> typeTokens;
    size_t angleDepth{};
    while (current().kind != TokenKind::End)
    {
      if (current().kind == TokenKind::Less) ++angleDepth;
      else if (current().kind == TokenKind::Greater && angleDepth != 0) --angleDepth;
      else if (current().kind == TokenKind::ShiftRight)
      {
        const Token token = consume();
        const size_t middle = token.span.begin + 1;
        typeTokens.push_back(Token{.kind = TokenKind::Greater, .text = ">", .span = SourceSpan{token.span.begin, middle}});
        typeTokens.push_back(Token{.kind = TokenKind::Greater, .text = ">", .span = SourceSpan{middle, token.span.end}});
        angleDepth = angleDepth > 2 ? angleDepth - 2 : 0;
        continue;
      }
      if (angleDepth == 0 && current().kind == terminator) break;
      typeTokens.push_back(consume());
    }
    const size_t position = typeTokens.empty() ? current().span.begin : typeTokens.back().span.end;
    typeTokens.push_back(Token{.kind = TokenKind::End, .text = {}, .span = SourceSpan{position, position}});
    return TypeParser{std::move(typeTokens)}.parse();
  }

  auto BlockParser::hasTopLevelAssignment() const -> bool
  {
    size_t parenthesisDepth{};
    size_t squareDepth{};
    for (size_t index = cursor_; index < tokens_.size(); ++index)
    {
      const auto kind = tokens_[index].kind;
      if (kind == TokenKind::LeftParen) ++parenthesisDepth;
      else if (kind == TokenKind::RightParen && parenthesisDepth != 0) --parenthesisDepth;
      else if (kind == TokenKind::LeftSquare) ++squareDepth;
      else if (kind == TokenKind::RightSquare && squareDepth != 0) --squareDepth;
      else if (parenthesisDepth == 0 && squareDepth == 0 && kind == TokenKind::Assign) return true;
      if (parenthesisDepth == 0 && squareDepth == 0 &&
          (kind == TokenKind::Semicolon || kind == TokenKind::RightBrace || kind == TokenKind::End))
        return false;
    }
    return false;
  }

  auto BlockParser::current() const -> const Token &
  {
    return tokens_[cursor_];
  }

  auto BlockParser::peek(size_t offset) const -> const Token &
  {
    return tokens_[std::min(cursor_ + offset, tokens_.size() - 1)];
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
