#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

static ParseResult<ASTRef<ASTNode>> parse(const Str &source) {
    auto result =  Parser(ParseState(Lexer(LexState{source}).lex())).parse();
    if (!result) {
        debug_log(result.error());
    }
    return result;
}

TEST_CASE("parser should parse function", "[ParserTest]") {
    auto astResult = parse("fun id(n) { return n; }");
    REQUIRE(astResult.has_value());

    destroyast(*astResult);
}

TEST_CASE("parser should parse arrow return", "[ParserTest]") {
    auto astResult = parse("fun id (n) => n;");
    REQUIRE(astResult.has_value());

    auto value  = *astResult;

    destroyast(*astResult);
}

TEST_CASE("parser should parse if structure", "[ParserTest]") {
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

TEST_CASE("parser should parse return", "[ParserTest]") {
    auto astResult = parse("fun one() { return 1; }");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse assignment", "[ParserTest]") {
    auto astResult = parse("fun id(n) { val x = n; return x; }");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse function calls", "[ParserTest]") {
    auto astResult = parse("fun id(a, b, c, e, f, g) { a(b); b(c); e(c)(b)(a); f(a, b, c); f(b(c), a(b), c); }");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse id accessors", "[ParserTest]") {
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

TEST_CASE("parser should parse operators", "[ParserTest]") {
    auto astResult = parse(R"(
        fun id() {
            return 1 + 2 * 3 / 4.times(5);
        }
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse modules", "[ParserTest]") {
    auto astResult = parse(R"(
        module fuck;
        module you;
        module stupid;
        module whatever.thisMod;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse strings", "[ParserTest]") {
    auto astResult = parse(R"(
        module hello;
        fun hi() {
            return "hello world";
        }
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse value definitions", "[ParserTest]") {
    auto astResult = parse(R"(
        val x = 1;
        val y = 2;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse true/false literals", "[ParserTest]") {
    auto astResult = parse(R"(
        val x = true;
        val y = false;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse array literals", "[ParserTest]") {
    auto astResult = parse(R"(
        val x = [1, 2, 3, 4, 5];
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse index accessor expression", "[ParserTest]") {
    auto astResult = parse(R"(
        x[1];
        y["abc"];
        z[x[1]];
        j.k()[l[1][2]].m();
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}


TEST_CASE("parser should parse array index assign expression", "[ParserTest]") {
    auto astResult = parse(R"(
        d.x[1] = 2;
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}


TEST_CASE("parser should parse simple type definition", "[ParserTest]") {
    auto astResult = parse(R"(
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
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}


TEST_CASE("parser should parse new object creation", "[ParserTest]") {
    auto astResult = parse(R"(
val person = new Person {
    firstName: "Kimmy",
    lastName: "Leo"
};
    )");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse exports", "[ParserTest]") {
    auto astResult = parse(R"(
// export all
module hello exports *;

// export symbol
module hello exports (world);

// export multiple symbol
module hello exports (a, b, c);

// export none
module hello;
)");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}

TEST_CASE("parser should parse imports", "[ParserTest]") {
    auto astResult = parse(R"(
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
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}



TEST_CASE("parser should parse builtin types", "[ParserTestBuiltin]") {
    auto astResult = parse(R"(
val x: int = 1;

val y: bool = false;
val z: float = 1;

type SomeType {}

val some_object: SomeObject = new SomeObject {};
)");
    REQUIRE(astResult.has_value());
    destroyast(*astResult);
}
