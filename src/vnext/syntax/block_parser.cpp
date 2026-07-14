// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/syntax/block_parser.hpp"

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
      if (current().kind == TokenKind::Identifier && peek(1).kind == TokenKind::Assign)
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

  auto BlockParser::parseAssignStatement() -> StatementPtr
  {
    const Token name = consume();
    expect(TokenKind::Assign, "expected `:=` after assignment target");
    auto value = parseExpressionUntil(TokenKind::Semicolon);
    const Token semicolon = current();
    expect(TokenKind::Semicolon, "expected `;` after assignment value");
    return std::make_unique<AssignStatement>(name.text, std::move(value), SourceSpan{name.span.begin, semicolon.span.end});
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
    while (current().kind != TokenKind::End)
    {
      const TokenKind kind = current().kind;
      if (parenthesisDepth == 0 && squareDepth == 0 &&
          std::find(terminators.begin(), terminators.end(), kind) != terminators.end())
      {
        break;
      }
      if (parenthesisDepth == 0 && squareDepth == 0 &&
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
