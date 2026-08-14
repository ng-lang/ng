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

TEST_CASE("vNext generic struct instances coexist with distinct field types", "[vNext][GenericStruct][Runtime]")
{
  expectValue("struct Box<T> { value: T, } "
              "fun main() -> i64 { "
              "let ints: Box<i64> = Box { value: 42 }; "
              "let texts: Box<string> = Box { value: \"boxed\" }; "
              "let mut total = ints.value; "
              "if (texts.value == \"boxed\") { total := total + 1; } "
              "return total; }",
              "43");
}

TEST_CASE("vNext generic structs support multiple parameters and nesting", "[vNext][GenericStruct][Runtime]")
{
  expectValue("struct Pair<A, B> { first: A, second: B, } "
              "struct Box<T> { value: T, } "
              "fun main() -> i64 { "
              "let nested: Box<Pair<i64, string>> = Box { value: Pair { first: 3, second: \"nested\" } }; "
              "let mut total = nested.value.first; "
              "if (nested.value.second == \"nested\") { total := total + 1; } "
              "return total; }",
              "4");
}

TEST_CASE("vNext generic struct instances flow through function signatures", "[vNext][GenericStruct][Runtime]")
{
  expectValue("struct Box<T> { value: T, } "
              "fun wrap(value: Box<i64>) -> i64 { return value.value; } "
              "fun main() -> i64 { let box: Box<i64> = Box { value: 9 }; return wrap(box); }",
              "9");
}

TEST_CASE("vNext generic struct validation reports malformed applications", "[vNext][GenericStruct][Errors]")
{
  try
  {
    check("struct Box<T> { value: T, } fun main() -> i64 { let bad: Box = Box { value: 1 }; return 0; }");
    FAIL("expected a missing generic argument error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "struct type `Box` expects 1 arguments, got 0");
  }

  try
  {
    check("struct Box<T> { value: T, } fun main() -> i64 { let bad: Box<i64, i64> = Box { value: 1 }; return 0; }");
    FAIL("expected a generic argument count error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "struct type `Box` expects 1 arguments, got 2");
  }

  try
  {
    check("struct Box<T> { value: T, } fun main() -> i64 { let bad = Box { value: 1 }; return 0; }");
    FAIL("expected an uninferred struct literal error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} ==
            "cannot infer generic arguments for struct literal `Box`; annotate the binding type");
  }
}

TEST_CASE("vNext generic struct example file runs end to end through ngi", "[vNext][GenericStruct][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/generic_structs.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 15") != std::string::npos);
}
