#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse builtin types", "[Parser][Loop][Next][ControlFlow]")
{
    auto astResult = parse(R"(
        loop {
            next;
        }
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}