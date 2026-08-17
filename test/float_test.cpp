// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
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
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
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
    REQUIRE(output.find(std::string{"native main exited with code "} + std::string{value}) != std::string::npos);
  }
} // namespace

TEST_CASE("vNext f32 and f64 run same-type arithmetic", "[vNext][Floats][Runtime]")
{
  expectValue("fun area(radius: f64) -> f64 { return radius * radius * 3.25; } "
              "fun main() -> i64 { let circle = area(2.0); "
              "if (circle > 12.9 && circle < 13.1) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext float literals adopt contextual types with suffixes", "[vNext][Floats][Runtime]")
{
  expectValue("fun main() -> i64 { let single: f32 = 1.5f32; let half: f32 = single / 2.0; "
              "if (half == 0.75) { return 7; } return 0; }",
              "7");
}

TEST_CASE("vNext numeric equality and ordering compare across widths", "[vNext][Floats][Runtime]")
{
  expectValue("fun main() -> i64 { let mut total = 0; "
              "if (1.5f32 == 1.5f64) { total := total + 1; } "
              "if (1i16 == 1u32) { total := total + 2; } "
              "if (1 < 1.5) { total := total + 4; } "
              "if (2.5f64 > 2i8) { total := total + 8; } "
              "return total; }",
              "15");
}

TEST_CASE("vNext float validation rejects bad suffixes and conflicts", "[vNext][Floats][Errors]")
{
  try
  {
    static_cast<void>(syntax::parseSourceUnit("fun main() -> i64 { let bad = 1u128; return 0; }"));
    FAIL("expected an unknown suffix error");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "unknown numeric literal suffix `u128`");
  }

  try
  {
    static_cast<void>(syntax::parseSourceUnit("fun main() -> i64 { let bad = 1.5u8; return 0; }"));
    FAIL("expected a float/integer suffix error");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "float literal cannot carry integer suffix `u8`");
  }

  try
  {
    check("fun main() -> i64 { let bad: f64 = 1.5f32; return 0; }");
    FAIL("expected a float suffix conflict error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "float literal suffix `f32` conflicts with expected type f64");
  }

  try
  {
    check("fun main() -> i64 { let a: f32 = 1.5; let b: f64 = 2.5; let c = a + b; return 0; }");
    FAIL("expected a mixed-width float arithmetic error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "binary operands type mismatch: expected f32, got f64");
  }
}

TEST_CASE("vNext float example file runs end to end through ngi", "[vNext][Floats][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/floats.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 63") != std::string::npos);
}
