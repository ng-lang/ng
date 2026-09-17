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

  void expectReject(std::string_view source, std::string_view message)
  {
    try
    {
      check(source);
      FAIL("expected a type error containing: " << message);
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE_THAT(std::string{error.what()}, Catch::Matchers::ContainsSubstring(std::string{message}));
    }
  }
} // namespace

TEST_CASE("vNext whole-array span views read elements without copying", "[vNext][Span][Runtime]")
{
  expectValue("fun main() -> i64 { let xs = [10, 20, 30, 40]; let v = span xs; return v[0] + v[3]; }", "50");
  expectValue("fun main() -> i64 { let xs = [1, 2]; let v: span<i64> = span xs; return v[1]; }", "2");
}

TEST_CASE("vNext window span views read the selected slice", "[vNext][Span][Runtime]")
{
  expectValue("fun main() -> i64 { let xs = [10, 20, 30, 40]; let v = span xs[1..3]; return v[0] + v[1]; }", "50");
}

TEST_CASE("vNext spans pass across call boundaries and through generics", "[vNext][Span][Runtime]")
{
  expectValue("fun sum(v: span<i64>) -> i64 { return v[0] + v[1]; } "
              "fun main() -> i64 { let xs = [5, 7, 9]; return sum(span xs); }",
              "12");
  expectValue("fun first<T>(v: span<T>) -> T { return v[0]; } "
              "fun main() -> i64 { let xs = [42, 1]; return first(span xs); }",
              "42");
  expectValue("fun pick(v: i64 span) -> i64 { return v[0]; } "
              "fun main() -> i64 { let xs = [3, 4]; return pick(span xs); }",
              "3");
}

TEST_CASE("vNext spans view string arrays", "[vNext][Span][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("fun pick(v: span<string>) -> string { return v[1]; } "
              "fun main() -> string { let words = [\"a\", \"b\", \"c\"]; return pick(span words); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("b") != std::string::npos);
}

TEST_CASE("vNext out-of-range span windows halt at runtime", "[vNext][Span][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("fun main() { let xs = [1, 2, 3]; let v = span xs[1..5]; return; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("killed by a signal"));

  REQUIRE(run("fun main() { let xs = [1, 2]; let v = span xs[-1..1]; return; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("killed by a signal"));
}

TEST_CASE("vNext out-of-range span element reads halt at runtime", "[vNext][Span][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("fun main() -> i64 { let xs = [1, 2, 3]; let v = span xs; return v[3]; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("killed by a signal"));

  REQUIRE(run("fun main() -> i64 { let xs = [1, 2, 3]; let v = span xs; return v[-2]; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("killed by a signal"));
}

TEST_CASE("vNext span types resolve and announce themselves", "[vNext][Span][Typecheck]")
{
  const auto module = resolve("fun f(values: span<i64>) -> i64 { return values[0]; }");
  REQUIRE(module.functions.front().parameters.front().typeName == "i64 span");
  REQUIRE_NOTHROW(check("fun main() { let xs = [1, 2]; let v = span xs; return; }"));
}

TEST_CASE("vNext span views cannot be stored or returned", "[vNext][Span][Errors]")
{
  expectReject("struct Wrap { v: span<i64> } fun main() { }", "spans cannot be stored in struct fields");
  expectReject("struct Wrap2 { v: span<i64> } fun main() { let xs = [1, 2]; let wrapped = Wrap2 { v: span xs }; }",
               "spans cannot be stored in struct fields");
  expectReject("fun main() { let xs = [1, 2]; let ys = [span xs]; return; }", "spans cannot be stored in arrays");
  expectReject("fun main() { let xs = [1, 2]; let t = (span xs, 3); return; }", "spans cannot be stored in tuples");
  expectReject("fun f() -> span<i64> { let xs = [1]; return span xs; }", "span views cannot be returned");
}

TEST_CASE("vNext span views reject writes, reslicing, and non-array sources", "[vNext][Span][Errors]")
{
  expectReject("fun main() { let xs = [1, 2]; let mut v = span xs; v[0] := 9; }",
               "cannot assign through a read-only span view");
  expectReject("fun main() { let xs = [1, 2, 3]; let v = span xs; let w = span v[0..2]; }",
               "slicing a span view is deferred");
  expectReject("fun main() { let xs = [1, 2, 3]; let v = span xs; let w = v[0..2]; }",
               "slicing a span view is deferred");
  expectReject("fun main() { let n = 5; let v = span n; }", "cannot span value of type i64");
  expectReject("fun main() { let v = span [1, 2]; }", "span operand is not a place");
  expectReject("fun main() { let xs = [1, 2]; let v = span xs[1]; }",
               "span window requires a range subscript `xs[lo..hi]`");
  expectReject("fun main() { let xs = [1, 2]; let mut v = span xs; v := span xs; }",
               "cannot reassign a span view binding");
  expectReject("struct Hold { values: array<i64> } fun main() { let h = Hold { values: [1, 2] }; let v = span h.values; }",
               "span over a structure field is deferred");
}

TEST_CASE("vNext span views conflict with mutable borrows of the source", "[vNext][Span][Errors]")
{
  try
  {
    check("fun main() { let mut xs = [1, 2]; let r = ref mut xs; let v = span xs; }");
    FAIL("expected an E-borrow-conflict type error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(error.code == "E-borrow-conflict");
    REQUIRE_THAT(std::string{error.what()}, ContainsSubstring("xs"));
  }

  {
    try
    {
      check("fun main() -> i64 { let mut xs = [1, 2]; let v = span xs; let r = ref mut xs; return v[0]; }");
      FAIL("expected an E-borrow-conflict type error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(error.code == "E-borrow-conflict");
      REQUIRE_THAT(std::string{error.what()}, ContainsSubstring("xs"));
    }
  }
}

TEST_CASE("vNext span example file runs end to end through ngi", "[vNext][Span][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/span.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 15") != std::string::npos);
}