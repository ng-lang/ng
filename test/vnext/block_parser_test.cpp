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
