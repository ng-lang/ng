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
    typecheck::TypeChecker{}.check(module);
  }
} // namespace

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
