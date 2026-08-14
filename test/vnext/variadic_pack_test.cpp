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

TEST_CASE("vNext module parser accepts variadic type parameters", "[vNext][Pack][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(
      "fun gather<T...>(args: T...) -> tuple<T...> { return (...args,); }");
  REQUIRE(unit.items.size() == 1);
  const auto &function = *static_cast<const syntax::FunctionDeclaration *>(unit.items[0].get());
  REQUIRE(function.genericParameters.size() == 1);
  REQUIRE(function.genericParameters[0].kind == syntax::GenericParameterKind::Pack);
  REQUIRE(function.parameters.size() == 1);
  REQUIRE(function.parameters[0].type->kind == syntax::TypeSyntaxKind::Pack);
  REQUIRE(function.returnType->kind == syntax::TypeSyntaxKind::Applied);
}

TEST_CASE("vNext resolver records pack parameters on functions", "[vNext][Pack][Hir]")
{
  const auto module = resolve("fun gather<T...>(args: T...) -> tuple<T...> { return (...args,); }");
  REQUIRE(module.functions.front().packParameters == std::vector<std::string>{"T"});
  REQUIRE(module.functions.front().parameters.front().type.kind == hir::TypeKind::Pack);
}

TEST_CASE("vNext variadic functions gather heterogeneous argument packs", "[vNext][Pack][Runtime]")
{
  expectValue("fun gather<T...>(args: T...) -> tuple<T...> { return (...args,); } "
              "fun main() -> i64 { let packed: tuple<i64, string, bool> = gather(1, \"pack\", false); "
              "if (packed.1 == \"pack\") { return 7; } return 0; }",
              "7");
}

TEST_CASE("vNext tuple literal spreads append to fixed elements", "[vNext][Pack][Runtime]")
{
  expectValue("fun append_value<T...>(args: T...) -> tuple<T..., i64> { return (...args, 42); } "
              "fun main() -> i64 { let packed: tuple<i64, string, i64> = append_value(1, \"two\"); return packed.2; }",
              "42");
}

TEST_CASE("vNext variadic functions instantiate per concrete pack", "[vNext][Pack][Runtime]")
{
  expectValue("fun identity_pair<T...>(args: T...) -> tuple<T...> { return (...args,); } "
              "fun main() -> i64 { let a: tuple<i64, bool> = identity_pair(5, true); "
              "let b: tuple<string> = identity_pair(\"solo\"); return a.0; }",
              "5");

  expectValue("fun first<T...>(args: T...) -> tuple<T...> { return (...args,); } "
              "fun main() -> i64 { let x = first(9); return x.0; }",
              "9");
}

TEST_CASE("vNext call-site tuple spreads flatten into argument lists", "[vNext][Pack][Runtime]")
{
  expectValue("fun pick_two(a: i64, b: i64) -> i64 { return a + b; } "
              "fun main() -> i64 { let p: tuple<i64, i64> = (3, 4); return pick_two(...p); }",
              "7");

  expectValue("fun pick_middle(left: i64, value: string, ok: bool) -> string { return value; } "
              "fun main() -> i64 { let r = pick_middle(...(7, \"spread\", true)); "
              "if (r == \"spread\") { return 7; } return 0; }",
              "7");
}

TEST_CASE("vNext call-site spreads reject non-tuple values", "[vNext][Pack][Errors]")
{
  try
  {
    check("fun pick_two(a: i64, b: i64) -> i64 { return a + b; } "
          "fun main() { let value = 3; return pick_two(...value); }");
    FAIL("expected a non-tuple spread error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot spread value of type i64");
  }
}

TEST_CASE("vNext variadic call arity is checked against fixed parameters", "[vNext][Pack][Errors]")
{
  try
  {
    check("fun mix(left: i64, rest: i64...) -> tuple<i64, i64...> { return (left, ...rest); } "
          "fun main() { let t = mix(); return; }");
    FAIL("expected a variadic arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call argument count mismatch: expected 2, got 0");
  }
}

TEST_CASE("vNext variadic packs example file runs end to end through ngi", "[vNext][Pack][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/variadic_packs.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 63") != std::string::npos);
}
