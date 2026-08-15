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

TEST_CASE("vNext module parser accepts native fun declarations", "[vNext][Native][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(
      "native fun print(value: i64) -> unit; "
      "native fun readLine() -> string;");
  REQUIRE(unit.items.size() == 2);
  const auto &print = *static_cast<const syntax::FunctionDeclaration *>(unit.items[0].get());
  REQUIRE(print.nativeFunction);
  REQUIRE(print.body.statements.empty());
  REQUIRE(print.parameters.size() == 1);
  const auto &readLine = *static_cast<const syntax::FunctionDeclaration *>(unit.items[1].get());
  REQUIRE(readLine.nativeFunction);
}

TEST_CASE("vNext resolver marks native functions in HIR", "[vNext][Native][Hir]")
{
  const auto module = resolve("native fun print(value: i64) -> unit;");
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions.front().nativeFunction);
  REQUIRE(module.functions.front().name == "print");
}

TEST_CASE("vNext print and assert builtins execute end to end", "[vNext][Native][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun print(value: i64) -> unit; native fun print(value: string) -> unit; "
              "native fun print(value: bool) -> unit; native fun assert(condition: bool) -> unit; "
              "fun main() { print(42); print(\"hello\"); print(true); assert(1 + 1 == 2); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("42\nhello\ntrue\n") != std::string::npos);
}

TEST_CASE("vNext assert failures surface as runtime errors", "[vNext][Native][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun assert(condition: bool) -> unit; fun main() { assert(false); }", output, errors) == 1);
  REQUIRE(errors == "bytecode error: assertion failed\n");
}

TEST_CASE("vNext unregistered natives are deterministic runtime errors", "[vNext][Native][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun missing(value: i64) -> unit; fun main() { missing(1); }", output, errors) == 1);
  REQUIRE(errors == "bytecode error: native function `missing` is not registered\n");
}

TEST_CASE("vNext native functions typecheck like ordinary declarations", "[vNext][Native][Typecheck]")
{
  REQUIRE_NOTHROW(check("native fun print(value: i64) -> unit; "
                        "fun main() { print(1); }"));
  try
  {
    check("native fun print(value: i64) -> unit; fun main() { print(\"text\"); }");
    FAIL("expected an argument type mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call argument 1 type mismatch: expected i64, got string");
  }
}

TEST_CASE("vNext native io example runs end to end through ngi", "[vNext][Native][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/native_io.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("42\nhello, native world\ntrue\ntrue\n") != std::string::npos);
}
