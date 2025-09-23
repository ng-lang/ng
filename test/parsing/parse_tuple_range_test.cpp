#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse tuple and range values", "[Parser][Type][Literal][Tuple]")
{
    auto ast = parse(R"(
        val x = (1, false, "hello");
    )");
    REQUIRE(ast != nullptr);
    debug_log("Parse result:", ast->repr());
    destroyast(ast);
}

TEST_CASE("parser should parse value like accessors", "[Parser][Type][Tuple][Accessor]")
{
    auto ast = parse(R"(
        val x = tup.0;
        val y = tup.1;
        val z = tup.2;
        val a = value."property";
        val b = value.123;
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse spread operator and multiple bindings", "[Parser][Type][Tuple][Accessor]")
{
    auto ast = parse(R"(
        val tup = (1, 2, 3, 4, 5);

        val (a, b, ...rest) = tup;

        val new_tup = (0, ...tup, 6);
        
        val [a, b, ...c] = [1, 2, 3, 4, 5];
    )");
    REQUIRE(ast != nullptr);
    debug_log("Parse result:", ast->repr());
    destroyast(ast);
}
