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

TEST_CASE("vNext derived clone deep-copies independently of the source", "[vNext][Derive][Runtime]")
{
  expectValue("struct Point: derive(Copy + Clone) { x: i64, y: i64, } "
              "fun main() -> i64 { "
              "let point = Point { x: 20, y: 22 }; "
              "let mut cloned = point.clone(); "
              "cloned.x := 30; "
              "let mut total = 0; "
              "if (point.x == 20 && cloned.x == 30) { total := total + 1; } "
              "if (point.y == 22 && cloned.y == 22) { total := total + 2; } "
              "return total; }",
              "3");
}

TEST_CASE("vNext derived Copy and Clone satisfy trait bounds and where clauses", "[vNext][Derive][Runtime]")
{
  expectValue("struct Point: derive(Copy + Clone) { x: i64, } "
              "fun require_copy<T: Copy>() -> i64 { return 1; } "
              "fun require_clone<T: Clone>() -> i64 { return 2; } "
              "fun require_copy_where<T>(value: T) -> i64 where T: Copy { return 4; } "
              "fun main() -> i64 { let point = Point { x: 1 }; "
              "return require_copy<Point>() + require_clone<Point>() + require_copy_where(point); }",
              "7");
}

TEST_CASE("vNext auto traits apply to every concrete type", "[vNext][Derive][Runtime]")
{
  expectValue("auto trait Send {} "
              "fun require_send<T: Send>() -> i64 { return 5; } "
              "fun require_send_where<T>(value: T) -> i64 where T: Send { return 6; } "
              "fun main() -> i64 { return require_send<i64>() + require_send<string>() + require_send_where(true); }",
              "16");
}

TEST_CASE("vNext derive validation reports malformed derives", "[vNext][Derive][Errors]")
{
  try
  {
    check("auto trait Send {} struct Point: derive(Send) { x: i64, } fun main() -> i64 { return 0; }");
    FAIL("expected an unsupported derive target error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "derive currently supports Copy and Clone only: Send");
  }

  try
  {
    check("struct Point: derive(Copy + Copy) { x: i64, } fun main() -> i64 { return 0; }");
    FAIL("expected a duplicate derive error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate derive for trait `Copy` on type `Point`");
  }

  try
  {
    check("struct Point: derive(Copy) { x: i64, } impl Copy for Point {} "
          "fun main() -> i64 { return 0; }");
    FAIL("expected a derive/impl conflict error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "derive conflicts with explicit impl for trait `Copy` on type `Point`");
  }

  try
  {
    check("struct Point: derive(Copy) { x: i64, } "
          "fun main() -> i64 { let point = Point { x: 1 }; let bad = point.clone(); return 0; }");
    FAIL("expected a missing clone method error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "no method `clone` for value of type Point");
  }
}

TEST_CASE("vNext derive example file runs end to end through ngi", "[vNext][Derive][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/derive.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 63") != std::string::npos);
}
