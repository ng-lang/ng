// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/driver.hpp"

#include <sstream>

namespace
{
  auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::vnext::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext ngi driver parses an expression without loading the legacy runtime", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--expr", "2 * 3 + 4"}, output, errors) == 0);
  REQUIRE(output == "parsed vNext expression at bytes [0, 9)\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver runs the complete replacement compile-verify-execute pipeline", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() { let value = 1; value }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s); main returned after 4 instruction(s)\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver passes typed i64 arguments to main", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main(value: i64) -> i64 { return value + 1; }", "--", "41"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s); main returned after 4 instruction(s) with value 42\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver passes string arguments and reports string returns", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main(name: string) -> string { return \"hello, \" + name; }", "--", "Ada"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s); main returned after 4 instruction(s) with value hello, Ada\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver rejects malformed runtime arguments", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main(value: i64) -> i64 { return value; }", "--", "nope"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE(errors == "invalid i64 argument `nope`\n");
}

TEST_CASE("vNext ngi driver executes direct calls through the module VM", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun helper(value: i64) -> i64 { return value + 1; } fun main() -> i64 { return helper(41); }"}, output,
              errors) == 0);
  REQUIRE(output == "compiled 2 vNext function(s); main returned after 7 instruction(s) with value 42\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver exposes a concrete main return value", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() -> i64 { return 6 * 7; }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s); main returned after 4 instruction(s) with value 42\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver reports typed pipeline errors through its new frontend boundary", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() { if 1 { return; } }"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE(errors == "type error at bytes [16, 17): if condition type mismatch: expected bool, got i64\n");
}

TEST_CASE("vNext ngi driver reports syntax errors through its new frontend boundary", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--expr", "a + @"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE(errors == "syntax error at bytes [4, 5): unexpected character `@`\n");
}

TEST_CASE("vNext ngi driver provides deterministic command-line diagnostics", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--expr"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE(errors == "--expr requires exactly one expression argument\n");
}
