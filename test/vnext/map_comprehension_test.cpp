// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/driver.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"

#include <filesystem>
#include <sstream>

namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;

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
    const int status = NG::vnext::runDriver({"--source", source}, outputStream, errorStream);
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
    const int status = NG::vnext::runDriver({path}, outputStream, errorStream);
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

TEST_CASE("vNext array map comprehensions apply functions elementwise", "[vNext][Map][Runtime]")
{
  expectValue("fun inc(value: i64) -> i64 { return value + 1; } "
              "fun main() -> i64 { let xs = [1, 2, 3]; let ys = [inc(xs)...]; return ys[0] + ys[1] + ys[2]; }",
              "9");
}

TEST_CASE("vNext map comprehensions support expression-bodied functions", "[vNext][Map][Runtime]")
{
  expectValue("fun square(value: i64) -> i64 => value * value; "
              "fun main() -> i64 { let xs = [2, 3]; let ys = [square(xs)...]; return ys[0] + ys[1]; }",
              "13");
}

TEST_CASE("vNext map comprehensions run empty sources correctly", "[vNext][Map][Runtime]")
{
  expectValue("fun inc(value: i64) -> i64 { return value + 1; } "
              "fun main() -> i64 { let xs: array<i64> = []; let ys = [inc(xs)...]; return 0; }",
              "0");
}

TEST_CASE("vNext map comprehension validation reports malformed spreads", "[vNext][Map][Errors]")
{
  try
  {
    check("fun inc(value: i64) -> i64 { return value + 1; } "
          "fun main() { let xs = [1]; let ys = [inc(1)...]; return; }");
    FAIL("expected a non-array map source error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "map spread source must be an array or range, got i64");
  }

  try
  {
    check("fun add(a: i64, b: i64) -> i64 { return a + b; } "
          "fun main() { let xs = [1]; let ys = [add(xs, 2)...]; return; }");
    FAIL("expected a map arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "map spread function must take exactly one argument");
  }
}

TEST_CASE("vNext map comprehensions filter and iterate ranges", "[vNext][Map][Runtime]")
{
  expectValue("fun inc(value: i64) -> i64 { return value + 1; } "
              "fun even(value: i64) -> bool { return (value % 2) == 0; } "
              "fun main() -> i64 { let xs = [1, 2, 3, 4]; let evens = [even(xs)?...]; "
              "let mapped = [inc(0..3)...]; return evens[0] + evens[1] + mapped[0] + mapped[2]; }",
              "10");
}

TEST_CASE("vNext mixed comprehensions interleave literal elements with spreads", "[vNext][Map][Runtime]")
{
  expectValue("fun inc(value: i64) -> i64 { return value + 1; } "
              "fun main() -> i64 { let xs = [1, 2, 3]; let mixed = [0, inc(xs)..., 9]; "
              "let mut total = 0; "
              "if (mixed[0] == 0 && mixed[4] == 9 && mixed[1] == 2 && mixed[3] == 4) { total := total + 5; } "
              "return total; }",
              "5");
}

TEST_CASE("vNext mixed comprehensions support filtered spreads and one-sided forms", "[vNext][Map][Runtime]")
{
  expectValue("fun even(value: i64) -> bool { return (value % 2) == 0; } "
              "fun inc(value: i64) -> i64 { return value + 1; } "
              "fun main() -> i64 { let xs = [1, 2, 3, 4]; "
              "let filtered = [7, even(xs)?..., 8]; "
              "let lead = [9, inc(xs)...]; let trail = [inc(xs)..., 6]; "
              "return filtered[1] + filtered[2] + lead[0] + lead[4] + trail[0] + trail[4]; }",
              "28");
}

TEST_CASE("vNext map comprehension example file runs end to end through ngi", "[vNext][Map][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/map_comprehension.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 15") != std::string::npos);
}
