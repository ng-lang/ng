#include "../../../3rdparty/Catch2/include/catch.hpp"
#include <test.hpp>

using namespace NG;
using namespace NG::AST;
using namespace NG::Parsing;

static ASTRef<ASTNode> parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

TEST_CASE("parser should parse function", "[ParserTest]") {
    auto ast = parse("fun id(n) { return n; }");

    delete ast;
}

TEST_CASE("parser should parse arrow return", "[ParserTest]") {
    auto ast = parse("fun id (n) => n;");

    delete ast;
}

TEST_CASE("parser should parse if structure", "[ParserTest]") {
    auto ast = parse("fun id4(y) if (y) return y;");

    delete ast;

    ast = parse("fun id5(z) { if (x) return y; else return z; }");

    delete ast;

    ast = parse("fun id6(x) if (x) { if (y) return z; else return x1; } else return x2;");

    delete ast;

    ast = parse("fun id7(x) if (x) { if (y) return z; else return x1; } else if (y) return z;");

    delete ast;
}

TEST_CASE("parser should parse return", "[ParserTest]") {
    auto ast = parse("fun one() { return 1; }");

    delete ast;
}

TEST_CASE("parser should parse assignment", "[ParserTest]") {
    auto ast = parse("fun id(n) { val x = n; return x; }");

    delete ast;
}

TEST_CASE("parser should parse function calls", "[ParserTest]") {
    auto ast = parse("fun id(a, b, c, e, f, g) { a(b); b(c); e(c)(b)(a); f(a, b, c); f(b(c), a(b), c); }");

    delete ast;
}

TEST_CASE("parser should parse id accessors", "[ParserTest]") {
    auto ast = parse(R"(
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
        }
    )");

    delete ast;
}

TEST_CASE("parser should parse operators", "[ParserTest]") {
    auto ast = parse(R"(
        fun id() {
            return 1 + 2 * 3 / 4.times(5);
        }
    )");

    delete ast;
}

TEST_CASE("parser should parse modules", "[ParserTest]") {
    auto ast = parse(R"(
        module fuck;
        module you;
        module stupid;
        module whatever.thisMod;
    )");

    REQUIRE(static_cast<ASTRef<Module>>(ast)->name == "fuck.you.stupid.whatever.thisMod");
    delete ast;
}

TEST_CASE("parser should parse strings", "[ParserTest]") {
    auto ast = parse(R"(
        module hello;
        fun hi() {
            return "hello world";
        }
    )");

    delete ast;
}

TEST_CASE("parser should parse value definitions", "[ParserTest]") {
    auto ast = parse(R"(
        val x = 1;
        val y = 2;
    )");

    delete ast;
}

TEST_CASE("parser should parse true/false literals", "[ParserTest]") {
    auto ast = parse(R"(
        val x = true;
        val y = false;
    )");

    delete ast;
}

TEST_CASE("parser should parse array literals", "[ParserTest]") {
    auto ast = parse(R"(
        val x = [1, 2, 3, 4, 5];
    )");

    delete ast;
}