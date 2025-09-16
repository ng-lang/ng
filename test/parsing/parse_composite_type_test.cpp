#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse array types", "[Parser][Type][Builtin][ValueDefinition]")
{
    auto ast = parse(R"(
        val x: [int] = [1];
        fun repeat_n(n: int, arr: [int]) -> [int] = native;
        val twoDimension: [[int]] = [[1, 2], [3, 4]];
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}
