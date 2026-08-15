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
    REQUIRE(output.find(std::string{"with value "} + std::string{value}) != std::string::npos);
  }
} // namespace

TEST_CASE("vNext union types accept member values and compare against members", "[vNext][Union][Runtime]")
{
  expectValue("fun main() -> i64 { let x: i64 | string = 1; let y: i64 | string = \"hello\"; "
              "let mut total = 0; if (x == 1) { total := total + 1; } "
              "if (y == \"hello\") { total := total + 2; } return total; }",
              "3");
}

TEST_CASE("vNext union values flow through function parameters", "[vNext][Union][Runtime]")
{
  expectValue("fun read(value: bool | i64 | f64) -> i64 { if (value == 42) { return 7; } return 0; } "
              "fun main() -> i64 { let w: bool | i64 | f64 = 42; return read(w); }",
              "7");
}

TEST_CASE("vNext union validation rejects non-member values", "[vNext][Union][Errors]")
{
  try
  {
    check("fun main() -> i64 { let bad: bool | string = 5; return 0; }");
    FAIL("expected a non-member union error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "value of type i64 is not a member of union bool | string");
  }
}

TEST_CASE("vNext union example file runs end to end through ngi", "[vNext][Union][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/unions.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 15") != std::string::npos);
}
