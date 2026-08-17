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

TEST_CASE("vNext const predicates introspect tuple and pack shapes", "[vNext][TupleIntrospection][Runtime]")
{
  expectValue("fun main() -> i64 { let mut total = 0; "
              "const if (is_tuple<tuple<i64, string, bool>>) { total := total + 1; } "
              "const if (is_tuple<i64>) { total := total + 2; } "
              "const if (tuple_size<tuple<i64, string, bool>> == 3) { total := total + 4; } "
              "const if (sizeof_pack<i64, string, bool> == 3) { total := total + 8; } "
              "return total; }",
              "13");
}

TEST_CASE("vNext tuple_element resolves tuple element types in annotations", "[vNext][TupleIntrospection][Runtime]")
{
  expectValue("fun main() -> i64 { "
              "let first: tuple_element<tuple<i64, string, bool>, 0> = 7; "
              "let second: tuple_element<tuple<i64, string, bool>, 1> = \"tuple\"; "
              "let third: tuple_element<tuple<i64, string, bool>, 2> = true; "
              "let mut result = first; if (third && second == \"tuple\") { result := result + 1; } "
              "return result; }",
              "8");
}

TEST_CASE("vNext tuple_concat concatenates tuple types", "[vNext][TupleIntrospection][Runtime]")
{
  expectValue("fun main() -> i64 { "
              "let joined: tuple_concat<tuple<i64, string>, tuple<bool, i64>> = (1, \"joined\", true, 9); "
              "return joined.3; }",
              "9");
}

TEST_CASE("vNext tuple introspection validation reports malformed uses", "[vNext][TupleIntrospection][Errors]")
{
  try
  {
    check("fun main() { let mut total = 0; const if (tuple_size<i64> == 3) { total := 1; } return; }");
    FAIL("expected a non-tuple tuple_size error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "tuple_size<T> expects a tuple type, got i64");
  }

  try
  {
    check("fun main() { let first: tuple_element<tuple<i64, string>, 2> = 7; return; }");
    FAIL("expected a tuple_element bounds error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "tuple_element index out of range: index 2, length 2");
  }

  try
  {
    check("fun main() { let joined: tuple_concat<i64, tuple<bool>> = (1, true); return; }");
    FAIL("expected a tuple_concat non-tuple error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "tuple_concat<A, B> expects tuple types");
  }
}

TEST_CASE("vNext enhanced tuple example file runs end to end through ngi", "[vNext][TupleIntrospection][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/enhanced_tuples.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 255") != std::string::npos);
}
