#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse a forever loop", "[Parser][Loop][Next][ControlFlow]")
{
    auto ast = parse(R"(
        loop {
            next;
        }
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should loop with simple variables ", "[Parser][Loop][Next][ControlFlow]")
{
    auto ast = parse(R"(
        loop n {
            next;
        }
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should loop with variable type annotations", "[Parser][Loop][Next][ControlFlow]")
{
    auto ast = parse(R"(
        val x = 1;
        loop n : int = x {
            next;
        }
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should loop with mixied binding types", "[Parser][Loop][Next][ControlFlow]")
{
    auto ast = parse(R"(
        val x = 1;
        loop a, b = 1, c = 2, d : int = x {
            next;
        }
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}
