#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse function", "[Parser][Function]")
{
    auto ast = parse("fun id(n) { return n; }");
    REQUIRE(ast != nullptr);

    destroyast(ast);
}

TEST_CASE("parser should parse arrow return", "[Parser][Function][ArrowReturn]")
{
    auto ast = parse("fun id (n) => n;");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse if structure", "[Parser][Function][IfElse]")
{
    auto ast = parse("fun id4(y) if (y) return y;");
    REQUIRE(ast != nullptr);
    destroyast(ast);

    ast = parse("fun id5(z) { if (x) return y; else return z; }");
    REQUIRE(ast != nullptr);
    destroyast(ast);

    ast = parse("fun id6(x) if (x) { if (y) return z; else return x1; } else return x2;");
    REQUIRE(ast != nullptr);
    destroyast(ast);

    ast = parse("fun id7(x) if (x) { if (y) return z; else return x1; } else if (y) return z;");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse return", "[Parser][Function][Return]")
{
    auto ast = parse("fun one() { return 1; }");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse assignment", "[Parser][Assignment][ValueDefinition][Function]")
{
    auto ast = parse("fun id(n) { val x = n; return x; }");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse function calls", "[Parser][Function][FunctionCall]")
{
    auto ast = parse("fun id(a, b, c, e, f, g) { a(b); b(c); e(c)(b)(a); f(a, b, c); f(b(c), a(b), c); }");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse id accessors", "[Parser][Function][Accessor][Dot]")
{
    auto ast = parse(R"(
        fun id(a, b, c)
        {
            a.exec;
            b.gg;
            c.foo;
            a.bar(b);
            c.get(a, b);
            a.b.c;
            a.foo(b.c);
            a.b().c().defg(a.bc().foo);
            a.b.c();
        }
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse operators", "[Parser][Operator][Function]")
{
    auto ast = parse(R"(
        fun id() {
            return 1 + 2 * 3 / 4.times(5);
        }
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}
