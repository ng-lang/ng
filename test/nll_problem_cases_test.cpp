// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-16 (D-002 A1.2): move/borrow interplay. The classic NLL problem-corpus
// families land as contract tests here, one case family at a time:
//
//   1. move of a whole value while it is borrowed,
//   2. move/assign of a value at the same time a reference to it is live,
//   3. move of a member (field / element) while that member is borrowed,
//   4. assign through a borrowed member path.
//
// Each negative case is paired with a positive control showing the NLL
// semantics still release the loan after the last use (statement-grained).
// Runtime E2E exercises the positive interplay through the native tier.

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

TEST_CASE("INC-16 S1 case 1: move of a whole borrowed value", "[vNext][Borrow][NLL][INC-16][Errors]")
{
  expectBorrowConflict(
      R"(
fun main() -> i64 {
  let mut x = 5;
  let r = ref x;
  let y = move x;
  0
}
)",
      "cannot move `x` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-16 S1 case 1: moved borrowed value in a call argument", "[vNext][Borrow][NLL][INC-16][Errors]")
{
  expectBorrowConflict(
      R"(
struct Box { value: i64 }
fun take(b: Box) -> i64 { b.value }
fun main() -> i64 {
  let mut x = Box { value: 5 };
  let r = ref x;
  take(x)
}
)",
      "cannot move `x` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-16 S1 case 2: assign to a whole borrowed local", "[vNext][Borrow][NLL][INC-16][Errors]")
{
  expectBorrowConflict(
      R"(
fun main() -> i64 {
  let mut x = 5;
  let r = ref x;
  x := 9;
  0
}
)",
      "cannot assign to this local (of type i64) while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-16 S1 case 3: move of a borrowed field", "[vNext][Borrow][NLL][INC-16][Errors]")
{
  expectBorrowConflict(
      R"(
struct Box { value: i64 }
fun take(v: i64) -> i64 { v }
fun main() -> i64 {
  let mut b = Box { value: 1 };
  let r = ref b.value;
  take(b.value);
  0
}
)",
      "cannot move `b.value` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-16 S1 case 3: move of a borrowed array element", "[vNext][Borrow][NLL][INC-16][Errors]")
{
  expectBorrowConflict(
      R"(
fun main() -> i64 {
  let mut v = [1, 2, 3];
  let r = ref v[0];
  let taken = move v[0];
  taken
}
)",
      "cannot move `v[0]` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-16 S1 case 4: assign through a borrowed member path", "[vNext][Borrow][NLL][INC-16][Errors]")
{
  expectBorrowConflict(
      R"(
struct Box { value: i64 }
fun main() -> i64 {
  let mut b = Box { value: 1 };
  let r = ref b.value;
  b.value := 5;
  0
}
)",
      "cannot assign to `b.value` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-16 S1 case 4: assign through a borrowed index path", "[vNext][Borrow][NLL][INC-16][Errors]")
{
  expectBorrowConflict(
      R"(
fun main() -> i64 {
  let mut v = [1, 2];
  let r = ref v[0];
  v[0] := 9;
  0
}
)",
      "cannot assign to `v[0]` while it is borrowed (D-002 A1.2)");
}

TEST_CASE("INC-16 S1 positive: borrowing does not block subsequent reads", "[vNext][Borrow][NLL][INC-16]")
{
  check(R"(
struct Box { value: i64 }
fun main() -> i64 {
  let mut x = 5;
  let r = ref x;
  let peek = *r;
  peek + x
}
)");
}

TEST_CASE("INC-16 S1 positive: move after the loan's last use in a branch", "[vNext][Borrow][NLL][INC-16]")
{
  check(R"(
fun main() -> i64 {
  let mut x = 5;
  if (x > 0) {
    let r = ref x;
    let peek = *r;
  }
  let y = move x;
  y
}
)");
}

TEST_CASE("INC-16 S1 positive: mutable borrow after shared borrow ends (statement-grained)", "[vNext][Borrow][NLL][INC-16]")
{
  check(R"(
fun main() -> i64 {
  let mut x = 5;
  let r = ref x;
  let peek = *r;
  let write = ref mut x;
  *write := 7;
  x
}
)");
}

TEST_CASE("INC-16 S1 runtime: borrow/last-use interplay through the native tier", "[vNext][Borrow][NLL][INC-16]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(
import prelude;
fun main() {
  let mut x = 5;
  let r = ref x;
  print(*r == 5);
  let mut sum = x;
  let write = ref mut x;
  *write := 7;
  print(*write == 7);
  print(x == 7);
}
)",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue\ntrue"));
}