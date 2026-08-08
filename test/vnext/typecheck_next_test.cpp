// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"

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

TEST_CASE("vNext type checker exposes immutable expression type side tables", "[vNext][Typecheck]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun entry(value: i64) -> i64 { return value + 1; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto &returnExpression = *module.functions.front().body.statements.front().expression;
  REQUIRE(result.typeOf(returnExpression) == "i64");
  REQUIRE(result.typeIdOf(returnExpression) == typecheck::builtin::I64);
  REQUIRE(result.expressionTypes.size() == 3);
  REQUIRE(result.expressionTypeIds.size() == 3);
  REQUIRE(result.localTypes.at(module.functions.front().parameters.front().local.value) == "i64");
  REQUIRE(result.localTypeIds.at(module.functions.front().parameters.front().local.value) == typecheck::builtin::I64);
  REQUIRE(result.functionTypes.at(module.functions.front().id.value).parameters == std::vector<std::string>{"i64"});
  REQUIRE(result.functionTypes.at(module.functions.front().id.value).returnType == "i64");
}

TEST_CASE("vNext type checker validates loop and tail next arguments", "[vNext][Typecheck][Next]")
{
  REQUIRE_NOTHROW(check(
      "fun step(seed: i64) -> i64 { loop (index = seed, total = 1) { if index < 3 { next (index + 1, total + index); } } next (seed); }"));
}

TEST_CASE("vNext type checker rejects loop next arity mismatch", "[vNext][Typecheck][Next]")
{
  try
  {
    check("fun invalid() { loop (left = 1, right = 2) { next (left); } }");
    FAIL("expected loop next arity mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "next argument count mismatch: expected 2, got 1");
    REQUIRE(error.span.begin == 45);
    REQUIRE(error.span.end == 57);
  }
}

TEST_CASE("vNext type checker rejects loop next type mismatch", "[vNext][Typecheck][Next]")
{
  try
  {
    check("fun invalid(seed: u8) { loop (state = seed) { next (1); } }");
    FAIL("expected loop next type mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "next argument 1 type mismatch: expected u8, got i64");
    REQUIRE(error.span.begin == 52);
    REQUIRE(error.span.end == 53);
  }
}

TEST_CASE("vNext type checker validates function call and boolean condition contracts", "[vNext][Typecheck][Call]")
{
  REQUIRE_NOTHROW(check(
      "fun increment(value: i64) -> i64 { return value + 1; } fun entry() -> i64 { if true { return increment(1); } return 0; }"));
}

TEST_CASE("vNext type checker rejects non-boolean if conditions", "[vNext][Typecheck][Call]")
{
  try
  {
    check("fun invalid() { if 1 { return; } }");
    FAIL("expected non-boolean if condition to fail");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "if condition type mismatch: expected bool, got i64");
    REQUIRE(error.span.begin == 19);
    REQUIRE(error.span.end == 20);
  }
}

TEST_CASE("vNext type checker rejects call arity and argument type mismatch", "[vNext][Typecheck][Call]")
{
  try
  {
    check("fun target(left: i64, right: i64) { } fun invalid() { target(1); }");
    FAIL("expected call arity mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call argument count mismatch: expected 2, got 1");
    REQUIRE(error.span.begin == 54);
  }

  try
  {
    check("fun target(value: u8) { } fun invalid() { target(1); }");
    FAIL("expected call argument type mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call argument 1 type mismatch: expected u8, got i64");
    REQUIRE(error.span.begin == 49);
    REQUIRE(error.span.end == 50);
  }
}

TEST_CASE("vNext type checker interns dynamic and fixed arrays as distinct types", "[vNext][Typecheck]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun dynamic() -> array<i64> { return [1, 2, 3]; } "
      "fun fixedThree() -> array<i64, 3> { return [1, 2, 3]; } "
      "fun fixedFour() -> array<i64, 4> { return [1, 2, 3, 4]; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto dynamic = result.functionTypeIds.at(0).returnType;
  const auto fixedThree = result.functionTypeIds.at(1).returnType;
  const auto fixedFour = result.functionTypeIds.at(2).returnType;
  REQUIRE(dynamic != fixedThree);
  REQUIRE(fixedThree != fixedFour);
  REQUIRE(result.typeDescriptors.at(dynamic.value).kind == typecheck::TypeKind::DynamicArray);
  REQUIRE(result.typeDescriptors.at(fixedThree.value).kind == typecheck::TypeKind::FixedArray);
  REQUIRE(result.typeDescriptors.at(fixedThree.value).length == 3);
  REQUIRE(result.typeDescriptors.at(fixedFour.value).length == 4);
}

TEST_CASE("vNext type checker validates homogeneous and fixed-length array literals", "[vNext][Typecheck]")
{
  REQUIRE_NOTHROW(check("fun values() -> array<i64> { return [1, 2]; }"));
  REQUIRE_NOTHROW(check("fun values() -> array<i64, 2> { return [1, 2]; }"));
  REQUIRE_THROWS_WITH(check("fun invalid() -> array<i64> { return [1, true]; }"),
                      "array element type mismatch: expected i64, got bool");
  REQUIRE_THROWS_WITH(check("fun invalid() -> array<i64, 3> { return [1, 2]; }"),
                      "fixed array length mismatch: expected 3, got 2");
}

TEST_CASE("vNext type checker uses local annotations as constructor context", "[vNext][Typecheck]")
{
  REQUIRE_NOTHROW(check(
      "enum Result<T, E> { Ok(value: T), Err(error: E) } "
      "fun local() -> Result<i64, string> { let result: Result<i64, string> = Result.Ok(7); return result; }"));
}

TEST_CASE("vNext type checker interns generic enum instances", "[vNext][Typecheck]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "enum Result<T, E> { Ok(value: T), Err(error: E) } "
      "fun numbers() -> Result<i64, string> { return Result.Ok(7); } "
      "fun bytes(value: u8) -> Result<u8, string> { return Result.Ok(value); }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto numbers = result.functionTypeIds.at(0).returnType;
  const auto bytes = result.functionTypeIds.at(1).returnType;
  REQUIRE(numbers != bytes);
  REQUIRE(result.typeDescriptors.at(numbers.value).kind == typecheck::TypeKind::Enum);
  REQUIRE(result.typeDescriptors.at(numbers.value).elements[0] == typecheck::builtin::I64);
  REQUIRE(result.typeDescriptors.at(bytes.value).elements[0] == typecheck::builtin::U8);
  REQUIRE_NOTHROW(check(
      "enum Box<T> { Value(value: array<T>) } "
      "fun boxed() -> Box<i64> { return Box.Value([1, 2]); }"));
  REQUIRE_NOTHROW(check(
      "enum Result<T, E> { Ok(value: T), Err(error: E) } "
      "enum Nested<T> { Value(value: Result<T, string>) } "
      "fun nested() -> Nested<i64> { return Nested.Value(Result.Ok(7)); }"));
  REQUIRE_THROWS_WITH(check("enum Result<T, E> { Ok(value: T), Err(error: E) } fun ok() -> Result<i64, string> { return Result.Ok(true); }"),
                      "variant payload type mismatch: expected i64, got bool");
  REQUIRE_THROWS_WITH(check("enum Result<T, E> { Ok(value: T), Err(error: E) } fun invalid(value: Result) { return; }"),
                      "enum type `Result` expects 2 arguments, got 0");
  REQUIRE_THROWS_WITH(check("enum Result<T, E> { Ok(value: T), Err(error: E) } fun invalid() { let value = Result.Ok(1); return; }"),
                      "cannot infer generic arguments for enum constructor `Result`");
}

TEST_CASE("vNext type checker validates nominal enum constructors", "[vNext][Typecheck]")
{
  REQUIRE_NOTHROW(check("enum Result { Ok(i64), Error(string), Empty } fun ok() -> Result { return Result.Ok(7); }"));
  REQUIRE_NOTHROW(check("enum Result { Ok(i64), Error(string), Empty } fun empty() -> Result { return Result.Empty; }"));
  REQUIRE_THROWS_WITH(check("enum Result { Ok(i64), Empty } fun invalid() -> Result { return Result.Ok(); }"),
                      "enum variant `Ok` expects 1 payload values, got 0");
  REQUIRE_THROWS_WITH(check("enum Result { Ok(i64), Empty } fun invalid() -> Result { return Result.Empty(1); }"),
                      "enum variant `Empty` expects 0 payload values, got 1");
  REQUIRE_THROWS_WITH(check("enum Result { Ok(i64) } fun invalid() -> Result { return Result.Ok(true); }"),
                      "variant payload type mismatch: expected i64, got bool");
}

TEST_CASE("vNext type checker validates nominal structs and member places", "[vNext][Typecheck]")
{
  REQUIRE_NOTHROW(check("struct Point { x: i64, label: string } fun read() -> i64 { let point = Point { x: 7, label: \"p\" }; return point.x; }"));
  REQUIRE_NOTHROW(check("struct Point { x: i64, label: string } fun update() -> i64 { let mut point = Point { x: 7, label: \"p\" }; point.x := 9; return point.x; }"));
  REQUIRE_THROWS_WITH(check("struct Point { x: i64, label: string } fun invalid() { return Point { x: 1 }; }"),
                      "missing field in struct `Point`");
  REQUIRE_THROWS_WITH(check("struct Point { x: i64, label: string } fun invalid() { return Point { x: true, label: \"p\" }; }"),
                      "field `x` type mismatch: expected i64, got bool");
  REQUIRE_THROWS_WITH(check("struct Point { x: i64, label: string } fun invalid() { let point = Point { x: 1, label: \"p\" }; return point.z; }"),
                      "unknown field `z` in struct `Point`");
}

TEST_CASE("vNext type checker validates tuple destructuring", "[vNext][Typecheck]")
{
  REQUIRE_NOTHROW(check("fun values() -> bool { let mut (number, flag) = (1, false); flag := true; return flag; }"));
  REQUIRE_THROWS_WITH(check("fun invalid() { let (first, second) = (1, true, \"extra\"); return; }"),
                      "tuple destructuring length mismatch: expected 3, got 2");
  REQUIRE_THROWS_WITH(check("fun invalid() { let (first, second) = [1, 2]; return; }"),
                      "cannot destructure value of type array<i64>");
}

TEST_CASE("vNext type checker interns structural tuples and validates projections", "[vNext][Typecheck]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun pair() -> tuple<i64, bool, string> { return (1, false, \"value\"); }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto tuple = result.functionTypeIds.at(0).returnType;
  REQUIRE(result.typeDescriptors.at(tuple.value).kind == typecheck::TypeKind::Tuple);
  REQUIRE(result.typeDescriptors.at(tuple.value).elements ==
          std::vector<typecheck::TypeId>{typecheck::builtin::I64, typecheck::builtin::Bool, typecheck::builtin::String});

  REQUIRE_NOTHROW(check("fun second() -> bool { let value = (1, true); return value.1; }"));
  REQUIRE_THROWS_WITH(check("fun invalid(index: i64) -> i64 { let value = (1, true); return value[index]; }"),
                      "tuple index must be an integer literal");
  REQUIRE_THROWS_WITH(check("fun invalid() -> i64 { let value = (1, true); return value.2; }"),
                      "tuple index 2 is out of range for length 2");
}

TEST_CASE("vNext type checker accepts string literals and concatenation", "[vNext][Typecheck]")
{
  REQUIRE_NOTHROW(check("fun greeting() -> string { return \"hello\" + \" world\"; }"));
}

TEST_CASE("vNext type checker rejects unknown type annotations", "[vNext][Typecheck]")
{
  try
  {
    check("fun invalid(value: imaginary) { return; }");
    FAIL("expected unknown type to fail");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "unknown type `imaginary`");
    REQUIRE(error.span.begin == 19);
    REQUIRE(error.span.end == 28);
  }
}

TEST_CASE("vNext type checker validates array indexing and rejects unsupported postfix operations", "[vNext][Typecheck]")
{
  REQUIRE_NOTHROW(check("fun first() -> i64 { let values = [1, 2]; return values[0]; }"));
  REQUIRE_THROWS_WITH(check("fun invalid() { let value = 1; value(); }"), "call target is not a function");
  REQUIRE_THROWS_WITH(check("fun invalid() { let value = 1; return value[0]; }"), "cannot index value of type i64");
  REQUIRE_THROWS_WITH(check("fun invalid() { let value = 1; return value.field; }"), "cannot access member `field` on value of type i64");
}

TEST_CASE("vNext type checker rejects tail next arity mismatch", "[vNext][Typecheck][Next]")
{
  try
  {
    check("fun invalid(first: i64, second: i64) { next (first); }");
    FAIL("expected tail next arity mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "next argument count mismatch: expected 2, got 1");
    REQUIRE(error.span.begin == 39);
    REQUIRE(error.span.end == 52);
  }
}
