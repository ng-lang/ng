// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-17 (M2b): borrow-capturing closures. An explicit capture of a scoped
// reference (`[r] || { ... }`) turns the closure into an anonymous borrower of
// the referent: the carried loan lives as long as any bearer does, where the
// bearers are the ref binding AND every closure capturing it. This is the
// INC-16 (D-002 A1.2) NLL corpus replayed as closure-capture variants, plus
// the multi-bearer release semantics M2b adds:
//
//   - move/assign of the referent while a capturing closure is alive is
//     rejected (same E-borrow-conflict diagnostics as INC-16),
//   - the loan releases only at the last surviving bearer's last use, so a
//     closure that outlives the ref binding still guards the referent,
//   - a capturing closure that is never called releases its loan at its
//     declaration (closures cannot escape their scope, INC-14),
//   - runtime E2E exercises writes through a captured `ref mut`.

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

namespace
{
  void expectBorrowConflict(std::string_view source, std::string_view needle)
  {
    try
    {
      check(source);
      FAIL("expected a borrow-conflict error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()}.find(needle) != std::string::npos);
    }
  }
} // namespace

TEST_CASE("INC-17 S1 case 1: move of the referent while a capturing closure is alive",
          "[vNext][Borrow][NLL][Closure][INC-17][Errors]")
{
  expectBorrowConflict(
      R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let f = [r] || { *r };
  let y = move s;
  let z = f().v;
  z + y.v
}
)",
      "cannot move `s` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-17 S1 case 1: move of the referent while a shared-capture closure is alive",
          "[vNext][Borrow][NLL][Closure][INC-17][Errors]")
{
  expectBorrowConflict(
      R"(
import prelude;
fun main() -> i64 {
  let mut x: i64 = 5;
  let r = ref x;
  let f = [r] || { *r };
  let y = move x;
  y + f()
}
)",
      "cannot move `x` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-17 S1 case 2: assign to the whole referent while a capturing refmut closure is alive",
          "[vNext][Borrow][NLL][Closure][INC-17][Errors]")
{
  expectBorrowConflict(
      R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let f = [r] || { *r };
  s := S { v: 99 };
  f().v
}
)",
      "cannot assign to this local (of type S) while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-17 S1 case 4: write through a captured immutable ref is rejected",
          "[vNext][Borrow][NLL][Closure][INC-17][Errors]")
{
  expectBorrowConflict(
      R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref s;
  let f = [r] || { r.v := 9; *r };
  f().v
}
)",
      "cannot assign through an immutable reference");
}

TEST_CASE("INC-17 S1 positive: move after the capturing closure's last call", "[vNext][Borrow][NLL][Closure][INC-17]")
{
  check(R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let f = [r] || { *r };
  let z = f().v;
  let y = move s;
  z + y.v
}
)");
}

TEST_CASE("INC-17 S1 positive: assign after the capturing closure's last call", "[vNext][Borrow][NLL][Closure][INC-17]")
{
  check(R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let f = [r] || { *r };
  let z = f().v;
  s := S { v: 99 };
  z + s.v
}
)");
}

TEST_CASE("INC-17 M2b positive: an unused capturing closure releases its loan at its declaration",
          "[vNext][Borrow][NLL][Closure][INC-17]")
{
  check(R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let f = [r] || { *r };
  s := S { v: 42 };
  s.v
}
)");
}

TEST_CASE("INC-17 M2b: the loan survives while ANY capturing closure is alive", "[vNext][Borrow][NLL][Closure][INC-17]")
{
  expectBorrowConflict(
      R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let h = [r] || { *r };
  let y = move s;
  h().v
}
)",
      "cannot move `s` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-17 M2b positive: two capturing closures release only after the last one's last call",
          "[vNext][Borrow][NLL][Closure][INC-17]")
{
  check(R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let h = [r] || { *r };
  let k = [r] || { r.v := 2 * r.v; *r };
  let a = h();
  let b = k();
  let y = move s;
  a.v + b.v + y.v
}
)");
}

TEST_CASE("INC-17 M2b positive: ref mut referent after a dead shared-capture closure",
          "[vNext][Borrow][NLL][Closure][INC-17]")
{
  check(R"(
import prelude;
struct S { v: i64 }
fun main() -> i64 {
  let mut s = S { v: 5 };
  let r = ref s;
  let f = [r] || { *r };
  let z = f().v;
  let write = ref mut s;
  (*write).v := 9;
  z + s.v
}
)");
}

TEST_CASE("INC-17 M2b runtime: writes through a captured ref mut reach the referent", "[vNext][Borrow][NLL][Closure][INC-17]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(
import prelude;
struct S { v: i64 }
fun main() -> unit {
  let mut s = S { v: 5 };
  let r = ref mut s;
  let f = [r] || { r.v := 30; *r };
  print(f().v == 30);
  print((*r).v == 30);
  let g = [r] || { r.v := r.v + 12; *r };
  print(g().v == 42);
  print(s.v == 42);
}
)",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue\ntrue\ntrue"));
}