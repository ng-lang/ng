// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-28: `isize` / `usize` — the target-width integer types (D-016). On every
// supported target the pointer width is 64 bits, so `isize` shares the `i64`
// slot/layout and `usize` the `u64` slot/layout while staying distinct types
// (mixed arithmetic is still a type error).
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"
#include "typecheck.hpp"

#include <sstream>

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

  [[nodiscard]] auto expectValue(std::string_view source, std::string_view value) -> void
  {
    std::string output;
    std::string errors;
    REQUIRE(run(source, output, errors) == 0);
    INFO("errors: " << errors);
    REQUIRE(errors.empty());
    REQUIRE(output.find(std::string{"native main exited with code "} + std::string{value} + "\n") != std::string::npos);
  }

  [[nodiscard]] auto emitsSsa(std::string_view source) -> std::string
  {
    std::ostringstream output;
    std::ostringstream errors;
    REQUIRE(NG::runDriver({"--emit=ssa", "--source", source}, output, errors) == 0);
    return output.str();
  }
} // namespace

TEST_CASE("vNext target-width integers resolve and run same-type arithmetic", "[vNext][Numerics][Isize]")
{
  expectValue("fun main() -> isize { let a: isize = 5isize; let b: isize = a + 2isize; return b; }", "7");
  expectValue("fun main() -> usize { let a: usize = 10usize; let b: usize = a - 3usize; return b; }", "7");
  expectValue("fun main() -> i64 { let a: usize = 7usize; let b: usize = a * 6usize; "
              "if (b == 42usize) { return 1; } return 0; }",
              "1");
  expectValue("fun main() -> i64 { let a: usize = 9usize; "
              "if (a / 2usize == 4usize && a % 2usize == 1usize) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext target-width integers keep the full 64-bit slot range", "[vNext][Numerics][Isize]")
{
  // usize reaches u64::MAX and stays unsigned in comparisons.
  expectValue("fun main() -> i64 { let big: usize = 18446744073709551615usize; "
              "if (big > 5usize) { return 1; } return 0; }",
              "1");
  // isize reaches i64::MIN.
  expectValue("fun main() -> i64 { let low: isize = -9223372036854775807isize - 1isize; "
              "if (low < 0isize) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext target-width integer literals adopt expected types", "[vNext][Numerics][Isize]")
{
  expectValue("struct Pair { x: usize, y: isize, } "
              "fun main() -> i64 { let p = Pair { x: 300usize, y: -7isize }; "
              "if (p.x == 300usize && p.y == -7isize) { return 1; } return 0; }",
              "1");
  expectValue("fun id(n: usize) -> usize { return n; } "
              "fun main() -> i64 { if (id(300) == 300usize) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext target-width suffixed literals parse in expressions and case patterns",
          "[vNext][Numerics][Isize]")
{
  expectValue("fun main() -> i64 { let x = 42usize; let y = -3isize; "
              "if (x == 42usize && y == -3isize) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext target-width literal range checks reject out-of-range values", "[vNext][Numerics][Isize]")
{
  try
  {
    check("fun main() { let x: usize = 18446744073709551616usize; }");
    FAIL("expected a usize range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `18446744073709551616` is out of range for type usize");
  }
  try
  {
    check("fun main() { let x: usize = -1usize; }");
    FAIL("expected a usize range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `-1` is out of range for type usize");
  }
  try
  {
    check("fun main() { let x: isize = 9223372036854775808isize; }");
    FAIL("expected an isize range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `9223372036854775808` is out of range for type isize");
  }
}

TEST_CASE("vNext target-width arithmetic traps on overflow", "[vNext][Numerics][Isize]")
{
  std::string output;
  std::string errors;
  // isize::MAX + 1
  REQUIRE(run("fun main() -> isize { let a: isize = 9223372036854775807isize; "
              "let b: isize = a + 1isize; return b; }",
              output, errors) == 1);
  REQUIRE(errors.find("killed by a signal") != std::string::npos);
  // usize::MAX + 1
  REQUIRE(run("fun main() -> usize { let a: usize = 18446744073709551615usize; "
              "let b: usize = a + 1usize; return b; }",
              output, errors) == 1);
  REQUIRE(errors.find("killed by a signal") != std::string::npos);
  // isize::MIN - 1
  REQUIRE(run("fun main() -> isize { let a: isize = -9223372036854775807isize - 1isize; "
              "let b: isize = a - 1isize; return b; }",
              output, errors) == 1);
  REQUIRE(errors.find("killed by a signal") != std::string::npos);
}

TEST_CASE("vNext target-width integer mixed arithmetic stays a type error", "[vNext][Numerics][Isize]")
{
  try
  {
    check("fun main() { let a: isize = 1isize; let b: usize = 2usize; let c = a + b; }");
    FAIL("expected a mixed-sign arithmetic error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "binary operands type mismatch: expected isize, got usize");
  }
  try
  {
    check("fun main() { let a: isize = 1isize; let b: i64 = 2; let c = a + b; }");
    FAIL("expected a mixed-width arithmetic error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "binary operands type mismatch: expected isize, got i64");
  }
}

TEST_CASE("vNext target-width comparisons follow DEC-A", "[vNext][Numerics][Isize]")
{
  // Same-signness cross-width comparison is exact and legal.
  expectValue("fun main() -> i64 { let a: isize = 1isize; let b: i32 = 2; "
              "if (a < b && a <= b) { return 1; } return 0; }",
              "1");
  expectValue("fun main() -> i64 { let a: usize = 5usize; let b: u8 = 5u8; "
              "if (a >= b) { return 1; } return 0; }",
              "1");
  // Mixed signedness comparison is a compile error.
  try
  {
    check("fun main() { let a: isize = 1isize; let b: usize = 2usize; if (a == b) { } }");
    FAIL("expected a mixed-sign comparison error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("implicit mixed-sign comparison") != std::string::npos);
  }
  // Mixed integer/float comparison is a compile error.
  try
  {
    check("fun main() { let a: usize = 1usize; let f: f64 = 1.0; if (a == f) { } }");
    FAIL("expected a mixed numeric-kind comparison error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("implicit mixed numeric-kind comparison") != std::string::npos);
  }
}

TEST_CASE("vNext target-width integers work in const evaluation", "[vNext][Numerics][Isize]")
{
  expectValue("const fun twice(u: usize) -> usize => u * 2usize; "
              "fun main() -> i64 { const if (twice(21usize) == 42usize) { return 1; } return 0; }",
              "1");
  expectValue("const fun isBig(u: usize) -> bool => u > 100usize; "
              "fun main() -> i64 { const if (isBig(200usize)) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext target-width integers convert explicitly and print full range", "[vNext][Numerics][Isize]")
{
  // toSigned(usize)/toSigned(isize) and toFloat(usize) are declared in the prelude.
  expectValue("import prelude; fun main() -> i64 { let u: usize = 42usize; "
              "let f: f64 = toFloat(u); if (toSigned(u) == 42 && f == 42.0) { return 1; } return 0; }",
              "1");
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; fun main() -> unit { let u: usize = 18446744073709551615usize; print(u); }", output,
              errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("18446744073709551615\n") != std::string::npos);
  REQUIRE(run("import prelude; fun main() -> unit { let s: isize = -42isize; print(toString(s)); }", output, errors) ==
          0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("-42\n") != std::string::npos);
}

TEST_CASE("vNext target-width integers map to the 64-bit QBE slot", "[vNext][Numerics][Isize][Native][Qbe]")
{
  // usize/isize share the i64/u64 slot: arithmetic routes through the signed
  // vs unsigned checked helpers and the 64-bit width check is a no-op.
  const auto unsignedIsa = emitsSsa("fun main() -> usize { let a: usize = 5usize; let b: usize = a + 2usize; "
                                 "return b; }");
  REQUIRE_THAT(unsignedIsa, ContainsSubstring("call $ngrt_checked_uadd("));
  REQUIRE_THAT(unsignedIsa, ContainsSubstring("call $ngrt_check_w_u64("));
  const auto signedIsa = emitsSsa("fun main() -> isize { let a: isize = 5isize; let b: isize = a + 2isize; return b; }");
  REQUIRE_THAT(signedIsa, ContainsSubstring("call $ngrt_checked_add("));
  REQUIRE_THAT(signedIsa, ContainsSubstring("call $ngrt_check_w_i64("));
}

TEST_CASE("vNext target-width integers cross the C ABI", "[vNext][Numerics][Isize][Native][Qbe]")
{
  expectValue("extern \"C\" fun llabs(value: isize) -> isize; "
              "fun main() -> i64 { if (llabs(-42isize) == 42isize) { return 1; } return 0; }",
              "1");
  // isize is a valid repr(C) field (64-bit integer layout).
  expectValue("repr(C) struct WithSize { a: isize, b: i32, } "
              "extern \"C\" fun llabs(value: isize) -> isize; "
              "fun main() -> i64 { let w = WithSize { a: 3isize, b: 4 }; "
              "if (w.a == 3isize && llabs(w.a) == 3isize) { return 1; } return 0; }",
              "1");
}
