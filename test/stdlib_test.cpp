// AI-generated code; reviewed for this repository's vNext rewrite.
#include "driver.hpp"
#include "module_loader.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace modules = NG::modules;
namespace syntax = NG::syntax;

namespace
{
  [[nodiscard]] auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors)
      -> int
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
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example"))
      path = std::string{"../"} + path;
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
  REQUIRE(run({"--source", "import prelude; import string; fun main() { "
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
  REQUIRE(run({"--source", "import prelude; import io; fun main() { writeFile(\"" + path +
                               "\", \"file contents\"); "
                               "let text = readFile(\"" +
                               path +
                               "\"); "
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
  REQUIRE(run({"--source", "import prelude; fun main() { "
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

TEST_CASE("vNext std.string regexMatch matches and rejects", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() { "
                           "assert(regexMatch(\"alpha,beta,gamma\", \"alpha.*gamma\")); "
                           "assert(regexMatch(\"1-2-3\", \"[0-9]-[0-9]-[0-9]\")); "
                           "assert(!regexMatch(\"abc\", \"z+\")); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext std.string regexMatch reports invalid patterns", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() { assert(regexMatch(\"x\", \"[[\")); }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("regexMatch: invalid pattern"));
}

TEST_CASE("vNext std_string example runs end to end through ngi", "[vNext][Stdlib][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/std_string.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("main returned") != std::string::npos);
}

TEST_CASE("vNext std.string charAt and substring report runtime bounds errors", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(
      run({"--source", "import prelude; fun main() { print(charAt(\"abc\", 1)); print(substring(\"abcdef\", 2, 4)); }"},
          output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("b\ncd\n") != std::string::npos);

  REQUIRE(run({"--source", "import prelude; fun main() { print(charAt(\"abc\", 5)); }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("charAt index out of bounds: index 5, length 3"));

  REQUIRE(run({"--source", "import prelude; fun main() { print(substring(\"abc\", 0, 9)); }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("substring bounds out of range: [0..9) of length 3"));
}

TEST_CASE("vNext std.seq len reverse sum arrayContains execute end to end", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; import seq; fun main() { let xs = [1, 2, 3]; "
                           "print(len(xs)); print(reverse(xs)[0]); print(sum(xs)); print(arrayContains(xs, 2)); "
                           "print(arrayContains(xs, 9)); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("3\n3\n6\ntrue\nfalse\n") != std::string::npos);
}

TEST_CASE("vNext std.memory handles reuse freed slots", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; import memory; fun main() { "
                           "let h1 = allocate(7); let h2 = allocate(9); print(load(h1) + load(h2)); "
                           "release(h1); print(outstanding()); let h3 = allocate(5); print(load(h3)); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("16\n1\n5\n") != std::string::npos);
}

TEST_CASE("vNext runNgi re-enters the pipeline and captures diagnostics", "[vNext][Stdlib][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import prelude; fun main() { print(runNgi(\"fun main() -> i64 { return 41; }\")); }"},
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, ContainsSubstring("with value 41"));

  REQUIRE(run({"--source", "import prelude; fun main() { print(runNgi(\"fun main() { let x = ; }\")); }"}, output,
              errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, ContainsSubstring("syntax error at bytes ["));
  REQUIRE_THAT(output, ContainsSubstring("[exit 1]"));
}
