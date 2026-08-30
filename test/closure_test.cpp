// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-14 slice S1: closure syntax, explicit parameter typing, and direct
// calls through closure-typed locals. Captures arrive in S3/S4 and generic
// closures in S2, so this suite asserts the S1 negative contracts too.
#include "test.hpp"

#include "driver.hpp"
#include "syntax/parser.hpp"

#include <filesystem>
#include <sstream>

namespace syntax = NG::syntax;

namespace
{
  [[nodiscard]] auto asClosure(const syntax::ExpressionPtr &expression) -> const syntax::ClosureExpression &
  {
    const auto *closure = dynamic_cast<const syntax::ClosureExpression *>(expression.get());
    REQUIRE(closure != nullptr);
    return *closure;
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
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example"))
      path = std::string{"../"} + path;
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
    REQUIRE(output.find(std::string{"native main exited with code "} + std::string{value} + "\n") !=
            std::string::npos);
  }
} // namespace

TEST_CASE("vNext closure syntax parses single and multi-parameter heads", "[vNext][Closure][Syntax]")
{
  const auto singleSource = syntax::parseExpression("|x: i64| x + 1");
  const auto &single = asClosure(singleSource);
  REQUIRE(single.parameters.size() == 1);
  REQUIRE(single.parameters[0].name == "x");
  REQUIRE(single.parameters[0].type != nullptr);

  const auto multiSource = syntax::parseExpression("|a: i64, b: f64, c: bool| a");
  const auto &multi = asClosure(multiSource);
  REQUIRE(multi.parameters.size() == 3);
  REQUIRE(multi.parameters[1].name == "b");
}

TEST_CASE("vNext closure parses zero-parameter and block bodies", "[vNext][Closure][Syntax]")
{
  const auto blockSource = syntax::parseExpression("|x: i64| { let y = x * 2; y + 1 }");
  const auto &block = asClosure(blockSource);
  REQUIRE(block.parameters.size() == 1);
  REQUIRE(block.body->statements.size() == 1);
  REQUIRE(block.body->tailExpression != nullptr);
}

TEST_CASE("vNext closure parameters may omit types until INC-14 S2", "[vNext][Closure][Syntax]")
{
  const auto closureSource = syntax::parseExpression("|x| x + 1");
  const auto &closure = asClosure(closureSource);
  REQUIRE(closure.parameters.size() == 1);
  REQUIRE(closure.parameters[0].type == nullptr);
}

TEST_CASE("vNext closure calls run through the native backend", "[vNext][Closure][Runtime]")
{
  expectValue("fun main() -> i64 { let f = |x: i64| x + 1; return f(41); }", "42");
  expectValue("fun main() -> i64 { let g = |a: i64, b: i64| a * b; return g(6, 7); }", "42");
}

TEST_CASE("vNext closure block bodies run statements before the tail", "[vNext][Closure][Runtime]")
{
  expectValue("fun main() -> i64 { let f = |x: i64| { let y = x * 2; y + 1 }; return f(11); }", "23");
}

TEST_CASE("vNext closure body with side effects returns unit when tailless", "[vNext][Closure][Runtime]")
{
  std::string output;
  std::string errors;
  const int status =
      run("import prelude; fun main() -> i64 { let log = |x: i64| print(x); log(7); return 0; }", output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("7\n"));
}

TEST_CASE("vNext closure is usable inside a helper function", "[vNext][Closure][Runtime]")
{
  expectValue("fun apply(x: i64) -> i64 { let d = |v: i64| v * 2; return d(x); } "
              "fun main() -> i64 { return apply(21); }",
              "42");
}

TEST_CASE("vNext closure arity must match its call", "[vNext][Closure][TypeCheck]")
{
  std::string output;
  std::string errors;
  const int status = run("fun main() -> i64 { let f = |a: i64, b: i64| a + b; return f(1); }", output, errors);
  REQUIRE(status != 0);
  REQUIRE_THAT(errors, Catch::Matchers::ContainsSubstring("argument count mismatch"));
}

TEST_CASE("vNext closures require explicit parameter types in INC-14 S1", "[vNext][Closure][TypeCheck]")
{
  std::string output;
  std::string errors;
  const int status = run("fun main() -> i64 { let f = |x| x + 1; return f(1); }", output, errors);
  REQUIRE(status != 0);
  REQUIRE_THAT(errors, Catch::Matchers::ContainsSubstring("requires a type annotation"));
}

TEST_CASE("vNext closures reject captures until INC-14 S3", "[vNext][Closure][TypeCheck]")
{
  std::string output;
  std::string errors;
  const int status =
      run("fun main() -> i64 { let a = 5; let f = |x: i64| x + a; return f(1); }", output, errors);
  REQUIRE(status != 0);
  REQUIRE_THAT(errors, Catch::Matchers::ContainsSubstring("closure would capture"));
}

TEST_CASE("vNext closure types report params and result", "[vNext][Closure][TypeCheck]")
{
  expectValue("fun main() -> i64 { let f = |a: i64, b: i64| a + b; return f(2, 3); }", "5");
}

TEST_CASE("vNext closure example runs end to end", "[vNext][Closure][Example]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/closures.ng", output, errors) == 0);
  REQUIRE(errors.empty());
}