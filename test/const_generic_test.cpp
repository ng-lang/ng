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

TEST_CASE("vNext type checker resolves const-generic array lengths to dependent arrays", "[vNext][Typecheck][ConstGeneric]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun repeat<const N: i64>(value: i64) -> array<i64, N> { }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  REQUIRE(module.functions.front().constParameters.size() == 1);
  REQUIRE(module.functions.front().constParameters[0].name == "N");

  const auto result = typecheck::TypeChecker{}.check(module);
  const auto returnType = result.functionTypeIds.at(0).returnType;
  REQUIRE(result.typeDescriptors.at(returnType.value).kind == typecheck::TypeKind::DependentArray);
  REQUIRE(result.typeDescriptors.at(returnType.value).element == typecheck::builtin::I64);
  REQUIRE(result.typeDescriptors.at(returnType.value).constParameterIndex == 0);
  REQUIRE(result.typeDescriptors.at(returnType.value).constParameterName == "N");
}

TEST_CASE("vNext type checker accepts mixed type and const generic parameters", "[vNext][Typecheck][ConstGeneric]")
{
  REQUIRE_NOTHROW(check("fun mix<T, const N: i64>(value: T) -> array<T, N> { }"));

  const auto syntaxUnit = syntax::parseSourceUnit("fun mix<T, const N: i64>(value: T) -> array<T, N> { }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto returnType = result.functionTypeIds.at(0).returnType;
  REQUIRE(result.typeDescriptors.at(returnType.value).kind == typecheck::TypeKind::DependentArray);
  REQUIRE(result.typeDescriptors.at(returnType.value).element == result.functionTypeIds.at(0).genericParameters.at(0));
}

TEST_CASE("vNext type checker rejects non-integer const generic parameters", "[vNext][Typecheck][ConstGeneric]")
{
  try
  {
    check("fun invalid<const N: bool>(value: i64) -> array<i64, N> { }");
    FAIL("expected const generic parameter type validation");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const generic parameter `N` must be i64, got bool");
  }
}

TEST_CASE("vNext type checker rejects const parameters used as types", "[vNext][Typecheck][ConstGeneric]")
{
  try
  {
    check("fun invalid<const N: i64>(value: N) { }");
    FAIL("expected const parameter used as type to fail");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "unknown type `N`");
  }
}

TEST_CASE("vNext type checker rejects unknown const names in array lengths", "[vNext][Typecheck][ConstGeneric]")
{
  try
  {
    check("fun invalid() -> array<i64, Missing> { }");
    FAIL("expected unresolved const name");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "unresolved const name `Missing`");
  }
}
