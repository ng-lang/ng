// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/syntax/parser.hpp"

namespace syntax = NG::vnext::syntax;

namespace
{
  [[nodiscard]] auto asBinary(const syntax::ExpressionPtr &expression) -> const syntax::BinaryExpression &
  {
    const auto *binary = dynamic_cast<const syntax::BinaryExpression *>(expression.get());
    REQUIRE(binary != nullptr);
    return *binary;
  }

  [[nodiscard]] auto asPrefix(const syntax::ExpressionPtr &expression) -> const syntax::PrefixExpression &
  {
    const auto *prefix = dynamic_cast<const syntax::PrefixExpression *>(expression.get());
    REQUIRE(prefix != nullptr);
    return *prefix;
  }

  [[nodiscard]] auto asGrouped(const syntax::ExpressionPtr &expression) -> const syntax::GroupedExpression &
  {
    const auto *grouped = dynamic_cast<const syntax::GroupedExpression *>(expression.get());
    REQUIRE(grouped != nullptr);
    return *grouped;
  }
} // namespace

TEST_CASE("vNext expression parser gives multiplication precedence over addition", "[vNext][Syntax][Expression]")
{
  const auto expression = syntax::parseExpression("2 * 3 + 4");
  const auto &addition = asBinary(expression);
  REQUIRE(addition.operatorText == "+");
  REQUIRE(asBinary(addition.left).operatorText == "*");
}

TEST_CASE("vNext expression parser keeps equal-precedence operators left associative", "[vNext][Syntax][Expression]")
{
  const auto expression = syntax::parseExpression("a - b - c");
  const auto &outerSubtract = asBinary(expression);
  REQUIRE(outerSubtract.operatorText == "-");
  REQUIRE(asBinary(outerSubtract.left).operatorText == "-");
}

TEST_CASE("vNext expression parser gives prefix operators tighter precedence", "[vNext][Syntax][Expression]")
{
  const auto expression = syntax::parseExpression("-a * b");
  const auto &multiply = asBinary(expression);
  REQUIRE(multiply.operatorText == "*");
  REQUIRE(asPrefix(multiply.left).operatorText == "-");
}

TEST_CASE("vNext expression parser preserves grouping spans and AST shape", "[vNext][Syntax][Expression]")
{
  const auto expression = syntax::parseExpression("(a + b) * c");
  const auto &multiply = asBinary(expression);
  REQUIRE(multiply.operatorText == "*");
  const auto &grouped = asGrouped(multiply.left);
  REQUIRE(asBinary(grouped.expression).operatorText == "+");
  REQUIRE(grouped.span.begin == 0);
  REQUIRE(grouped.span.end == 7);
  REQUIRE(multiply.span.begin == 0);
  REQUIRE(multiply.span.end == 11);
}

TEST_CASE("vNext expression parser decodes string literal escapes with exact spans", "[vNext][Syntax][Expression]")
{
  const auto expression = syntax::parseExpression("\"line\\n\\\"quote\\\"\\\\\"");
  const auto *literal = dynamic_cast<const syntax::StringLiteralExpression *>(expression.get());
  REQUIRE(literal != nullptr);
  REQUIRE(literal->value == "line\n\"quote\"\\");
  REQUIRE(literal->span.begin == 0);
  REQUIRE(literal->span.end == 19);
}

TEST_CASE("vNext expression parser rejects malformed string literals", "[vNext][Syntax][Expression]")
{
  REQUIRE_THROWS_WITH(syntax::parseExpression("\"unterminated"), "unterminated string literal");
  REQUIRE_THROWS_WITH(syntax::parseExpression("\"\\r\""), "unsupported string escape `\\r`");
}

TEST_CASE("vNext expression parser diagnoses invalid source with a source span", "[vNext][Syntax][Expression]")
{
  try
  {
    static_cast<void>(syntax::parseExpression("a + @"));
    FAIL("expected vNext parser to reject unexpected character");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "unexpected character `@`");
    REQUIRE(error.span().begin == 4);
    REQUIRE(error.span().end == 5);
  }
}
