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
  REQUIRE(asBinary(multiply.left).operatorText == "+");
  REQUIRE(multiply.span.begin == 0);
  REQUIRE(multiply.span.end == 11);
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
