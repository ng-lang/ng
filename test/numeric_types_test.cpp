// AI-generated code; reviewed for this repository's vNext rewrite.
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"
#include "typecheck.hpp"

#include <filesystem>
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

  [[nodiscard]] auto runExample(std::string_view filename, std::string &output, std::string &errors) -> int
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example"))
      path = std::string{"../"} + path;
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({path}, outputStream, errorStream);
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
    REQUIRE(output.find(std::string{"with value "} + std::string{value}) != std::string::npos);
  }
} // namespace

TEST_CASE("vNext integer types resolve and run same-type arithmetic", "[vNext][Numerics][Runtime]")
{
  expectValue("fun add(a: i32, b: i32) -> i32 { return a + b; } "
              "fun main() -> i8 { let small: i8 = 7; let smallSum: i8 = small + 1; return smallSum; }",
              "8");
}

TEST_CASE("vNext integer literals adopt contextual types in aggregates and calls", "[vNext][Numerics][Runtime]")
{
  expectValue("struct Point { x: i16, y: u32, } "
              "fun main() -> i16 { let pair = Point { x: 300, y: 4000 }; return pair.x; }",
              "300");
}

TEST_CASE("vNext typed range literals slice arrays", "[vNext][Numerics][Runtime]")
{
  expectValue("fun main() -> i32 { let xs: array<i32> = [10, 20, 30]; "
              "let r: range<i32> = 1..3; let sliced = xs[r]; return sliced[0] + sliced[1]; }",
              "50");
}

TEST_CASE("vNext integer literal range checks reject out-of-range values", "[vNext][Numerics][Errors]")
{
  try
  {
    check("fun main() { let small: i8 = 300; return; }");
    FAIL("expected an i8 range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `300` is out of range for type i8");
  }

  try
  {
    check("fun main() { let byte: u8 = -1; return; }");
    FAIL("expected a u8 range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `-1` is out of range for type u8");
  }

  try
  {
    check("fun main() { let a: i32 = 1; let b: u32 = 2; let c = a + b; return; }");
    FAIL("expected a mixed-width arithmetic error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "binary operands type mismatch: expected i32, got u32");
  }
}

TEST_CASE("vNext runtime arithmetic checks per-width overflow", "[vNext][Numerics][Runtime]")
{
  // In-range narrow arithmetic executes.
  expectValue("fun main() -> i64 { let a: i8 = 100; let b: i8 = a + 20; if (b == 120) { return 1; } return 0; }", "1");
  // Overflowing narrow arithmetic fails at runtime with a typed diagnostic.
  std::string output;
  std::string errors;
  REQUIRE(run("fun main() -> i64 { let a: i8 = 100; let b: i8 = a + a; if (b == 0) { return 1; } return 0; }", output,
              errors) == 1);
  REQUIRE(errors.find("integer overflow for type `i8`") != std::string::npos);
  REQUIRE(run("fun main() -> i32 { let a: i32 = 2000000000; let b: i32 = a * 2; return b; }", output, errors) == 1);
  REQUIRE(errors.find("integer overflow for type `i32`") != std::string::npos);
  REQUIRE(run("fun main() -> i64 { let a: u8 = 200; let b: u8 = a + a; if (b == 0) { return 1; } return 0; }", output,
              errors) == 1);
  REQUIRE(errors.find("integer overflow for type `u8`") != std::string::npos);
}

TEST_CASE("vNext per-width overflow checks cover every narrow width", "[vNext][Numerics][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("fun main() -> i64 { let a: i16 = 30000; let b: i16 = a + a; if (b == 0) { return 1; } return 0; }",
              output, errors) == 1);
  REQUIRE(errors.find("integer overflow for type `i16`") != std::string::npos);
  REQUIRE(run("fun main() -> i64 { let a: u16 = 60000; let b: u16 = a + a; if (b == 0) { return 1; } return 0; }",
              output, errors) == 1);
  REQUIRE(errors.find("integer overflow for type `u16`") != std::string::npos);
  REQUIRE(run("fun main() -> i64 { let a: u32 = 4000000000; let b: u32 = a + a; if (b == 0) { return 1; } return 0; }",
              output, errors) == 1);
  REQUIRE(errors.find("integer overflow for type `u32`") != std::string::npos);
  REQUIRE(run("fun main() -> i64 { let a: i8 = -100; let b: i8 = a - 30; if (b == 0) { return 1; } return 0; }", output,
              errors) == 1);
  REQUIRE(errors.find("integer overflow for type `i8`") != std::string::npos);
  REQUIRE(run("fun main() -> i64 { let a: i64 = -9223372036854775808; let b: i64 = a - 1; return b; }", output,
              errors) == 1);
  REQUIRE(errors.find("integer subtraction overflow") != std::string::npos);
}

TEST_CASE("vNext unary sign and narrow division execute on locals", "[vNext][Numerics][Runtime]")
{
  expectValue("fun main() -> i64 { let x: i8 = 100; let negated: i8 = -x; let kept: i8 = +x; "
              "let halved: i8 = x / 2; if (negated == -100 && kept == 100 && halved == 50) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext f64 subtraction division and comparisons execute end to end", "[vNext][Numerics][Runtime]")
{
  expectValue("fun main() -> i64 { let a: f64 = 5.5; let b: f64 = a - 1.5; if (b == 4.0 && b <= 4.0 && b >= 4.0 && "
              "b / 2.0 == 2.0) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext unary plus on literals and constant logical operands fold correctly", "[vNext][Numerics][Runtime]")
{
  expectValue("fun main() -> i64 { return +5; }", "5");
  expectValue("fun main() -> i64 { let c = true && false; if (c) { return 1; } return 0; }", "0");
  expectValue("fun main() -> i64 { let c = true || false; if (c) { return 1; } return 0; }", "1");
}

TEST_CASE("vNext i64::min is expressible as a literal", "[vNext][Numerics][Literals]")
{
  expectValue("fun main() -> i64 { return -9223372036854775808; }", "-9223372036854775808");
  expectValue(
      "fun main() -> i64 { let x = -9223372036854775808; if (x == -9223372036854775807 - 1) { return 1; } return 0; }",
      "1");
  expectValue("fun main() -> i64 { let x: i64 = -9223372036854775808; if (x + 1 == -9223372036854775807) { return 1; } "
              "return 0; }",
              "1");
}

TEST_CASE("vNext u64 literals reach the full unsigned range", "[vNext][Numerics][Literals]")
{
  expectValue("fun main() -> u64 { return 18446744073709551615u64; }", "18446744073709551615");
  expectValue("fun main() -> i64 { let x: u64 = 18446744073709551615u64; "
              "if (x == 18446744073709551614u64 + 1u64) { return 1; } return 0; }",
              "1");
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; fun main() { print(18446744073709551615u64); }", output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("18446744073709551615\n") != std::string::npos);
}

TEST_CASE("vNext out-of-range literals report clean type diagnostics", "[vNext][Numerics][Errors]")
{
  try
  {
    check("fun main() { let x: i64 = 9223372036854775808; }");
    FAIL("expected an i64 range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `9223372036854775808` is out of range for type i64");
  }

  try
  {
    check("fun main() { let x: i64 = -9223372036854775809; }");
    FAIL("expected an i64 range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `-9223372036854775809` is out of range for type i64");
  }

  try
  {
    check("fun main() { let x: u64 = 18446744073709551616u64; }");
    FAIL("expected a u64 range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `18446744073709551616` is out of range for type u64");
  }

  try
  {
    check("fun main() { let x = 99999999999999999999; }");
    FAIL("expected an untyped literal range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `99999999999999999999` is out of range for type i64");
  }

  try
  {
    check("fun main() { let x = -9223372036854775809; }");
    FAIL("expected an untyped literal range error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "integer literal `-9223372036854775809` is out of range");
  }
}

TEST_CASE("vNext numeric example file runs end to end through ngi", "[vNext][Numerics][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/numeric_types.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1312") != std::string::npos);
}
