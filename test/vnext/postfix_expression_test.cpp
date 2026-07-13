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

  [[nodiscard]] auto asCall(const syntax::ExpressionPtr &expression) -> const syntax::CallExpression &
  {
    const auto *call = dynamic_cast<const syntax::CallExpression *>(expression.get());
    REQUIRE(call != nullptr);
    return *call;
  }

  [[nodiscard]] auto asIndex(const syntax::ExpressionPtr &expression) -> const syntax::IndexExpression &
  {
    const auto *index = dynamic_cast<const syntax::IndexExpression *>(expression.get());
    REQUIRE(index != nullptr);
    return *index;
  }

  [[nodiscard]] auto asMember(const syntax::ExpressionPtr &expression) -> const syntax::MemberExpression &
  {
    const auto *member = dynamic_cast<const syntax::MemberExpression *>(expression.get());
    REQUIRE(member != nullptr);
    return *member;
  }
} // namespace

TEST_CASE("vNext expression parser binds postfix chains before infix operators", "[vNext][Syntax][Postfix]")
{
  const auto expression = syntax::parseExpression("f(1, x + 2).items[0] * 3");
  const auto &multiply = asBinary(expression);
  REQUIRE(multiply.operatorText == "*");

  const auto &index = asIndex(multiply.left);
  const auto &member = asMember(index.receiver);
  const auto &call = asCall(member.receiver);
  REQUIRE(member.member == "items");
  REQUIRE(call.arguments.size() == 2);
  REQUIRE(asBinary(call.arguments[1]).operatorText == "+");
}

TEST_CASE("vNext expression parser preserves left-to-right postfix nesting", "[vNext][Syntax][Postfix]")
{
  const auto expression = syntax::parseExpression("a.b(c)[i].d");
  const auto &outerMember = asMember(expression);
  REQUIRE(outerMember.member == "d");
  const auto &index = asIndex(outerMember.receiver);
  const auto &call = asCall(index.receiver);
  const auto &innerMember = asMember(call.callee);
  REQUIRE(innerMember.member == "b");
}

TEST_CASE("vNext expression parser diagnoses a missing member name", "[vNext][Syntax][Postfix]")
{
  try
  {
    static_cast<void>(syntax::parseExpression("value."));
    FAIL("expected vNext parser to reject a member access without a name");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected a member name after `.`");
    REQUIRE(error.span().begin == 6);
    REQUIRE(error.span().end == 6);
  }
}
