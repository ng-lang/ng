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

TEST_CASE("vNext type constructor parameters instantiate explicitly", "[vNext][Hkt][Runtime]")
{
  expectValue("struct Box<T> { value: T, } "
              "fun accept<F<_>, T>(value: F<T> ref) -> i64 { return 7; } "
              "fun main() -> i64 { let box: Box<i64> = Box { value: 42 }; return accept<i64, Box>(ref box); }",
              "7");
}

TEST_CASE("vNext type constructor parameters instantiate by inference", "[vNext][Hkt][Runtime]")
{
  expectValue("struct Box<T> { value: T, } "
              "fun accept<F<_>, T>(value: F<T> ref) -> i64 { return 3; } "
              "fun main() -> i64 { "
              "let ints: Box<i64> = Box { value: 42 }; "
              "let texts: Box<string> = Box { value: \"boxed\" }; "
              "return accept(ref ints) + accept(ref texts); }",
              "6");
}

TEST_CASE("vNext type constructor applications specialize return types", "[vNext][Hkt][Runtime]")
{
  expectValue("struct Box<T> { value: T, } "
              "fun unwrap<F<_>, T>(value: F<T>) -> F<T> { return value; } "
              "fun main() -> i64 { let box: Box<i64> = Box { value: 42 }; let back: Box<i64> = unwrap(box); "
              "return back.value; }",
              "42");
}

TEST_CASE("vNext type constructor validation reports malformed applications", "[vNext][Hkt][Errors]")
{
  try
  {
    check("struct Box<T> { value: T, } fun accept<F<_>, T>(value: F<i64, i64> ref) -> i64 { return 0; } "
          "fun main() -> i64 { let box: Box<i64> = Box { value: 42 }; return accept<i64, Box>(ref box); }");
    FAIL("expected a constructor arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "type constructor `F` expects exactly 1 argument, got 2");
  }

  try
  {
    check("struct Box<T> { value: T, } fun accept<F<_>, T>(value: F<T> ref) -> i64 { return 0; } "
          "fun main() -> i64 { let box: Box<i64> = Box { value: 42 }; return accept<i64, i64>(ref box); }");
    FAIL("expected a non-struct constructor error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "generic argument 2 must be a struct type constructor");
  }

  try
  {
    check("struct Box<T> { value: T, } fun accept<F<_>, T>(value: F<T> ref) -> i64 { return 0; } "
          "fun main() -> i64 { let box: Box<i64> = Box { value: 42 }; return accept<Box, i64>(ref box); }");
    FAIL("expected a constructor argument position error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "struct type `Box` expects 1 arguments, got 0");
  }
}

TEST_CASE("vNext higher-kinded example file runs end to end through ngi", "[vNext][Hkt][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/hkt.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 5") != std::string::npos);
}
