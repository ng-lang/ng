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

namespace
{
  constexpr std::string_view ShowFixture =
      "struct Counter { label: string } "
      "struct Number { value: i64 } "
      "trait Show { fun show(self: Self ref) -> string; } "
      "impl Show for Counter { fun show(self: Self ref) -> string { return (*self).label; } } "
      "impl Show for Number { fun show(self: Self ref) -> string { return \"number\"; } } ";
} // namespace

TEST_CASE("vNext trait views coerce values and dispatch dynamically", "[vNext][TraitObjects][Runtime]")
{
  expectValue(std::string{ShowFixture} + "fun main() -> i64 { "
              "let counter = Counter { label: \"seven\" }; let view: ref<Show> = counter; "
              "if (view.show() == \"seven\") { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext trait views flow through parameters and dispatch per concrete type", "[vNext][TraitObjects][Runtime]")
{
  expectValue(std::string{ShowFixture} + "fun render(item: ref<Show>) -> string { return item.show(); } "
              "fun main() -> i64 { let counter = Counter { label: \"a\" }; let number = Number { value: 2 }; "
              "let mut total = 0; if (render(counter) == \"a\") { total := total + 1; } "
              "if (render(number) == \"number\") { total := total + 2; } return total; }",
              "3");
}

TEST_CASE("vNext arrays of trait views dispatch elementwise", "[vNext][TraitObjects][Runtime]")
{
  expectValue(std::string{ShowFixture} + "fun main() -> i64 { "
              "let counter = Counter { label: \"x\" }; let number = Number { value: 1 }; "
              "let views: array<ref<Show>> = [counter, number]; "
              "if (views[0].show() == \"x\" && views[1].show() == \"number\") { return 5; } return 0; }",
              "5");
}

TEST_CASE("vNext trait view validation rejects non-implementors and bad methods", "[vNext][TraitObjects][Errors]")
{
  try
  {
    check(std::string{ShowFixture} + "struct Plain { value: i64 } "
          "fun main() -> i64 { let plain = Plain { value: 1 }; let view: ref<Show> = plain; return 0; }");
    FAIL("expected a missing impl error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "value of type Plain does not implement trait `Show`");
  }

  try
  {
    check(std::string{ShowFixture} + "fun main() -> i64 { let counter = Counter { label: \"x\" }; "
          "let view: ref<Show> = counter; return view.missing(); }");
    FAIL("expected an unknown method error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "trait `Show` has no method `missing`");
  }

  try
  {
    check("trait Show { fun show(self: Self ref) -> string; } "
          "fun main() { let bad: Show = 1; return; }");
    FAIL("expected a bare trait value error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "trait `Show` is not a value type; use `ref<Show>`");
  }
}

TEST_CASE("vNext trait object example file runs end to end through ngi", "[vNext][TraitObjects][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/trait_objects.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 31") != std::string::npos);
}
