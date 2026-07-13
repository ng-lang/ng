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

TEST_CASE("vNext ngi driver parses a source unit through the replacement module parser", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() { let value = 1; value }"}, output, errors) == 0);
  REQUIRE(output == "parsed vNext source unit with 1 module item(s)\n");
  REQUIRE(errors.empty());
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
