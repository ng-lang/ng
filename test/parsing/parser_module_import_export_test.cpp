
#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse modules", "[Parser][Module]")
{
    auto astResult = parse(R"(
        module shit;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse exports", "[Parser][Module][Export]")
{
    auto astResult = parse(R"(
        // export all
        module hello exports *;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);

    astResult = parse(R"(
        // export symbol
        module hello exports (world);
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);

    astResult = parse(R"(
        // export multiple symbol
        module hello exports (a, b, c);
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);

    astResult = parse(R"(
        // export none
        module hello;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);

    astResult = parse(R"(
        // export without a module name
        module exports *;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("Should export single declaration", "[Parser][Export][Native]")
{
    auto astResult = parse(R"(
        export val x = 1;

        export fun get() -> int = native;

        export type Simple {}
    )");

    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("Should not export statement", "[Parser][Export]")
{
    auto astResult = parse(R"(
        export loop x = 1 {
        }
    )");

    REQUIRE(!astResult.has_value());
    // destroyast(*astResult);
}