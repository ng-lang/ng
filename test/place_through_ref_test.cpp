// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-16 (D-002 A1.2/3): place-through-ref — member and index places behind a
// scoped reference that crosses a call boundary. `items[i]` and `box.value`
// work directly on `T ref` / `T ref mut` parameters (read, and write through
// the mutable form), while reads keep the deref (copy-first) value semantics.
//
// Positive: reads and writes through index/member on `ref` params (passed both
// inline and via a ref binding), plus runtime E2E through the native tier.
// Negative: writes through an immutable `ref` (resolver and checker layers),
// reference-through-reference creation and slicing (deferred to A1.3), and
// reassigning a scoped-reference binding.

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

TEST_CASE("INC-16 S2 positive: index reads through a ref parameter", "[vNext][Borrow][INC-16]")
{
  check(R"(
fun pick(items: array<i64> ref, i: i64) -> i64 { items[i] }
fun main() -> i64 {
  let mut xs = [10, 20, 30];
  pick(ref xs, 1)
}
)");
}

TEST_CASE("INC-16 S2 positive: member reads through a ref parameter", "[vNext][Borrow][INC-16]")
{
  check(R"(
struct Box { value: i64 }
fun read(b: Box ref) -> i64 { b.value }
fun main() -> i64 {
  let mut b = Box { value: 42 };
  read(ref b)
}
)");
}

TEST_CASE("INC-16 S2 positive: index writes through a ref mut parameter", "[vNext][Borrow][INC-16]")
{
  check(R"(
fun write(items: array<i64> ref mut, i: i64, v: i64) -> unit { items[i] := v; }
fun main() -> i64 {
  let mut xs = [10, 20, 30];
  write(ref mut xs, 0, 7);
  xs[0]
}
)");
}

TEST_CASE("INC-16 S2 positive: member writes through a ref mut parameter", "[vNext][Borrow][INC-16]")
{
  check(R"(
struct Box { value: i64 }
fun write(b: Box ref mut, v: i64) -> unit { b.value := v; }
fun main() -> i64 {
  let mut b = Box { value: 1 };
  write(ref mut b, 42);
  b.value
}
)");
}

TEST_CASE("INC-16 S2 positive: a ref binding passed to a ref parameter", "[vNext][Borrow][INC-16]")
{
  check(R"(
fun pick(items: array<i64> ref, i: i64) -> i64 { items[i] }
fun main() -> i64 {
  let mut xs = [10, 20, 30];
  let r = ref xs;
  pick(r, 1)
}
)");
}

TEST_CASE("INC-16 S2 negative: write through an immutable ref parameter", "[vNext][Borrow][INC-16][Errors]")
{
  try
  {
    check(R"(
fun write(items: array<i64> ref, i: i64, v: i64) -> unit { items[i] := v; }
)");
    FAIL("expected a resolution error for an immutable binding");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()}.find("cannot assign to immutable binding") != std::string::npos);
  }
}

TEST_CASE("INC-16 S2 negative: deref-write through an immutable ref binding", "[vNext][Borrow][INC-16][Errors]")
{
  try
  {
    check(R"(
fun main() -> i64 {
  let x = 5;
  let r = ref x;
  *r := 7;
  0
}
)");
    FAIL("expected an immutable-reference write error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("cannot assign through an immutable reference") != std::string::npos);
  }
}

TEST_CASE("INC-16 S2 negative: reference-through-reference is deferred (A1.3)", "[vNext][Borrow][INC-16][Errors]")
{
  try
  {
    check(R"(
fun main() -> i64 {
  let mut xs = [1, 2];
  let r = ref xs;
  let rr = ref r[0];
  0
}
)");
    FAIL("expected a reference-through-reference error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("creating a reference through a scoped reference is deferred (A1.3)") !=
            std::string::npos);
  }
}

TEST_CASE("INC-16 S2 negative: slicing through a scoped reference is deferred (A1.3)", "[vNext][Borrow][INC-16][Errors]")
{
  try
  {
    check(R"(
fun main() -> i64 {
  let mut xs = [1, 2, 3];
  let r = ref xs;
  let s = r[1..2];
  0
}
)");
    FAIL("expected a slice-through-reference error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("cannot slice through a scoped reference (A1.3 is deferred)") !=
            std::string::npos);
  }
}

TEST_CASE("INC-16 S2 negative: reassigning a scoped-reference binding", "[vNext][Borrow][INC-16][Errors]")
{
  try
  {
    check(R"(
fun main() -> i64 {
  let mut x = 5;
  let r = ref x;
  let mut s = ref x;
  s := ref x;
  0
}
)");
    FAIL("expected a reference-rebinding error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("cannot reassign a scoped reference binding") != std::string::npos);
  }
}

TEST_CASE("INC-16 S2 runtime: place-through-ref end to end", "[vNext][Borrow][INC-16]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(
import prelude;
struct Box { value: i64 }

fun read_el(items: array<i64> ref, i: i64) -> i64 { items[i] }

fun write_el(items: array<i64> ref mut, i: i64, v: i64) -> unit { items[i] := v; }

fun read_field(b: Box ref) -> i64 { b.value }

fun write_field(b: Box ref mut, v: i64) -> unit { b.value := v; }

fun main() {
  let mut xs = [10, 20, 30];
  let r = ref xs;
  print(read_el(r, 1) == 20);
  write_el(ref mut xs, 0, 7);
  print(xs[0] == 7);

  let mut b = Box { value: 99 };
  print(read_field(ref b) == 99);
  write_field(ref mut b, 42);
  print(b.value == 42);
}
)",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue\ntrue\ntrue"));
}