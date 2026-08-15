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
} // namespace

TEST_CASE("vNext module parser accepts const fun declarations and expression bodies", "[vNext][ConstFun][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(
      "const fun is_large(value: i64) -> bool { return value > 10; } "
      "const fun positive(n: i64) -> bool => n > 0; "
      "fun double(x: i64) -> i64 => x * 2;");
  REQUIRE(unit.items.size() == 3);
  const auto &first = *static_cast<const syntax::FunctionDeclaration *>(unit.items[0].get());
  REQUIRE(first.constFunction);
  const auto &second = *static_cast<const syntax::FunctionDeclaration *>(unit.items[1].get());
  REQUIRE(second.constFunction);
  REQUIRE(second.body.statements.size() == 1);
  REQUIRE(dynamic_cast<const syntax::ReturnStatement *>(second.body.statements[0].get()) != nullptr);
  const auto &third = *static_cast<const syntax::FunctionDeclaration *>(unit.items[2].get());
  REQUIRE_FALSE(third.constFunction);
  REQUIRE(third.body.statements.size() == 1);
}

TEST_CASE("vNext const fun bodies execute at runtime like ordinary functions", "[vNext][ConstFun][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun fact(value: i64) -> i64 { if (value == 0) { return 1; } return value * fact(value - 1); } "
              "fun main() -> i64 { let input = 5; return fact(input); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 120") != std::string::npos);

  REQUIRE(run("fun double(x: i64) -> i64 => x * 2; fun main() -> i64 { return double(21); }", output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 42") != std::string::npos);
}

TEST_CASE("vNext const fun folds recursive calls inside const if conditions", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun fact(value: i64) -> i64 { if (value == 0) { return 1; } return value * fact(value - 1); } "
              "fun main() -> i64 { const if (fact(4) == 24) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);

  REQUIRE(run("const fun fact(value: i64) -> i64 { if (value == 0) { return 1; } return value * fact(value - 1); } "
              "fun main() -> i64 { const if (fact(4) == 25) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 0") != std::string::npos);
}

TEST_CASE("vNext const fun interpreter executes loops and tail recursion", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun sum_to(n: i64) -> i64 { let mut total = 0; loop (i = 0) { total := total + i; "
              "if (i == n) { return total; } next (i + 1); } } "
              "fun main() -> i64 { const if (sum_to(100) == 5050) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);

  REQUIRE(run("const fun countdown(n: i64) -> i64 { if (n == 0) { return 0; } next (n - 1); } "
              "fun main() -> i64 { const if (countdown(1000) == 0) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);
}

TEST_CASE("vNext const fun bodies may use const predicates and const if", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const is_ref<T>: bool = false; const<T> is_ref<ref<T>>: bool = true; "
              "const fun classify() -> i64 { const if (is_ref<ref<i64>>) { return 1; } return 0; } "
              "fun main() -> i64 { return classify(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);
}

TEST_CASE("vNext const evaluation rejects non-const functions, runtime locals, and generic const fun", "[vNext][ConstFun][Errors]")
{
  try
  {
    check("fun plain(n: i64) -> i64 { return n; } fun main() { const if (plain(1) == 1) { return; } }");
    FAIL("expected a non-const call error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "function `plain` is not const-capable");
  }

  try
  {
    check("const fun fact(n: i64) -> i64 { return n; } "
          "fun main() { let input = 3; const if (fact(input) == 3) { return; } }");
    FAIL("expected a runtime local error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "runtime local `input` is not a compile-time constant");
  }

  try
  {
    check("const fun identity<T>(value: T) -> T { return value; } "
          "fun main() { const if (identity(1) == 1) { return; } }");
    FAIL("expected a generic const fun error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "compile-time calls to generic const fun `identity` are not yet supported");
  }
}

TEST_CASE("vNext const evaluation enforces the fuel budget", "[vNext][ConstFun][Errors]")
{
  try
  {
    check("const fun spin() -> i64 { loop (i = 0) { next (i + 1); } } "
          "fun main() { const if (spin() == 0) { return; } }");
    FAIL("expected a fuel budget error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const evaluation exceeded the fuel budget");
  }
}

TEST_CASE("vNext const fun example file runs end to end through ngi", "[vNext][ConstFun][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/const_fun.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 32") != std::string::npos);
}
