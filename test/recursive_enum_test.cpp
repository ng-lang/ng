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

TEST_CASE("vNext multi-field enum variants construct and destructure", "[vNext][RecursiveEnum][Runtime]")
{
  expectValue("enum Pair { Both(left: i64, right: string), Empty, } "
              "fun main() -> i64 { let pair: Pair = Pair.Both(7, \"seven\"); "
              "switch (pair) { case Both(value, label) { if (label == \"seven\") { return value; } } case Empty { return 0; } } "
              "return 0; }",
              "7");
}

TEST_CASE("vNext recursive enum payloads link nodes through references", "[vNext][RecursiveEnum][Runtime]")
{
  expectValue("enum Node<T> { Cell(content: T, next: ref<Node<T>>), Empty, } "
              "fun count<T>(head: ref<Node<T>>) -> i64 { "
              "switch (*head) { case Empty { return 0; } case Cell(value, rest) { return 1 + count(rest); } } } "
              "fun main() -> i64 { let empty: Node<i64> = Node.Empty; "
              "let third: Node<i64> = Node.Cell(3, ref empty); "
              "let second: Node<i64> = Node.Cell(2, ref third); "
              "let first: Node<i64> = Node.Cell(1, ref second); "
              "return count(ref first); }",
              "3");
}

TEST_CASE("vNext recursive enum validation rejects arity mismatches", "[vNext][RecursiveEnum][Errors]")
{
  try
  {
    check("enum Pair { Both(left: i64, right: string), Empty, } "
          "fun main() -> i64 { let bad: Pair = Pair.Both(7); return 0; }");
    FAIL("expected a payload arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "enum variant `Both` expects 2 payload values, got 1");
  }

  try
  {
    check("enum Pair { Both(left: i64, right: string), Empty, } "
          "fun main() -> i64 { let pair: Pair = Pair.Both(7, \"x\"); "
          "switch (pair) { case Both(value) { return value; } case Empty { return 0; } } return 0; }");
    FAIL("expected a binding arity error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "return value type mismatch: expected i64, got tuple<i64, string>");
  }
}

TEST_CASE("vNext recursive enum example file runs end to end through ngi", "[vNext][RecursiveEnum][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/recursive_enums.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 3") != std::string::npos);
}
