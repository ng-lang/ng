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
        val c = value.123(1, "arg");
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse spread operator and multiple bindings", "[Parser][Type][Tuple][Accessor]")
{
    auto ast = parse(R"(
        val tup: (i16, i32, i64, u8, i8) = (1, 2, 3, 4, 5);

        val (a: i16, b: i32, ...rest: (i64, u8, i8)) = tup;

        val new_tup = (0, ...tup, 6);
        
        val [a, b, ...c] = [1, 2, 3, 4, 5];

        val x = unit;
        val z: ((int, int, int), (int, int, int)) = (c, c);

        val (a, b, ...) = z;

    )");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->repr().find("...tup") != Str::npos);
    REQUIRE(ast->repr().find("...rest") != Str::npos);
    destroyast(ast);
}

TEST_CASE("parser should fail when parse invalid spread operator and multiple bindings",
          "[Parser][Type][Tuple][Accessor]")
{
    parseInvalid("val (x, ..., ) = (1, 2, 3);", "Unpacking binding must be last one");
}

TEST_CASE("parser should parse range slicing from-end index and pipeline expressions",
          "[Parser][Range][Pipeline]")
{
    auto ast = parse(R"(
        val xs = 0..=4;
        val window = xs[1..3];
        val prefix = xs[..3];
        val suffix = xs[2..];
        val last = xs[^1];
        val copy = xs |> reverse;
    )");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->repr().find("0..=4") != Str::npos);
    REQUIRE(ast->repr().find("xs[1..3]") != Str::npos);
    REQUIRE(ast->repr().find("xs[^1]") != Str::npos);
    destroyast(ast);
}

TEST_CASE("parser should parse postfix fold expressions", "[Parser][Fold]")
{
    auto ast = parse(R"(
        val ys = [inc(xs)...];
        val evens = [even(xs)?...];
        val right = plus(xs..., 0);
        val left = plus(0, xs...);
    )");
    REQUIRE(ast != nullptr);
    REQUIRE(ast->repr().find("inc(xs)...") != Str::npos);
    REQUIRE(ast->repr().find("even(xs)?...") != Str::npos);
    REQUIRE(ast->repr().find("xs...") != Str::npos);
    destroyast(ast);
}
