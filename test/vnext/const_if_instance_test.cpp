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

TEST_CASE("vNext const if selects branches per const generic instance", "[vNext][ConstIf][Runtime]")
{
  expectValue("fun count<const N: i64>(values: array<i64, N>) -> i64 { "
              "let mut total = 0; const if (N >= 3) { total := 3; } else { total := 1; } return total; } "
              "fun main() -> i64 { let xs: array<i64, 2> = [1, 2]; let ys: array<i64, 4> = [1, 2, 3, 4]; "
              "return count(xs) + count(ys); }",
              "4");
}

TEST_CASE("vNext per-instance const if evaluates const fun calls over const parameters", "[vNext][ConstIf][Runtime]")
{
  expectValue("const fun even(value: i64) -> bool { return (value % 2) == 0; } "
              "fun classify<const N: i64>(values: array<i64, N>) -> i64 { "
              "let mut total = 0; const if (even(N)) { total := 2; } else { total := 3; } return total; } "
              "fun main() -> i64 { let xs: array<i64, 4> = [1, 2, 3, 4]; let ys: array<i64, 3> = [1, 2, 3]; "
              "return classify(xs) + classify(ys); }",
              "5");
}

TEST_CASE("vNext concrete const if conditions still fold inside const generic functions", "[vNext][ConstIf][Runtime]")
{
  expectValue("fun plain<const N: i64>(values: array<i64, N>) -> i64 { "
              "let mut total = 0; const if (1 + 1 == 2) { total := 5; } else { total := 0; } return total; } "
              "fun main() -> i64 { let xs: array<i64, 1> = [1]; return plain(xs); }",
              "5");
}

TEST_CASE("vNext per-instance const if reports malformed conditions at instantiation", "[vNext][ConstIf][Errors]")
{
  try
  {
    check("fun bad<const N: i64>(values: array<i64, N>) -> i64 { "
          "const if (N) { return 1; } else { return 0; } } "
          "fun main() -> i64 { let xs: array<i64, 2> = [1, 2]; return bad(xs); }");
    FAIL("expected a non-bool const if condition error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const value of kind `3` is not a bool");
  }
}

TEST_CASE("vNext const if instance example file runs end to end through ngi", "[vNext][ConstIf][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/const_if_instances.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 13") != std::string::npos);
}
