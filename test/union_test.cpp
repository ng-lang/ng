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
    REQUIRE(output.find(std::string{"native main exited with code "} + std::string{value} + "\n") != std::string::npos);
  }
} // namespace

TEST_CASE("vNext union types accept member values and compare against members", "[vNext][Union][Runtime]")
{
  expectValue("fun main() -> i64 { let x: i64 | string = 1; let y: i64 | string = \"hello\"; "
              "let mut total = 0; if (x == 1) { total := total + 1; } "
              "if (y == \"hello\") { total := total + 2; } return total; }",
              "3");
}

TEST_CASE("vNext union values flow through function parameters", "[vNext][Union][Runtime]")
{
  expectValue("fun read(value: bool | i64 | f64) -> i64 { if (value == 42) { return 7; } return 0; } "
              "fun main() -> i64 { let w: bool | i64 | f64 = 42; return read(w); }",
              "7");
}

TEST_CASE("vNext union validation rejects non-member values", "[vNext][Union][Errors]")
{
  try
  {
    check("fun main() -> i64 { let bad: bool | string = 5; return 0; }");
    FAIL("expected a non-member union error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "value of type i64 is not a member of union bool | string");
  }
}

TEST_CASE("vNext union switch matches member types, literals, and otherwise", "[vNext][Union][Runtime]")
{
  expectValue("fun classify(value: i64 | string) -> i64 { switch (value) { case 42 { return 100; } "
              "case (n: i64) { return n; } otherwise { return 7; } } } "
              "fun main() -> i64 { let a: i64 | string = 42; let b: i64 | string = 5; "
              "let c: i64 | string = \"hi\"; return classify(a) + classify(b) + classify(c); }",
              "112");
}

TEST_CASE("vNext union switch type patterns cover every member without otherwise", "[vNext][Union][Runtime]")
{
  expectValue("fun kind(value: i64 | string) -> i64 { switch (value) { case (n: i64) { return n; } "
              "case (s: string) { return 7; } } } "
              "fun main() -> i64 { let a: i64 | string = 5; let b: i64 | string = \"hi\"; "
              "return kind(a) + kind(b); }",
              "12");
}

TEST_CASE("vNext union switch matches string and float member literals", "[vNext][Union][Runtime]")
{
  // Regression: string member literals used to reach the integer payload
  // encoder, and a string compare against a non-string member dereferenced
  // the integer payload.
  expectValue("fun f(value: i64 | f64 | string) -> i64 { switch (value) { case \"go\" { return 1; } "
              "case 2.5 { return 3; } otherwise { return 2; } } } "
              "fun main() -> i64 { let a: i64 | f64 | string = \"go\"; let b: i64 | f64 | string = 2.5; "
              "let c: i64 | f64 | string = 7; return f(a) * 100 + f(b) * 10 + f(c); }",
              "132");
}

TEST_CASE("vNext union equality against a string skips non-string members", "[vNext][Union][Runtime]")
{
  expectValue("fun main() -> i64 { let c: i64 | string = 7; let s: i64 | string = \"go\"; let mut t = 0; "
              "if (c == \"go\") { t := t + 1; } if (s == \"go\") { t := t + 10; } "
              "if (c != \"go\") { t := t + 100; } return t; }",
              "110");
}

TEST_CASE("vNext union switch rejects a non-exhaustive member set", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (n: i64) { return n; } } }");
    FAIL("expected a non-exhaustive union switch error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} ==
            "switch is not exhaustive: missing member type `string` of union i64 | string "
            "(member-literal patterns match only their literal value, so add an `otherwise` or a type pattern "
            "for `string`)");
  }
}

TEST_CASE("vNext union switch rejects an unknown member type", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (b: bool) { return 1; } } }");
    FAIL("expected an unknown member type error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "`bool` is not a member type of union i64 | string");
  }
}

TEST_CASE("vNext union switch rejects a duplicate member pattern", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (a: i64) { return a; } "
          "case (b: i64) { return b; } } }");
    FAIL("expected a duplicate member pattern error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate member pattern for type `i64` in union switch");
  }
}

TEST_CASE("vNext union switch rejects a literal with no matching member family", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case true { return 1; } } }");
    FAIL("expected a literal member family error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "literal `true` does not match any member type of union i64 | string");
  }
}

TEST_CASE("vNext union switch rejects variant patterns", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case Foo { return 1; } } }");
    FAIL("expected a variant-pattern rejection");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "union switches match member types or literals, not variants (`Foo`)");
  }
}

TEST_CASE("vNext union switch requires a name: Type member pattern", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (i64) { return 1; } } }");
    FAIL("expected a type pattern binding parse error");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `:` after binding name in type pattern");
  }
}

TEST_CASE("vNext union example file runs end to end through ngi", "[vNext][Union][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/unions.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 123\n") != std::string::npos);
}
