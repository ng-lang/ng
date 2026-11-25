// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "module_loader.hpp"
#include "syntax/module_parser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace modules = NG::modules;
namespace syntax = NG::syntax;

namespace
{
  /// Creates a temporary module directory, cleaned up on destruction.
  struct ModuleFixture
  {
    std::filesystem::path directory;

    ModuleFixture()
    {
      directory = std::filesystem::temp_directory_path() /
                  (std::string{"ng_modules_"} + std::to_string(::getpid()) + "_" +
                   std::to_string(reinterpret_cast<uintptr_t>(this)));
      std::filesystem::create_directories(directory);
    }

    ~ModuleFixture()
    {
      std::error_code ignored;
      std::filesystem::remove_all(directory, ignored);
    }

    void write(std::string_view name, std::string_view contents)
    {
      std::ofstream output{directory / std::string{name}};
      output << contents;
    }
  };

  [[nodiscard]] auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver(arguments, outputStream, errorStream);
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

TEST_CASE("vNext module parser builds import directives and export prefixes", "[vNext][Modules][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(
      "import io; import math (sqrt, pow); "
      "export fun hello() -> i64 => 1; "
      "fun hidden() -> i64 => 2;");
  REQUIRE(unit.items.size() == 4);
  const auto &importAll = *static_cast<const syntax::ImportDeclaration *>(unit.items[0].get());
  REQUIRE(importAll.name == "io");
  REQUIRE(importAll.names.empty());
  const auto &selective = *static_cast<const syntax::ImportDeclaration *>(unit.items[1].get());
  REQUIRE(selective.name == "math");
  REQUIRE(selective.names == std::vector<std::string>{"sqrt", "pow"});
  REQUIRE(static_cast<const syntax::FunctionDeclaration *>(unit.items[2].get())->exported);
  REQUIRE_FALSE(static_cast<const syntax::FunctionDeclaration *>(unit.items[3].get())->exported);
}

TEST_CASE("vNext module loader merges transitive import graphs", "[vNext][Modules][Loader]")
{
  ModuleFixture fixture;
  fixture.write("a.ng", "import b; export fun fromA() -> i64 => fromB() + 1;");
  fixture.write("b.ng", "import c; export fun fromB() -> i64 => fromC() + 1;");
  fixture.write("c.ng", "export fun fromC() -> i64 => 40;");
  const auto unit = modules::ModuleLoader{}.loadFile(fixture.directory / "a.ng");
  REQUIRE(unit.items.size() == 3);
  std::string output;
  std::string errors;
  fixture.write("main.ng", "import a; fun main() -> i64 { return fromA(); }");
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 42") != std::string::npos);
}

TEST_CASE("vNext module loader reports missing modules deterministically", "[vNext][Modules][Loader]")
{
  ModuleFixture fixture;
  fixture.write("main.ng", "import nowhere; fun main() -> i64 { return 0; }");
  std::string output;
  std::string errors;
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 1);
  REQUIRE(errors.starts_with("module error: module `nowhere` not found"));
}

TEST_CASE("vNext module loader tolerates import cycles", "[vNext][Modules][Loader]")
{
  ModuleFixture fixture;
  fixture.write("left.ng", "import right; export fun fromLeft() -> i64 => fromRight() + 1;");
  fixture.write("right.ng", "import left; export fun fromRight() -> i64 => 41;");
  fixture.write("main.ng", "import left; fun main() -> i64 { return fromLeft(); }");
  std::string output;
  std::string errors;
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 42") != std::string::npos);
}

TEST_CASE("vNext imported modules may declare structs and traits", "[vNext][Modules][Loader]")
{
  ModuleFixture fixture;
  fixture.write("shapes.ng",
                "export struct Point { x: i64 } "
                "export trait Measure { fun size(self: Self ref) -> i64; } "
                "export impl Measure for Point { fun size(self: Self ref) -> i64 { return (*self).x; } }");
  fixture.write("main.ng", "import shapes; fun main() -> i64 { let point = Point { x: 7 }; return point.size(); }");
  std::string output;
  std::string errors;
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 7") != std::string::npos);
}

TEST_CASE("vNext module privacy hides non-exported functions from importers", "[vNext][Modules][Privacy]")
{
  ModuleFixture fixture;
  fixture.write("a.ng", "export fun visible() -> i64 => 1; fun hidden() -> i64 => 2;");
  fixture.write("main.ng", "import a; fun main() -> i64 { return visible(); }");
  std::string output;
  std::string errors;
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 0);
  REQUIRE(errors.empty());

  fixture.write("main.ng", "import a; fun main() -> i64 { return hidden(); }");
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 1);
  REQUIRE(errors.find("name `hidden` is not visible in this module") != std::string::npos);
}

TEST_CASE("vNext selective imports grant exactly the named functions", "[vNext][Modules][Privacy]")
{
  ModuleFixture fixture;
  fixture.write("a.ng", "export fun first() -> i64 => 1; export fun second() -> i64 => 2;");
  fixture.write("main.ng", "import a (first); fun main() -> i64 { return first(); }");
  std::string output;
  std::string errors;
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 0);
  REQUIRE(errors.empty());

  fixture.write("main.ng", "import a (first); fun main() -> i64 { return second(); }");
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 1);
  REQUIRE(errors.find("name `second` is not visible in this module") != std::string::npos);
}

TEST_CASE("vNext wildcard imports re-export imported module surfaces transitively", "[vNext][Modules][Privacy]")
{
  ModuleFixture fixture;
  fixture.write("leaf.ng", "export fun leafValue() -> i64 => 42; fun leafHidden() -> i64 => 0;");
  fixture.write("middle.ng", "import leaf; export fun middleValue() -> i64 => leafValue();");
  fixture.write("main.ng", "import middle; fun main() -> i64 { return leafValue() + middleValue(); }");
  std::string output;
  std::string errors;
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 84") != std::string::npos);

  fixture.write("main.ng", "import middle; fun main() -> i64 { return leafHidden(); }");
  REQUIRE(run({(fixture.directory / "main.ng").string()}, output, errors) == 1);
  REQUIRE(errors.find("name `leafHidden` is not visible in this module") != std::string::npos);
}

TEST_CASE("vNext module import example runs end to end through ngi", "[vNext][Modules][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/modules/imports_main.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 42") != std::string::npos);
}
