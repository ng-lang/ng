// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "syntax/const_expr.hpp"

namespace syntax = NG::syntax;

namespace
{
  [[nodiscard]] auto parseExpr(std::string_view source) -> syntax::ConstExprPtr
  {
    return syntax::ConstExprParser{syntax::Lexer{}.lex(source)}.parse();
  }

  [[nodiscard]] auto asBinary(const syntax::ConstExprPtr &expression) -> const syntax::ConstBinaryExpr &
  {
    const auto *binary = dynamic_cast<const syntax::ConstBinaryExpr *>(expression.get());
    REQUIRE(binary != nullptr);
    return *binary;
  }

  [[nodiscard]] auto asUnary(const syntax::ConstExprPtr &expression) -> const syntax::ConstUnaryExpr &
  {
    const auto *unary = dynamic_cast<const syntax::ConstUnaryExpr *>(expression.get());
    REQUIRE(unary != nullptr);
    return *unary;
  }

  [[nodiscard]] auto asInteger(const syntax::ConstExprPtr &expression) -> const syntax::ConstIntegerLiteral &
  {
    const auto *literal = dynamic_cast<const syntax::ConstIntegerLiteral *>(expression.get());
    REQUIRE(literal != nullptr);
    return *literal;
  }
} // namespace

TEST_CASE("vNext const expression parser parses integer literals", "[vNext][Syntax][Const]")
{
  const auto expression = parseExpr("42");
  REQUIRE(expression->kind == syntax::ConstExprKind::IntegerLiteral);
  REQUIRE(asInteger(expression).text == "42");
  REQUIRE(expression->span.begin == 0);
  REQUIRE(expression->span.end == 2);
}

TEST_CASE("vNext const expression parser honors precedence and parentheses", "[vNext][Syntax][Const]")
{
  const auto additive = parseExpr("1 + 2 * 3");
  const auto &root = asBinary(additive);
  REQUIRE(root.operatorText == "+");
  REQUIRE(asInteger(root.left).text == "1");
  REQUIRE(asBinary(root.right).operatorText == "*");

  const auto grouped = parseExpr("(1 + 2) * 3");
  const auto &groupedRoot = asBinary(grouped);
  REQUIRE(groupedRoot.operatorText == "*");
  REQUIRE(asBinary(groupedRoot.left).operatorText == "+");
  REQUIRE(asInteger(groupedRoot.right).text == "3");
}

TEST_CASE("vNext const expression parser parses unary plus and minus", "[vNext][Syntax][Const]")
{
  const auto negated = parseExpr("-3");
  const auto &unary = asUnary(negated);
  REQUIRE(unary.operatorText == "-");
  REQUIRE(asInteger(unary.operand).text == "3");

  const auto doubleNegated = parseExpr("--4");
  REQUIRE(asUnary(doubleNegated).operatorText == "-");
  REQUIRE(asUnary(asUnary(doubleNegated).operand).operatorText == "-");
}

TEST_CASE("vNext const expression parser preserves identifiers for const parameters", "[vNext][Syntax][Const]")
{
  const auto identifier = parseExpr("N");
  REQUIRE(identifier->kind == syntax::ConstExprKind::Identifier);
  const auto *named = dynamic_cast<const syntax::ConstIdentifier *>(identifier.get());
  REQUIRE(named != nullptr);
  REQUIRE(named->name == "N");
}

TEST_CASE("vNext const expression parser rejects missing close paren", "[vNext][Syntax][Const]")
{
  try
  {
    static_cast<void>(parseExpr("(1 + 2"));
    FAIL("expected missing close paren to fail");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `)` after const expression");
  }
}

TEST_CASE("vNext const expression parser renders and clones structurally", "[vNext][Syntax][Const]")
{
  const auto expression = parseExpr("(1 + 2) * 3");
  REQUIRE(syntax::renderConstExpr(*expression) == "((1 + 2) * 3)");
  const auto copy = syntax::cloneConstExpr(*expression);
  REQUIRE(syntax::renderConstExpr(*copy) == syntax::renderConstExpr(*expression));
}
