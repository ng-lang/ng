#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse function", "[Parser][Function]")
{
    auto astResult = parse("fun id(n) { return n; }");
    REQUIRE(astResult.has_value());

    destroyast(*astResult);
}

TEST_CASE("parser should parse arrow return", "[Parser][Function][ArrowReturn]")
{
    auto astResult = parse("fun id (n) => n;");
    REQUIRE(astResult.has_value());

    auto value = *astResult;

    destroyast(*astResult);
}

TEST_CASE("parser should parse if structure", "[Parser][Function][IfElse]")
{
    auto astResult = parse("fun id4(y) if (y) return y;");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);

    astResult = parse("fun id5(z) { if (x) return y; else return z; }");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);

    astResult = parse("fun id6(x) if (x) { if (y) return z; else return x1; } else return x2;");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);

    astResult = parse("fun id7(x) if (x) { if (y) return z; else return x1; } else if (y) return z;");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse return", "[Parser][Function][Return]")
{
    auto astResult = parse("fun one() { return 1; }");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse assignment", "[Parser][Assignment][ValueDefinition][Function]")
{
    auto astResult = parse("fun id(n) { val x = n; return x; }");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse function calls", "[Parser][Function][FunctionCall]")
{
    auto astResult = parse("fun id(a, b, c, e, f, g) { a(b); b(c); e(c)(b)(a); f(a, b, c); f(b(c), a(b), c); }");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse id accessors", "[Parser][Function][Accessor][Dot]")
{
    auto astResult = parse(R"(
        fun id(a, b, c)
        {
            a.exec;
            b.gg;
            c.fuck;
            a.shit(b);
            a.fuck(b.c);
            c.get(a, b);
            a.b.c;
            a.b().c().defg(a.bc().fuck);

            a.b.c();
        }
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse operators", "[Parser][Operator][Function]")
{
    auto astResult = parse(R"(
        fun id() {
            return 1 + 2 * 3 / 4.times(5);
        }
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}
