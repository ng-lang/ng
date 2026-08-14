// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/syntax/module_parser.hpp"
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

TEST_CASE("vNext expression parser distinguishes tuples from grouping", "[vNext][Syntax][Expression]")
{
  const auto tuple = syntax::parseExpression("(1, false, \"value\")");
  REQUIRE(tuple->kind == syntax::ExpressionKind::TupleLiteral);
  const auto *literal = dynamic_cast<const syntax::TupleLiteralExpression *>(tuple.get());
  REQUIRE(literal != nullptr);
  REQUIRE(literal->elements.size() == 3);

  const auto projection = syntax::parseExpression("tuple.2");
  REQUIRE(projection->kind == syntax::ExpressionKind::Index);
  const auto *index = dynamic_cast<const syntax::IndexExpression *>(projection.get());
  REQUIRE(index != nullptr);
  REQUIRE(index->index->kind == syntax::ExpressionKind::IntegerLiteral);
}

TEST_CASE("vNext expression parser diagnoses malformed tuple literals", "[vNext][Syntax][Expression]")
{
  try
  {
    static_cast<void>(syntax::parseExpression("(1,, 2)"));
    FAIL("expected malformed tuple to fail");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected an expression before `,` in tuple literal");
    REQUIRE(error.span().begin == 3);
    REQUIRE(error.span().end == 4);
  }
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

TEST_CASE("vNext expression parser constructs nested array literals", "[vNext][Syntax][Expression]")
{
  const auto expression = syntax::parseExpression("[1, [2, 3],]");
  const auto *array = dynamic_cast<const syntax::ArrayLiteralExpression *>(expression.get());
  REQUIRE(array != nullptr);
  REQUIRE(array->elements.size() == 2);
  REQUIRE(array->elements[0]->kind == syntax::ExpressionKind::IntegerLiteral);
  const auto *nested = dynamic_cast<const syntax::ArrayLiteralExpression *>(array->elements[1].get());
  REQUIRE(nested != nullptr);
  REQUIRE(nested->elements.size() == 2);
  REQUIRE(array->span.begin == 0);
  REQUIRE(array->span.end == 12);
}

TEST_CASE("vNext expression parser diagnoses malformed array literals", "[vNext][Syntax][Expression]")
{
  REQUIRE_THROWS_WITH(syntax::parseExpression("[,1]"), "expected an expression before `,` in array literal");
  REQUIRE_THROWS_WITH(syntax::parseExpression("[1"), "expected `]` after array literal");
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

TEST_CASE("vNext lexer skips line and block comments", "[vNext][Syntax][Expression]")
{
  REQUIRE_NOTHROW(syntax::parseExpression("1 + 2 // trailing comment"));
  REQUIRE_NOTHROW(syntax::parseExpression("/* leading */ 1 + 2"));
  REQUIRE_NOTHROW(syntax::parseExpression("1 /* middle */ + 2"));
  REQUIRE_NOTHROW(syntax::parseExpression("1 + 2 /* multi\nline */"));
  REQUIRE_THROWS_WITH(syntax::parseExpression("1 + 2 /* unterminated"), "unterminated block comment");
}

TEST_CASE("vNext source units accept comments between declarations", "[vNext][Syntax][Module]")
{
  const auto unit = syntax::parseSourceUnit("// header comment\nfun main() -> i64 { return 1; }\n// footer");
  REQUIRE(unit.items.size() == 1);
}
