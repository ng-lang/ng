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

TEST_CASE("vNext range literals slice arrays with checked bounds", "[vNext][Range][Runtime]")
{
  expectValue("fun main() -> i64 { let values = [10, 20, 30, 40, 50]; "
              "let slice = values[1..4]; return slice[0] + slice[2]; }",
              "60");
}

TEST_CASE("vNext range values are first-class and reusable", "[vNext][Range][Runtime]")
{
  expectValue("fun main() -> i64 { let r = 1..3; let values = [10, 20, 30, 40]; "
              "let a = values[r]; let b = values[r]; return a[0] + b[1]; }",
              "50");
}

TEST_CASE("vNext range types resolve and check", "[vNext][Range][Typecheck]")
{
  const auto module = resolve("fun f() -> range<i64> { return 0..4; }");
  REQUIRE(module.functions.front().returnTypeName == "range<i64>");
  REQUIRE_NOTHROW(check("fun main() { let values = [1, 2]; let slice = values[1..2]; return; }"));
}

TEST_CASE("vNext out-of-bounds slices are runtime errors", "[vNext][Range][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("fun main() { let values = [1, 2]; let slice = values[1..4]; return; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("killed by a signal"));
}

TEST_CASE("vNext range types reject non-integer bounds", "[vNext][Range][Errors]")
{
  try
  {
    check("fun main() { let r = true..3; return; }");
    FAIL("expected a range bound type error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "range start type mismatch: expected i64, got bool");
  }
}

TEST_CASE("vNext ranges and slicing example file runs end to end through ngi", "[vNext][Range][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/ranges_slicing.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 7") != std::string::npos);
}

TEST_CASE("vNext array append keeps value semantics and element types", "[vNext][ArrayAppend]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; fun main() { "
               "let xs = [1, 2, 3]; let longer = xs << 9; "
               "assert(xs[2] == 3); assert(longer[3] == 9); }",
               output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; fun main() { "
               "let mut acc = [1]; acc := acc << 2 << 3; "
               "assert(acc[0] == 1); assert(acc[2] == 3); }",
               output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; fun main() { "
               "let words = [\"a\"]; let more = words << \"b\"; "
               "assert(words[0] == \"a\"); assert(more[1] == \"b\"); }",
               output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; fun main() { let n = 1 << 3; assert(n == 8); }", output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; fun main() { let xs = [1]; let bad = xs << \"no\"; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("appended element"));
}
