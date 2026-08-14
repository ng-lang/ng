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
} // namespace

TEST_CASE("vNext module parser builds where clauses on function declarations", "[vNext][Where][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(
      "const is_box<T>: bool = false; "
      "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; } "
      "fun exact<T>(value: T) -> i64 where T is i64 => 1;");
  REQUIRE(unit.items.size() == 3);
  const auto &describe = *static_cast<const syntax::FunctionDeclaration *>(unit.items[1].get());
  REQUIRE(describe.whereClause != nullptr);
  REQUIRE(describe.whereClause->kind == syntax::ExpressionKind::GenericApplication);
  const auto &exact = *static_cast<const syntax::FunctionDeclaration *>(unit.items[2].get());
  REQUIRE(exact.whereClause != nullptr);
  REQUIRE(exact.whereClause->kind == syntax::ExpressionKind::TypeTest);
}

TEST_CASE("vNext resolver lowers where clause constraints into the HIR function", "[vNext][Where][Hir]")
{
  const auto module = resolve("const is_box<T>: bool = false; "
                              "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; }");
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions.front().whereClause != nullptr);
  REQUIRE(module.functions.front().whereClause->kind == hir::ExpressionKind::GenericApplication);
  REQUIRE(module.functions.front().whereClause->text == "is_box");
}

TEST_CASE("vNext where clauses accept and reject calls by predicate satisfaction", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const is_box<T>: bool = false; struct Box { value: i64 } const<T> is_box<Box>: bool = true; "
              "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; } "
              "fun main() -> i64 { let box = Box { value: 1 }; return describe(box); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);

  try
  {
    check("const is_box<T>: bool = false; struct Box { value: i64 } const<T> is_box<Box>: bool = true; "
          "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; } "
          "fun main() -> i64 { let plain = 7; return describe(plain); }");
    FAIL("expected an unsatisfied where clause error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call to `describe` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses support negation and direct type patterns", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const is_box<T>: bool = false; "
              "fun describe_other<T>(value: T) -> i64 where !is_box<T> { return 2; } "
              "fun main() -> i64 { let plain = 7; return describe_other(plain); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 2") != std::string::npos);

  REQUIRE(run("fun exact<T>(value: T) -> i64 where T is i64 { return 16; } "
              "fun main() -> i64 { let plain = 7; return exact(plain); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 16") != std::string::npos);

  try
  {
    check("fun exact<T>(value: T) -> i64 where T is i64 { return 1; } "
          "fun main() -> i64 { let name = \"hi\"; return exact(name); }");
    FAIL("expected an unsatisfied type pattern error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call to `exact` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses call const fun over const parameters", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun is_large(value: i64) -> bool => value > 10; "
              "fun require_large<const N: i64>() -> i64 where is_large(N) { return 8; } "
              "fun main() -> i64 { return require_large<42>(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 8") != std::string::npos);

  try
  {
    check("const fun is_large(value: i64) -> bool => value > 10; "
          "fun require_large<const N: i64>() -> i64 where is_large(N) { return 8; } "
          "fun main() -> i64 { return require_large<3>(); }");
    FAIL("expected an unsatisfied const fun where clause");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call to `require_large` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses are checked per instance and at module level", "[vNext][Where][Typecheck]")
{
  try
  {
    check("fun exact<T>(value: T) -> i64 where T is i64 { return 1; } "
          "fun wrap<T>(x: T) -> i64 { return exact(x); } fun main() -> i64 { return wrap(1); }");
    FAIL("expected an abstract where clause evaluation error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot evaluate where clause of generic function with abstract type parameter `T`");
  }

  try
  {
    check("const is_box<T>: bool = false; "
          "fun unused(x: i64) -> i64 where is_box<i64> { return 1; } "
          "fun main() -> i64 { return 0; }");
    FAIL("expected a module-level where clause error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "function `unused` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses example file runs end to end through ngi", "[vNext][Where][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/where_clauses.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 31") != std::string::npos);
}
