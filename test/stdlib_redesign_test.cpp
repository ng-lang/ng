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

TEST_CASE("vNext std.string extends with length, charAt, and substring", "[vNext][Stdlib][Runtime]")
{
  expectValue("import string; fun main() -> i64 { let text = \"hello world\"; let mut total = 0; "
              "if (length(text) == 11) { total := total + 1; } "
              "if (charAt(text, 0) == \"h\") { total := total + 2; } "
              "if (substring(text, 0, 5) == \"hello\") { total := total + 4; } "
              "if (toLower(\"NG\") == \"ng\") { total := total + 8; } "
              "return total; }",
              "15");
}

TEST_CASE("vNext std.seq sums and searches value-typed arrays", "[vNext][Stdlib][Runtime]")
{
  expectValue("import seq; fun main() -> i64 { let xs = [1, 2, 3]; let mut total = 0; "
              "if (sum(xs) == 6) { total := total + 1; } "
              "if (arrayContains(xs, 2)) { total := total + 2; } "
              "if (!arrayContains(xs, 9)) { total := total + 4; } "
              "return total; }",
              "7");
}

TEST_CASE("vNext std.list traverses recursive sequences", "[vNext][Stdlib][Runtime]")
{
  expectValue("import list; fun main() -> i64 { "
              "let empty: List<i64> = List.Nil; "
              "let one: List<i64> = List.Cons(1, ref empty); "
              "let two: List<i64> = List.Cons(2, ref one); "
              "let three: List<i64> = List.Cons(3, ref two); "
              "let mut total = 0; "
              "if (length(ref three) == 3) { total := total + 1; } "
              "if (get(ref three, 0) == 3) { total := total + 2; } "
              "if (contains(ref three, 2)) { total := total + 4; } "
              "return total; }",
              "7");
}

TEST_CASE("vNext std.list builders rebuild sequences immutably", "[vNext][Stdlib][Runtime]")
{
  expectValue("import list; fun main() -> i64 { "
              "let made = listFrom([1, 2, 3]); "
              "let grown = append(ref made, 7); "
              "let front = pushFront(ref made, 5); "
              "let reversed = reverseList(ref made); "
              "let mut total = 0; "
              "if (length(ref grown) == 4 && get(ref grown, 3) == 7) { total := total + 1; } "
              "if (get(ref front, 0) == 5 && get(ref front, 1) == 1) { total := total + 2; } "
              "if (get(ref reversed, 0) == 3 && get(ref reversed, 2) == 1) { total := total + 4; } "
              "if (length(ref made) == 3) { total := total + 8; } "
              "return total; }",
              "15");
}

TEST_CASE("vNext generic let annotations and native instances compose", "[vNext][Stdlib][Runtime]")
{
  expectValue("import seq; fun annotate<T>(value: T) -> T { let copy: T = value; return copy; } "
              "fun main() -> i64 { let xs = [1, 2, 3]; "
              "if (len(xs) == 3 && annotate(4) == 4) { return 1; } return 0; }",
              "1");
}

TEST_CASE("vNext std.memory releases Box cells through Drop", "[vNext][Stdlib][Runtime]")
{
  expectValue("import memory; "
              "fun make() -> i64 { let mut first = box(7); write(ref first, 9); "
              "let mut total = read(ref first); let temp = box(100); "
              "if (outstanding() == 2) { total := total + 1; } return total; } "
              "fun main() -> i64 { let result = make(); "
              "if (outstanding() == 0) { return result + 2; } return 0; }",
              "12");
}

TEST_CASE("vNext std.string bounds errors surface as runtime errors", "[vNext][Stdlib][Errors]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import string; fun main() -> i64 { return length(charAt(\"ab\", 5)); }", output, errors) == 1);
  REQUIRE(errors.find("charAt index out of bounds") != std::string::npos);
}

TEST_CASE("vNext redesigned stdlib examples run end to end through ngi", "[vNext][Stdlib][Examples]")
{
  for (const auto &[filename, value] : std::vector<std::pair<std::string, std::string>>{
           {"example/std_list.ng", "with value 15"},
           {"example/list_builders.ng", "main returned"},
           {"example/list_literals.ng", "main returned"},
           {"example/heap_box.ng", "with value 12"},
           {"example/std_seq.ng", "with value 7"}})
  {
    std::string output;
    std::string errors;
    REQUIRE(runExample(filename, output, errors) == 0);
    INFO("errors: " << errors);
    REQUIRE(errors.empty());
    REQUIRE(output.find(value) != std::string::npos);
  }
}

TEST_CASE("vNext list collection literals build recursive lists in order", "[vNext][Stdlib][Runtime]")
{
  expectValue("import list; fun main() -> i64 { "
              "let xs: List<i64> = [1, 2, 3]; "
              "let empty: List<i64> = []; "
              "let words: List<string> = [\"a\", \"b\"]; "
              "let mut total = 0; "
              "if (length(ref xs) == 3 && get(ref xs, 0) == 1 && get(ref xs, 2) == 3) { total := total + 1; } "
              "if (length(ref empty) == 0) { total := total + 2; } "
              "if (get(ref words, 1) == \"b\") { total := total + 4; } "
              "return total; }",
              "7");

  std::string output;
  std::string errors;
  REQUIRE(run("import list; fun main() { let xs: List<i64> = [\"a\"]; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("list element"));
}
