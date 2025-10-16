#include "../test.hpp"
#include <regex>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse unary expression", "[Parser][UnaryExpression][Negative][Not]")
{
  auto ast = parse(R"(
        val x = 1;
        val y = -x;
        val a = !(x == 1);
        val b = !a;
        val z = ?c;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}
