// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "const_eval.hpp"

#include <limits>

namespace syntax = NG::syntax;
namespace const_eval = NG::const_eval;

namespace
{
  [[nodiscard]] auto parseExpr(std::string_view source) -> syntax::ConstExprPtr
  {
    return syntax::ConstExprParser{syntax::Lexer{}.lex(source)}.parse();
  }

  struct Fixture
  {
    const_eval::ConstInterner interner;
    const_eval::ConstEvaluator evaluator{interner};

    [[nodiscard]] auto eval(std::string_view source) -> const_eval::ConstValueId
    {
      return evaluator.evaluate(*parseExpr(source), {});
    }
  };
} // namespace

TEST_CASE("vNext const interner gives canonical identity to integers", "[vNext][Const][Intern]")
{
  const_eval::ConstInterner interner;
  REQUIRE(interner.internInteger(3) == interner.internInteger(3));
  REQUIRE(interner.internInteger(3) != interner.internInteger(4));
  REQUIRE(interner.internUnit() == interner.internUnit());
  REQUIRE(interner.internBool(true) == interner.internBool(true));
  REQUIRE(interner.internBool(true) != interner.internBool(false));
  REQUIRE(interner.internString("value") == interner.internString("value"));
  REQUIRE(interner.internString("value") != interner.internString("other"));

  const auto tuple = interner.internTuple({interner.internInteger(1), interner.internBool(false)});
  const auto same = interner.internTuple({interner.internInteger(1), interner.internBool(false)});
  const auto different = interner.internTuple({interner.internInteger(2), interner.internBool(false)});
  REQUIRE(tuple == same);
  REQUIRE(tuple != different);
}

TEST_CASE("vNext const evaluator equates equivalent integer spellings", "[vNext][Const][Eval]")
{
  Fixture fixture;
  REQUIRE(fixture.eval("1 + 2") == fixture.eval("3"));
  REQUIRE(fixture.eval("(1 + 2) * 4") == fixture.eval("12"));
  REQUIRE(fixture.eval("7 / 2") == fixture.eval("3"));
  REQUIRE(fixture.eval("7 % 3") == fixture.eval("1"));
  REQUIRE(fixture.eval("-5 + 3") == fixture.eval("-2"));
  REQUIRE(fixture.eval("+7") == fixture.eval("7"));
}

TEST_CASE("vNext const evaluator reports overflow with source spans", "[vNext][Const][Eval]")
{
  Fixture fixture;
  try
  {
    static_cast<void>(fixture.eval("9223372036854775807 + 1"));
    FAIL("expected addition overflow");
  }
  catch (const const_eval::ConstEvalError &error)
  {
    REQUIRE(std::string{error.what()} == "const integer `+` overflow");
    REQUIRE(error.span.begin == 0);
    REQUIRE(error.span.end == 23);
  }

  REQUIRE_THROWS_WITH(fixture.eval("4611686018427387904 * 2"), "const integer `*` overflow");
  REQUIRE_THROWS_WITH(fixture.eval("-9223372036854775807 - 2"), "const integer `-` overflow");
}

TEST_CASE("vNext const evaluator rejects division and modulo by zero", "[vNext][Const][Eval]")
{
  Fixture fixture;
  REQUIRE_THROWS_WITH(fixture.eval("1 / 0"), "const integer division by zero");
  REQUIRE_THROWS_WITH(fixture.eval("1 % 0"), "const integer modulo by zero");
  REQUIRE_THROWS_WITH(fixture.eval("9223372036854775807 / 0"), "const integer division by zero");
}

TEST_CASE("vNext const evaluator rejects signed division overflow", "[vNext][Const][Eval]")
{
  Fixture fixture;
  // -(INT64_MIN) is representable as a sub-expression, then dividing by -1 overflows.
  REQUIRE_THROWS_WITH(fixture.eval("(-9223372036854775807 - 1) / -1"), "const integer `/` overflow");
}

TEST_CASE("vNext const evaluator rejects out-of-range integer literals", "[vNext][Const][Eval]")
{
  Fixture fixture;
  REQUIRE_THROWS_WITH(fixture.eval("9223372036854775808"), "const integer `9223372036854775808` is out of range");
}

TEST_CASE("vNext const evaluator rejects unresolved const names", "[vNext][Const][Eval]")
{
  Fixture fixture;
  try
  {
    static_cast<void>(fixture.eval("N"));
    FAIL("expected unresolved const name");
  }
  catch (const const_eval::ConstEvalError &error)
  {
    REQUIRE(std::string{error.what()} == "unresolved const name `N`");
  }
}

TEST_CASE("vNext const evaluator resolves bound const names", "[vNext][Const][Eval]")
{
  Fixture fixture;
  const_eval::ConstBindings bindings{{"N", fixture.interner.internInteger(5)}};
  REQUIRE(fixture.evaluator.evaluate(*parseExpr("N + 2"), bindings) == fixture.interner.internInteger(7));
}

TEST_CASE("vNext const evaluator validates array lengths", "[vNext][Const][Eval]")
{
  Fixture fixture;
  REQUIRE(fixture.evaluator.evaluateArrayLength(*parseExpr("1 + 2"), {}) == 3);
  REQUIRE_THROWS_WITH(fixture.evaluator.evaluateArrayLength(*parseExpr("-1"), {}), "array length must be non-negative, got -1");
}
