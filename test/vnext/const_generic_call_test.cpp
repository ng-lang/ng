// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/driver.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"

#include <sstream>

namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;

namespace
{
  void check(std::string_view source)
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    static_cast<void>(typecheck::TypeChecker{}.check(module));
  }
} // namespace

TEST_CASE("vNext type checker infers const generic arguments from array parameters", "[vNext][Typecheck][ConstGeneric]")
{
  REQUIRE_NOTHROW(check("fun head<const N: i64>(values: array<i64, N>) -> i64 { return values[0]; } "
                        "fun main() -> i64 { let values: array<i64, 2> = [1, 2]; return head(values); }"));
}

TEST_CASE("vNext type checker infers const generic arguments from expected return types", "[vNext][Typecheck][ConstGeneric]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun zeros<const N: i64>() -> array<i64, N> { } "
      "fun main() { let z: array<i64, 3> = zeros(); return; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto &call = *module.functions.at(1).body.statements.at(0).expression;
  const auto type = result.typeIdOf(call);
  REQUIRE(result.typeDescriptors.at(type.value).kind == typecheck::TypeKind::FixedArray);
  REQUIRE(result.typeDescriptors.at(type.value).element == typecheck::builtin::I64);
  REQUIRE(result.typeDescriptors.at(type.value).length == 3);
}

TEST_CASE("vNext type checker interns distinct const generic instances", "[vNext][Typecheck][ConstGeneric]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun zeros<const N: i64>() -> array<i64, N> { } "
      "fun first() { let a: array<i64, 2> = zeros(); return; } "
      "fun second() { let b: array<i64, 2> = zeros(); return; } "
      "fun third() { let c: array<i64, 4> = zeros(); return; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto first = result.typeIdOf(*module.functions.at(1).body.statements.at(0).expression);
  const auto second = result.typeIdOf(*module.functions.at(2).body.statements.at(0).expression);
  const auto third = result.typeIdOf(*module.functions.at(3).body.statements.at(0).expression);
  REQUIRE(first == second);
  REQUIRE(first != third);
  REQUIRE(result.typeDescriptors.at(third.value).length == 4);
}

TEST_CASE("vNext type checker instantiates mixed type and const generic parameters", "[vNext][Typecheck][ConstGeneric]")
{
  REQUIRE_NOTHROW(check("fun first<T, const N: i64>(values: array<T, N>) -> T { return values[0]; } "
                        "fun main() -> i64 { let values: array<i64, 2> = [1, 2]; return first(values); }"));
}

TEST_CASE("vNext type checker reuses consistent const bindings across parameters", "[vNext][Typecheck][ConstGeneric]")
{
  REQUIRE_NOTHROW(check(
      "fun sum<const N: i64>(a: array<i64, N>, b: array<i64, N>) -> i64 { return 0; } "
      "fun main() -> i64 { let a: array<i64, 2> = [1, 2]; let b: array<i64, 2> = [3, 4]; return sum(a, b); }"));
}

TEST_CASE("vNext type checker rejects inconsistent const generic array lengths", "[vNext][Typecheck][ConstGeneric]")
{
  try
  {
    check("fun sum<const N: i64>(a: array<i64, N>, b: array<i64, N>) -> i64 { return 0; } "
          "fun main() -> i64 { let a: array<i64, 2> = [1, 2]; let b: array<i64, 3> = [3, 4, 5]; return sum(a, b); }");
    FAIL("expected inconsistent const array lengths");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "generic array length mismatch");
  }
}

TEST_CASE("vNext type checker rejects calls that cannot infer const generic arguments", "[vNext][Typecheck][ConstGeneric]")
{
  try
  {
    check("fun zeros<const N: i64>() -> array<i64, N> { } fun main() { let z = zeros(); return; }");
    FAIL("expected uninferable const generic argument");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot infer const generic argument `N` for function `zeros`");
  }
}

TEST_CASE("vNext type checker accepts dependent array literals in const generic bodies", "[vNext][Typecheck][ConstGeneric]")
{
  REQUIRE_NOTHROW(check("fun fill<const N: i64>() -> array<i64, N> { return [1, 2, 3]; }"));
}

TEST_CASE("vNext explicit type arguments drive generic call instantiation", "[vNext][Typecheck][GenericCall]")
{
  std::string output;
  std::string errors;
  std::ostringstream outputStream;
  std::ostringstream errorStream;
  const int status = NG::vnext::runDriver(
      {"--source", "fun identity<T>(value: T) -> T { return value; } fun main() -> i64 { return identity<i64>(42); }"},
      outputStream, errorStream);
  output = std::move(outputStream).str();
  errors = std::move(errorStream).str();
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 42") != std::string::npos);
}

TEST_CASE("vNext explicit const arguments instantiate const generic calls", "[vNext][Typecheck][GenericCall]")
{
  std::string output;
  std::string errors;
  std::ostringstream outputStream;
  std::ostringstream errorStream;
  const int status = NG::vnext::runDriver(
      {"--source", "fun make<const N: i64>() -> array<i64, N> { return [1, 2, 3]; } "
                   "fun main() -> i64 { let values: array<i64, 3> = make<3>(); return values[2]; }"},
      outputStream, errorStream);
  output = std::move(outputStream).str();
  errors = std::move(errorStream).str();
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 3") != std::string::npos);
}

TEST_CASE("vNext explicit generic arguments report arity and kind mismatches", "[vNext][Typecheck][GenericCall]")
{
  try
  {
    check("fun identity<T>(value: T) -> T { return value; } fun main() { return identity<i64, string>(1); }");
    FAIL("expected a generic argument count mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "generic argument count mismatch: expected 1, got 2");
  }

  try
  {
    check("fun repeat<const N: i64>(value: i64) -> i64 { return value; } fun main() { return repeat<i64>(1); }");
    FAIL("expected a generic argument kind mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "generic argument 1 must be a const expression");
  }
}
