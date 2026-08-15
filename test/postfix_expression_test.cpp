// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "syntax/parser.hpp"

namespace syntax = NG::syntax;

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

TEST_CASE("vNext expression parser accepts empty and trailing-comma call arguments", "[vNext][Syntax][Postfix]")
{
  const auto emptyCall = syntax::parseExpression("f()()");
  const auto &outerCall = asCall(emptyCall);
  REQUIRE(outerCall.arguments.empty());
  REQUIRE(asCall(outerCall.callee).arguments.empty());
  REQUIRE(outerCall.span.begin == 0);
  REQUIRE(outerCall.span.end == 5);

  const auto trailingCommaCall = syntax::parseExpression("f(1,)");
  const auto &call = asCall(trailingCommaCall);
  REQUIRE(call.arguments.size() == 1);
  REQUIRE(call.span.begin == 0);
  REQUIRE(call.span.end == 5);
}

TEST_CASE("vNext expression parser diagnoses incomplete postfix forms with exact spans", "[vNext][Syntax][Postfix]")
{
  const auto requireError = [](std::string_view source, std::string_view message, size_t begin, size_t end) {
    try
    {
      static_cast<void>(syntax::parseExpression(source));
      FAIL("expected vNext parser to reject incomplete postfix expression");
    }
    catch (const syntax::ParseError &error)
    {
      REQUIRE(std::string{error.what()} == message);
      REQUIRE(error.span().begin == begin);
      REQUIRE(error.span().end == end);
    }
  };

  requireError("f(1", "expected `)` after call arguments", 3, 3);
  requireError("f(, 1)", "expected an expression before `,` in call arguments", 2, 3);
  requireError("items[]", "expected an index expression after `[`", 6, 7);
  requireError("items[0", "expected `]` after index expression", 7, 7);
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
