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

TEST_CASE("vNext variadic type constructors accept opaque templates", "[vNext][VariadicHkt][Runtime]")
{
  expectValue("type Variadic<Head, Tail...> = native; "
              "fun accept_variadic_constructor<F<_, ...>>() -> i64 { return 3; } "
              "fun main() -> i64 { return accept_variadic_constructor<Variadic>(); }",
              "3");
}

TEST_CASE("vNext parameterized opaque templates instantiate per argument list", "[vNext][VariadicHkt][Runtime]")
{
  expectValue("type Variadic<Head, Tail...> = native; "
              "fun main() -> i64 { let mut total = 0; "
              "const if (!is_abstract<Variadic<i32, string, bool>>) { total := total + 1; } "
              "const if (!is_abstract<Variadic<i32>>) { total := total + 2; } "
              "return total; }",
              "3");
}

TEST_CASE("vNext variadic constructor applications typecheck in signatures", "[vNext][VariadicHkt][Typecheck]")
{
  REQUIRE_NOTHROW(check("type Variadic<Head, Tail...> = native; "
                        "fun describe<F<_, ...>, A, B>(first: F<A, B> ref) -> i64 { return 0; } "
                        "fun touch(handle: Variadic<i32, string> ref) -> i64 { return 1; }"));
}

TEST_CASE("vNext variadic HKT validation rejects malformed applications", "[vNext][VariadicHkt][Errors]")
{
  try
  {
    static_cast<void>(syntax::parseSourceUnit("type Variadic<Head, Tail...> = native; "
                                              "fun empty<F<_, ...>>(first: F<> ref) -> i64 { return 0; }"));
    FAIL("expected an empty variadic application error");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected a type name");
  }

  try
  {
    check("type Variadic<Head, Tail...> = native; fun main() { let bad: Variadic = 1; return; }");
    FAIL("expected a bare parameterized opaque error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "opaque type `Variadic` expects type arguments");
  }
}

TEST_CASE("vNext variadic HKT example file runs end to end through ngi", "[vNext][VariadicHkt][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/variadic_hkt.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 9") != std::string::npos);
}
