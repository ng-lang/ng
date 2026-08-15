// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  void check(std::string_view source)
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    static_cast<void>(typecheck::TypeChecker{}.check(module));
  }
} // namespace

TEST_CASE("vNext type checker interns equivalent const array lengths as one type", "[vNext][Typecheck][Const]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun computed() -> array<i64, 1 + 2> { return [1, 2, 3]; } "
      "fun literal() -> array<i64, 3> { return [1, 2, 3]; } "
      "fun other() -> array<i64, 2 * 2> { return [1, 2, 3, 4]; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto computed = result.functionTypeIds.at(0).returnType;
  const auto literal = result.functionTypeIds.at(1).returnType;
  const auto other = result.functionTypeIds.at(2).returnType;
  REQUIRE(computed == literal);
  REQUIRE(computed != other);
  REQUIRE(result.typeDescriptors.at(computed.value).length == 3);
  REQUIRE(result.typeDescriptors.at(other.value).length == 4);
}

TEST_CASE("vNext type checker accepts parenthesized and unary const array lengths", "[vNext][Typecheck][Const]")
{
  REQUIRE_NOTHROW(check("fun value() -> array<i64, (1 + 2) * 4> { return [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]; }"));
  REQUIRE_NOTHROW(check("fun value() -> array<i64, -2 + 4> { return [1, 2]; }"));
}

TEST_CASE("vNext type checker reports const overflow in array lengths", "[vNext][Typecheck][Const]")
{
  try
  {
    check("fun invalid() -> array<i64, 9223372036854775807 + 1> { return []; }");
    FAIL("expected array length const overflow");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const integer `+` overflow");
  }
}

TEST_CASE("vNext type checker reports division by zero in array lengths", "[vNext][Typecheck][Const]")
{
  try
  {
    check("fun invalid() -> array<i64, 4 / 0> { return []; }");
    FAIL("expected array length division by zero");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const integer division by zero");
  }
}

TEST_CASE("vNext type checker reports negative array lengths", "[vNext][Typecheck][Const]")
{
  try
  {
    check("fun invalid() -> array<i64, 1 - 2> { return []; }");
    FAIL("expected negative array length");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "array length must be non-negative, got -1");
  }
}
