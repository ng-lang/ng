#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

static ASTRef<ASTNode> parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

TEST_CASE("parser should parse function", "[ParserTest]") {
    auto ast = parse("fun id(n) { return n; }");

    destroyast(ast);
}

TEST_CASE("parser should parse arrow return", "[ParserTest]") {
    auto ast = parse("fun id (n) => n;");

    destroyast(ast);
}

TEST_CASE("parser should parse if structure", "[ParserTest]") {
    auto ast = parse("fun id4(y) if (y) return y;");

    destroyast(ast);

    ast = parse("fun id5(z) { if (x) return y; else return z; }");

    destroyast(ast);

    ast = parse("fun id6(x) if (x) { if (y) return z; else return x1; } else return x2;");

    destroyast(ast);

    ast = parse("fun id7(x) if (x) { if (y) return z; else return x1; } else if (y) return z;");

    destroyast(ast);
}

TEST_CASE("parser should parse return", "[ParserTest]") {
    auto ast = parse("fun one() { return 1; }");

    destroyast(ast);
}

TEST_CASE("parser should parse assignment", "[ParserTest]") {
    auto ast = parse("fun id(n) { val x = n; return x; }");

    destroyast(ast);
}

TEST_CASE("parser should parse function calls", "[ParserTest]") {
    auto ast = parse("fun id(a, b, c, e, f, g) { a(b); b(c); e(c)(b)(a); f(a, b, c); f(b(c), a(b), c); }");

    destroyast(ast);
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

            a.b.c();
        }
    )");

    destroyast(ast);
}

TEST_CASE("parser should parse operators", "[ParserTest]") {
    auto ast = parse(R"(
        fun id() {
            return 1 + 2 * 3 / 4.times(5);
        }
    )");

    destroyast(ast);
}

TEST_CASE("parser should parse modules", "[ParserTest]") {
    auto ast = parse(R"(
        module fuck;
        module you;
        module stupid;
        module whatever.thisMod;
    )");

    // TODO: add assertion for compile unit
//    REQUIRE(dynamic_ast_cast<Module>(ast)->name == "fuck.you.stupid.whatever.thisMod");
    destroyast(ast);
}

TEST_CASE("parser should parse strings", "[ParserTest]") {
    auto ast = parse(R"(
        module hello;
        fun hi() {
            return "hello world";
        }
    )");

    destroyast(ast);
}

TEST_CASE("parser should parse value definitions", "[ParserTest]") {
    auto ast = parse(R"(
        val x = 1;
        val y = 2;
    )");

    destroyast(ast);
}

TEST_CASE("parser should parse true/false literals", "[ParserTest]") {
    auto ast = parse(R"(
        val x = true;
        val y = false;
    )");

    destroyast(ast);
}

TEST_CASE("parser should parse array literals", "[ParserTest]") {
    auto ast = parse(R"(
        val x = [1, 2, 3, 4, 5];
    )");

    destroyast(ast);
}

TEST_CASE("parser should parse index accessor expression", "[ParserTest]") {
    auto ast = parse(R"(
        x[1];
        y["abc"];
        z[x[1]];
        j.k()[l[1][2]].m();
    )");

    destroyast(ast);
}


TEST_CASE("parser should parse array index assign expression", "[ParserTest]") {
    auto ast = parse(R"(
        d.x[1] = 2;
    )");

    destroyast(ast);
}


TEST_CASE("parser should parse simple type definition", "[ParserTest]") {
    auto ast = parse(R"(
        type Simple {}
        type WithProperties {
            property name;
        }
        type WithMultipleProperties {
            property name;
            property password;
        }
        type MixedPropertiesAndMembers {
            property name;
            property password;
            fun validate(name, password) {
                return self.password == password;
            }
        }
    )");

    destroyast(ast);
}


TEST_CASE("parser should parse new object creation", "[ParserTest]") {
    auto ast = parse(R"(
val person = new Person {
    firstName: "Kimmy",
    lastName: "Leo"
};
    )");

    destroyast(ast);
}

TEST_CASE("parser should parse exports", "[ParserTest]") {
    auto ast = parse(R"(
// export all
module hello exports *;

// export symbol
module hello exports (world);

// export multiple symbol
module hello exports (a, b, c);

// export none
module hello;
)");

    destroyast(ast);
}

TEST_CASE("parser should parse imports", "[ParserTest]") {
    auto ast = parse(R"(
// simplified import
import hello;

// direct import
import "hello";

// import all symbols
import "hello" (*);

// import specific symbol
import "hello" (world);

// import symbols
import "hello" (a, b, c);

// import with alias
import "hello" hell;

// import all symbols
import "hello" *;

// import symbols with alias
import "hello" hell(a, b, c);
)");

    destroyast(ast);
}
