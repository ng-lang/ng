// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-15 (D-002 A1.1): origin contracts for returned scoped references.
// `T ref(a)` / `T ref(a | b)` annotate a returned reference with the function
// parameters it may originate from; every return path is checked against the
// declared origin set.
//
// * Positive contracts: borrow-into-param returns (`first`-like), multi-origin
//   branch returns (`choose`-like), whole-array reference passthrough, and a
//   single-origin identity reference.
// * Negative contracts: returning a parameter not in the origin set, returning
//   a borrowed local (origin is a non-parameter), a non-parameter declared
//   origin, and the INC-14 baseline (undeclared-origin references are still
//   non-returnable).
// * Runtime E2E: `first`/`choose` compile and execute through the native tier.

#include "test.hpp"

#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <string_view>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  [[nodiscard]] auto resolve(std::string_view source) -> hir::Module
  {
    return hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
  }

  void check(std::string_view source)
  {
    static_cast<void>(typecheck::TypeChecker{}.check(resolve(source)));
  }

  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--source", source}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("INC-15 positive: choose returns either origin on a branch", "[vNext][Origin][INC-15]")
{
  check(R"(
struct Box { value: i64 }
fun choose(a: Box ref, b: Box ref, prefer_a: bool) -> Box ref(a | b) {
  if (prefer_a) { return a; }
  return b;
}
)");
}

TEST_CASE("INC-15 positive: first returns a borrow into a parameter array", "[vNext][Origin][INC-15]")
{
  check(R"(
fun first(items: array<i64>, idx: i64) -> i64 ref(items) {
  return ref items[0];
}
)");
}

TEST_CASE("INC-15 positive: whole scoped reference passes through under its origin", "[vNext][Origin][INC-15]")
{
  check(R"(
struct Box { value: i64 }
fun pass_through(items: array<Box> ref) -> array<Box> ref(items) {
  return items;
}
)");
}

TEST_CASE("INC-15 positive: single-origin identity reference", "[vNext][Origin][INC-15]")
{
  check(R"(
struct Box { value: i64 }
fun pick(a: Box ref, b: Box ref) -> Box ref(a) {
  return a;
}
)");
}

TEST_CASE("INC-15 positive: same-origin across functions keeps per-function ordinals", "[vNext][Origin][INC-15]")
{
  check(R"(
struct Box { value: i64 }
fun f1(a: Box ref, b: Box ref) -> Box ref(a) { return a; }
fun f2(b: Box ref, a: Box ref) -> Box ref(a) { return a; }
fun f3(a: Box ref, b: Box ref) -> Box ref(b) { return b; }
)");
}

TEST_CASE("INC-15 negative: returning a parameter outside the origin set", "[vNext][Origin][INC-15][Errors]")
{
  try
  {
    check(R"(
struct Box { value: i64 }
fun wrong(a: Box ref, b: Box ref, prefer_a: bool) -> Box ref(a) {
  if (prefer_a) { return a; }
  return b;
}
)");
    FAIL("expected an origin contract error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("does not satisfy the declared origin(s) `a`") != std::string::npos);
  }
}

TEST_CASE("INC-15 negative: returning a borrowed local (non-parameter origin)", "[vNext][Origin][INC-15][Errors]")
{
  try
  {
    check(R"(
struct Box { value: i64 }
fun bad(a: Box ref, use_a: bool) -> Box ref(a) {
  let mut local = Box { value: 2 };
  if (use_a) { return a; }
  return ref local;
}
)");
    FAIL("expected an origin contract error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("does not satisfy the declared origin(s) `a`") != std::string::npos);
  }
}

TEST_CASE("INC-15 negative: declared origin is not a parameter", "[vNext][Origin][INC-15][Errors]")
{
  try
  {
    check(R"(
struct Box { value: i64 }
fun bad() -> Box ref(x) {
  let mut x = Box { value: 1 };
  return ref x;
}
)");
    FAIL("expected a resolution error");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()} ==
            "reference origin `x` is not a parameter of the enclosing function");
  }
}

TEST_CASE("INC-15 negative: undeclared-origin references remain non-returnable", "[vNext][Origin][INC-15][Errors]")
{
  try
  {
    check("fun escape(value: i64 ref) -> i64 ref { return value; }");
    FAIL("expected an undeclared-origin return error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("references cannot be returned from a function") != std::string::npos);
  }
}

TEST_CASE("INC-15 runtime: first and choose compiles and runs end to end", "[vNext][Origin][INC-15]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(
import prelude;
struct Box { value: i64 }

fun first(items: array<i64>, idx: i64) -> i64 ref(items) {
  return ref items[0];
}

fun choose(a: Box ref, b: Box ref, prefer_a: bool) -> Box ref(a | b) {
  if (prefer_a) { return a; }
  return b;
}

fun main() {
  let mut xs = [10, 20, 30];
  let r = first(xs, 0);
  print(*r == 10);

  let mut x = Box { value: 7 };
  let mut y = Box { value: 8 };
  let ch = choose(ref x, ref y, true);
  print((*ch).value == 7);
}
)",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue"));
}