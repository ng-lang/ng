#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse strings", "[ParserTest]")
{
    auto astResult = parse(R"(
        module hello;
        fun hi() {
            return "hello world";
        }
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse value definitions", "[ParserTest]")
{
    auto astResult = parse(R"(
        val x = 1;
        val y = 2;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse true/false literals", "[ParserTest]")
{
    auto astResult = parse(R"(
        val x = true;
        val y = false;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse array literals", "[ParserTest]")
{
    auto astResult = parse(R"(
        val x = [1, 2, 3, 4, 5];
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse index accessor expression", "[ParserTest]")
{
    auto astResult = parse(R"(
        x[1];
        y["abc"];
        z[x[1]];
        j.k()[l[1][2]].m();
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse array index assign expression", "[ParserTest]")
{
    auto astResult = parse(R"(
        d.x[1] = 2;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse builtin integral and floating_point values", "[ParserTest]")
{
    auto astResult = parse(R"(
        val x: int = 1i32;

        val y: uint = 1u32;

        val a: f32 = 1.0;

        val y: f64 = 2.6f64;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("Parser should parse loop statement", "[ParserTestLoop]")
{
    auto astResult = parse(R"(
        loop x = 1 {
            if (x < 10) {
                next x + 1;
            }
        }

        loop a in [1, 2, 3], b = 2 {
            if (b < 5) {
                next a.iter(), b + 1;
            }
        }

    )");

    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

void match(const std::regex &regex, const Str &text, bool match)
{
    REQUIRE(std::regex_match(text, regex) == match);
}

TEST_CASE("Regex match", "[ParserTestRegexMatch]")
{
    std::regex pattern{"^[A-Za-z_][A-Za-z_\\-0-9\\.]+$"};

    match(pattern, "abc", true);
    match(pattern, "_abc", true);
    match(pattern, "a1c", true);
    match(pattern, "a_c", true);
    match(pattern, "a-c", true);
    match(pattern, "ab-", true);
    match(pattern, "ab_", true);
    match(pattern, "ab_123", true);
    match(pattern, "0a", false);
    match(pattern, "-a", false);
}
