// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/syntax/block_parser.hpp"

namespace syntax = NG::vnext::syntax;

namespace
{
  [[nodiscard]] auto asLet(const syntax::StatementPtr &statement) -> const syntax::LetStatement &
  {
    const auto *let = dynamic_cast<const syntax::LetStatement *>(statement.get());
    REQUIRE(let != nullptr);
    return *let;
  }

  [[nodiscard]] auto asReturn(const syntax::StatementPtr &statement) -> const syntax::ReturnStatement &
  {
    const auto *returnStatement = dynamic_cast<const syntax::ReturnStatement *>(statement.get());
    REQUIRE(returnStatement != nullptr);
    return *returnStatement;
  }

  [[nodiscard]] auto asIf(const syntax::StatementPtr &statement) -> const syntax::IfStatement &
  {
    const auto *ifStatement = dynamic_cast<const syntax::IfStatement *>(statement.get());
    REQUIRE(ifStatement != nullptr);
    return *ifStatement;
  }

  [[nodiscard]] auto asLoop(const syntax::StatementPtr &statement) -> const syntax::LoopStatement &
  {
    const auto *loop = dynamic_cast<const syntax::LoopStatement *>(statement.get());
    REQUIRE(loop != nullptr);
    return *loop;
  }

  [[nodiscard]] auto asNext(const syntax::StatementPtr &statement) -> const syntax::NextStatement &
  {
    const auto *next = dynamic_cast<const syntax::NextStatement *>(statement.get());
    REQUIRE(next != nullptr);
    return *next;
  }

  [[nodiscard]] auto asExpression(const syntax::StatementPtr &statement) -> const syntax::ExpressionStatement &
  {
    const auto *expression = dynamic_cast<const syntax::ExpressionStatement *>(statement.get());
    REQUIRE(expression != nullptr);
    return *expression;
  }

  [[nodiscard]] auto asBinary(const syntax::ExpressionPtr &expression) -> const syntax::BinaryExpression &
  {
    const auto *binary = dynamic_cast<const syntax::BinaryExpression *>(expression.get());
    REQUIRE(binary != nullptr);
    return *binary;
  }
} // namespace

TEST_CASE("vNext block parser preserves tuple binding patterns", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ let mut (first, second) = (1, true); }");
  const auto *let = dynamic_cast<const syntax::LetStatement *>(block.statements.front().get());
  REQUIRE(let != nullptr);
  REQUIRE(let->isMutable);
  REQUIRE(let->destructuredNames == std::vector<std::string>{"first", "second"});
  REQUIRE(let->initializer->kind == syntax::ExpressionKind::TupleLiteral);
}

TEST_CASE("vNext block parser preserves index assignment targets", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ items[1] := 42; }");
  REQUIRE(block.statements.size() == 1);
  const auto *assignment = dynamic_cast<const syntax::AssignStatement *>(block.statements.front().get());
  REQUIRE(assignment != nullptr);
  REQUIRE(assignment->target->kind == syntax::ExpressionKind::Index);
  REQUIRE(assignment->value->kind == syntax::ExpressionKind::IntegerLiteral);
  REQUIRE(assignment->target->span.begin == 2);
  REQUIRE(assignment->target->span.end == 10);
}

TEST_CASE("vNext block parser treats let as a lexical statement", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ let mut total = 2 * 3 + 4; total; }");
  REQUIRE(block.statements.size() == 2);
  REQUIRE(block.tailExpression == nullptr);

  const auto &binding = asLet(block.statements[0]);
  REQUIRE(binding.name == "total");
  REQUIRE(binding.isMutable);
  REQUIRE(asBinary(binding.initializer).operatorText == "+");
  REQUIRE(asBinary(asBinary(binding.initializer).left).operatorText == "*");

  const auto &use = asExpression(block.statements[1]);
  REQUIRE(use.expression->kind == syntax::ExpressionKind::Identifier);
}

TEST_CASE("vNext block parser keeps a final expression as the block value", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ let value = 1; value + 2 }");
  REQUIRE(block.statements.size() == 1);
  REQUIRE(block.tailExpression != nullptr);
  REQUIRE(asBinary(block.tailExpression).operatorText == "+");
}

TEST_CASE("vNext block parser keeps loop state and next arguments as dedicated syntax", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ loop (left = first(1, 2), right = 2) { next (right, left); } }");
  REQUIRE(block.statements.size() == 1);
  const auto &loop = asLoop(block.statements[0]);
  REQUIRE(loop.bindings.size() == 2);
  REQUIRE(loop.bindings[0].name == "left");
  REQUIRE(loop.bindings[0].initializer->kind == syntax::ExpressionKind::Call);
  REQUIRE(loop.bindings[1].name == "right");
  REQUIRE(loop.body.statements.size() == 1);
  const auto &next = asNext(loop.body.statements[0]);
  REQUIRE(next.arguments.size() == 2);
  REQUIRE(next.arguments[0]->kind == syntax::ExpressionKind::Identifier);
  REQUIRE(next.span.begin == 41);
  REQUIRE(next.span.end == 60);
}

TEST_CASE("vNext block parser diagnoses malformed loop and next syntax", "[vNext][Syntax][Block]")
{
  const auto requireError = [](std::string_view source, std::string_view message) {
    try
    {
      static_cast<void>(syntax::parseBlock(source));
      FAIL("expected malformed loop or next syntax to fail");
    }
    catch (const syntax::ParseError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };

  requireError("{ loop state = 0 { } }", "expected `(` after `loop`");
  requireError("{ loop (state = 0,) { } }", "expected a loop binding after `,`");
  requireError("{ next value; }", "expected `(` after `next`");
  requireError("{ next (value,) ; }", "expected a next argument after `,`");
}

TEST_CASE("vNext block parser keeps if branches as nested block statements", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ if ready { return value; } else { return fallback; } }");
  REQUIRE(block.statements.size() == 1);
  const auto &ifStatement = asIf(block.statements[0]);
  REQUIRE(ifStatement.condition->kind == syntax::ExpressionKind::Identifier);
  REQUIRE(ifStatement.consequence.statements.size() == 1);
  REQUIRE(asReturn(ifStatement.consequence.statements[0]).value != nullptr);
  REQUIRE(ifStatement.alternative != nullptr);
  REQUIRE(ifStatement.alternative->statements.size() == 1);
  REQUIRE(asReturn(ifStatement.alternative->statements[0]).value != nullptr);
  REQUIRE(ifStatement.span.begin == 2);
  REQUIRE(ifStatement.span.end == 54);
}

TEST_CASE("vNext block parser desugars else-if into a nested alternative block", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ if first { return 1; } else if second { return 2; } else { return 3; } }");
  const auto &outer = asIf(block.statements.front());
  REQUIRE(outer.alternative != nullptr);
  REQUIRE(outer.alternative->statements.size() == 1);
  const auto &nested = asIf(outer.alternative->statements.front());
  REQUIRE(nested.alternative != nullptr);
  REQUIRE(nested.consequence.statements.size() == 1);
  REQUIRE(nested.alternative->statements.size() == 1);
}

TEST_CASE("vNext block parser supports an if statement without else", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ if enabled { return; } }");
  REQUIRE(block.statements.size() == 1);
  const auto &ifStatement = asIf(block.statements[0]);
  REQUIRE(ifStatement.alternative == nullptr);
  REQUIRE(ifStatement.consequence.statements.size() == 1);
}

TEST_CASE("vNext block parser diagnoses missing if branch blocks", "[vNext][Syntax][Block]")
{
  try
  {
    static_cast<void>(syntax::parseBlock("{ if enabled return; }"));
    FAIL("expected if statement to require a consequence block");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "unexpected token `return`");
    REQUIRE(error.span().begin == 13);
    REQUIRE(error.span().end == 19);
  }
}

TEST_CASE("vNext block parser represents valued and valueless returns as statements", "[vNext][Syntax][Block]")
{
  const auto block = syntax::parseBlock("{ return value + 1; return; }");
  REQUIRE(block.statements.size() == 2);
  REQUIRE(block.tailExpression == nullptr);

  const auto &valuedReturn = asReturn(block.statements[0]);
  REQUIRE(valuedReturn.value != nullptr);
  REQUIRE(asBinary(valuedReturn.value).operatorText == "+");
  REQUIRE(valuedReturn.span.begin == 2);
  REQUIRE(valuedReturn.span.end == 19);

  const auto &unitReturn = asReturn(block.statements[1]);
  REQUIRE(unitReturn.value == nullptr);
  REQUIRE(unitReturn.span.begin == 20);
  REQUIRE(unitReturn.span.end == 27);
}

TEST_CASE("vNext block parser diagnoses a return value without a terminator", "[vNext][Syntax][Block]")
{
  try
  {
    static_cast<void>(syntax::parseBlock("{ return value }"));
    FAIL("expected vNext parser to require a return terminator");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `;` after return value");
    REQUIRE(error.span().begin == 15);
    REQUIRE(error.span().end == 16);
  }
}

TEST_CASE("vNext block parser rejects a let initializer without a terminator", "[vNext][Syntax][Block]")
{
  try
  {
    static_cast<void>(syntax::parseBlock("{ let value = 1 }"));
    FAIL("expected vNext parser to require a let terminator");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `;` after let initializer");
    REQUIRE(error.span().begin == 16);
    REQUIRE(error.span().end == 17);
  }
}
