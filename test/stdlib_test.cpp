// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "module_loader.hpp"
#include "syntax/module_parser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace modules = NG::modules;
namespace syntax = NG::syntax;

namespace
{
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

TEST_CASE("vNext stdlib modules resolve from the lib/std search path", "[vNext][Stdlib][Loader]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() { print(\"stdlib works\"); }"}, output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("stdlib works\n") != std::string::npos);
}

TEST_CASE("vNext std.string operations execute end to end", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source",
               "import prelude; import string; fun main() { "
               "print(trim(\"  hello  \")); "
               "print(toUpper(\"ng\")); "
               "let parts = split(\"a,b,c\", \",\"); "
               "print(join(parts, \"|\")); "
               "print(contains(\"haystack\", \"stack\")); "
               "print(replace(\"foo bar foo\", \"foo\", \"baz\")); "
               "print(startsWith(\"Hello\", \"He\")); "
               "print(endsWith(\"Hello\", \"llo\")); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("hello\nNG\na|b|c\ntrue\nbaz bar baz\ntrue\ntrue\n") != std::string::npos);
}

TEST_CASE("vNext std.io file operations execute end to end", "[vNext][Stdlib][Runtime]")
{
  const auto directory = std::filesystem::temp_directory_path() / "ng_stdio_test";
  std::filesystem::create_directories(directory);
  const auto path = (directory / "note.txt").string();
  std::string output;
  std::string errors;
  REQUIRE(run({"--source",
               "import prelude; import io; fun main() { writeFile(\"" + path + "\", \"file contents\"); "
               "let text = readFile(\"" + path + "\"); "
               "if (text == \"file contents\") { print(\"roundtrip ok\"); } }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("roundtrip ok\n") != std::string::npos);
  std::error_code ignored;
  std::filesystem::remove_all(directory, ignored);
}

TEST_CASE("vNext prelude exports predicates and helpers", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source",
               "import prelude; fun main() { "
               "print(not(false)); "
               "const if (is_ref<ref<i64>>) { print(\"ref detected\"); } "
               "assert(!not(true)); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("true\nref detected\n") != std::string::npos);
}

TEST_CASE("vNext stdlib basics example runs end to end through ngi", "[vNext][Stdlib][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/stdlib_basics.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("Hello, World!\napple | banana | cherry\ntrue\nbaz bar baz\n") != std::string::npos);
  REQUIRE(output.find("HELLO\nworld\ntrue\n") != std::string::npos);
}
