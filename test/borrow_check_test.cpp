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
  [[nodiscard]] auto resolve(std::string_view source) -> hir::Module
  {
    return hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
  }

  void check(std::string_view source)
  {
    static_cast<void>(typecheck::TypeChecker{}.check(resolve(source)));
  }
} // namespace

TEST_CASE("vNext references cannot be returned from functions", "[vNext][Borrow][Errors]")
{
  try
  {
    check("fun escape(value: i64 ref) -> i64 ref { return value; }");
    FAIL("expected a reference return error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "references cannot be returned from a function");
  }

  try
  {
    check("fun escape() -> i64 ref mut { let mut value = 1; let write = ref mut value; return write; }");
    FAIL("expected an indirect reference return error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "references cannot be returned from a function");
  }
}

TEST_CASE("vNext references cannot be stored in aggregates", "[vNext][Borrow][Errors]")
{
  try
  {
    check("fun main() { let mut value = 1; let write = ref mut value; let list = [write]; return; }");
    FAIL("expected an array storage error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "references cannot be stored in arrays");
  }

  try
  {
    check("fun main() { let mut value = 1; let write = ref mut value; let pair = (write, 1); return; }");
    FAIL("expected a tuple storage error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "references cannot be stored in tuples");
  }

  try
  {
    check("struct Holder { value: i64 ref }");
    FAIL("expected a struct field storage error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "references cannot be stored in struct fields");
  }
}

TEST_CASE("vNext shared and mutable borrows conflict on the same binding", "[vNext][Borrow][Errors]")
{
  try
  {
    check("fun main() { let mut value = 1; let read = ref value; let write = ref mut value; return; }");
    FAIL("expected a shared-then-mut conflict");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot mutably borrow `value` while it is shared-borrowed");
  }

  try
  {
    check("fun main() { let mut value = 1; let write = ref mut value; let read = ref value; return; }");
    FAIL("expected a mut-then-shared conflict");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot shared-borrow `value` while it is mutably borrowed");
  }

  try
  {
    check("fun main() { let mut value = 1; let first = ref mut value; let second = ref mut value; return; }");
    FAIL("expected a mut-mut conflict");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot mutably borrow `value` while it is already mutably borrowed");
  }
}

TEST_CASE("vNext borrows release at block scope and allow sequential reuse", "[vNext][Borrow][Runtime]")
{
  REQUIRE_NOTHROW(check("fun main() { let mut value = 1; "
                        "if (true) { let read = ref value; } "
                        "if (true) { let write = ref mut value; *write := 2; } "
                        "if (true) { let read = ref value; let other = ref value; } return; }"));
}

TEST_CASE("vNext non-lexical borrows release loans after last use", "[vNext][Borrow][NLL]")
{
  REQUIRE_NOTHROW(check("fun main() { let mut value = 1; let read = ref value; let seen = *read; "
                        "let write = ref mut value; *write := 2; let final = value; return; }"));

  REQUIRE_NOTHROW(check("fun main() { let mut value = 1; let a = ref mut value; *a := 5; "
                        "let b = ref mut value; *b := 6; let final = value; return; }"));

  REQUIRE_NOTHROW(check("fun main() { let mut value = 1; let read = ref value; "
                        "if (true) { let seen = *read; } "
                        "let write = ref mut value; *write := 3; let final = value; return; }"));

  REQUIRE_NOTHROW(check("fun main() { let mut value = 1; let read = ref value; "
                        "loop (i = 0) { let seen = *read; if (i == 0) { next (i + 1); } } "
                        "let write = ref mut value; *write := 4; let final = value; return; }"));

  REQUIRE_NOTHROW(check("fun main() { let mut value = 1; if (true) { let read = ref value; let seen = *read; } "
                        "let write = ref mut value; *write := 5; let final = value; return; }"));
}

TEST_CASE("vNext non-lexical borrows release inline call-site refs after the statement", "[vNext][Borrow][NLL]")
{
  REQUIRE_NOTHROW(check("fun bump(r: i64 ref mut) -> unit { *r := *r + 1; } "
                        "fun main() { let mut value = 1; bump(ref mut value); "
                        "let read = ref value; let seen = *read; return; }"));
}

TEST_CASE("vNext borrows still conflict when the shared ref outlives the mutable borrow", "[vNext][Borrow][NLL]")
{
  try
  {
    check("fun main() { let mut value = 1; let read = ref value; let write = ref mut value; "
          "let seen = *read; return; }");
    FAIL("expected a shared-then-mut conflict");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot mutably borrow `value` while it is shared-borrowed");
  }
}
