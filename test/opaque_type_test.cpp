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

TEST_CASE("vNext opaque and abstract types parse and resolve", "[vNext][Opaque][Runtime]")
{
  expectValue("type NativeHandle = native; type AbstractHandle; "
              "fun main() -> i64 { let mut total = 0; "
              "const if (!is_abstract<NativeHandle>) { total := total + 1; } "
              "const if (is_abstract<AbstractHandle>) { total := total + 2; } "
              "return total; }",
              "3");
}

TEST_CASE("vNext is_trait recognizes traits and concrete types", "[vNext][Opaque][Runtime]")
{
  expectValue("trait Show { fun show(self: Self ref) -> string; } struct Widget { value: i64, } "
              "fun main() -> i64 { let mut total = 0; "
              "const if (is_trait<Show>) { total := total + 1; } "
              "const if (!is_trait<Widget>) { total := total + 2; } "
              "return total; }",
              "3");
}

TEST_CASE("vNext opaque declarations reject malformed forms", "[vNext][Opaque][Errors]")
{
  try
  {
    static_cast<void>(syntax::parseSourceUnit("type NativeHandle = i64; fun main() -> i64 { return 0; }"));
    FAIL("expected an opaque native declaration error");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `native` after `=` in opaque type declaration");
  }

  try
  {
    static_cast<void>(hir::Resolver{}.resolve(syntax::parseSourceUnit(
        "type NativeHandle = native; type NativeHandle; fun main() -> i64 { return 0; }")));
    FAIL("expected a duplicate opaque declaration error");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate module declaration `NativeHandle`");
  }

  try
  {
    check("type NativeHandle = native; struct NativeHandle { value: i64, } "
          "fun main() -> i64 { return 0; }");
    FAIL("expected a duplicate opaque/struct name error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate type declaration `NativeHandle`");
  }

  try
  {
    check("fun main() { let mut total = 0; const if (is_abstract<i64, string>) { total := 1; } return; }");
    FAIL("expected an is_abstract arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "is_abstract<T> expects exactly 1 type argument");
  }
}

TEST_CASE("vNext opaque type example file runs end to end through ngi", "[vNext][Opaque][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/opaque_types.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 31") != std::string::npos);
}
