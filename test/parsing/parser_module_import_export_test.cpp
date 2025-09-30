
#include "../test.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

using Catch::Matchers::ContainsSubstring;

TEST_CASE("parser should parse modules", "[Parser][Module]")
{
  auto ast = parse(R"(
        module foo;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should parse exports", "[Parser][Module][Export]")
{
  auto ast = parse(R"(
        // export all
        module hello exports *;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export symbol
        module hello exports (world);
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export multiple symbol
        module hello exports (a, b, c);
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export none
        module hello;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export without a module name
        module exports *;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("Should export single declaration", "[Parser][Export][Native]")
{
  auto ast = parse(R"(
        export val x = 1;

        export fun get() -> int = native;

        export type Simple {}
    )");

  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("Should not export statement", "[Parser][Export]")
{
  parseInvalid(
      R"(
        export loop x = 1 {
        }
    )",
      "Invalid export");
}