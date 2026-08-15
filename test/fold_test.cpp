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

TEST_CASE("vNext fold calls accumulate arrays with the accumulator first", "[vNext][Fold][Runtime]")
{
  expectValue("fun add(acc: i64, value: i64) -> i64 { return acc + value; } "
              "fun main() -> i64 { let xs = [1, 2, 3]; return add(0, xs...); }",
              "6");
}

TEST_CASE("vNext fold calls accumulate arrays with the spread first", "[vNext][Fold][Runtime]")
{
  expectValue("fun sub(value: i64, acc: i64) -> i64 { return value - acc; } "
              "fun main() -> i64 { let xs = [1, 2, 3]; return sub(xs..., 0); }",
              "2");
}

TEST_CASE("vNext fold calls iterate ranges and slices", "[vNext][Fold][Runtime]")
{
  expectValue("fun add(acc: i64, value: i64) -> i64 { return acc + value; } "
              "fun main() -> i64 { let r = 1..4; let xs = [10, 20, 30]; let sp = xs[1..3]; "
              "return add(0, r...) + add(0, sp...); }",
              "56");
}

TEST_CASE("vNext fold calls return the seed for empty sources", "[vNext][Fold][Runtime]")
{
  expectValue("fun add(acc: i64, value: i64) -> i64 { return acc + value; } "
              "fun main() -> i64 { let xs: array<i64> = []; return add(5, xs...); }",
              "5");
}

TEST_CASE("vNext fold validation reports malformed fold calls", "[vNext][Fold][Errors]")
{
  try
  {
    check("fun add(a: i64, b: i64) -> i64 { return a + b; } "
          "fun main() { let xs = [1]; let ys = add(xs..., 1, 2); return; }");
    FAIL("expected a fold accumulator arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "fold calls require exactly one accumulator argument");
  }

  try
  {
    check("fun add(a: i64, b: i64, c: i64) -> i64 { return a + b + c; } "
          "fun main() { let xs = [1]; let ys = add(xs..., 2); return; }");
    FAIL("expected a fold arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "fold function must take exactly two arguments");
  }
}

TEST_CASE("vNext fold example file runs end to end through ngi", "[vNext][Fold][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/folds.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 15") != std::string::npos);
}
