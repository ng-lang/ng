#include <catch.hpp>
#include <test.hpp>

using namespace NG;
using namespace NG::parsing;
using namespace NG::intp;
using namespace NG::ast;

static ASTRef<ASTNode> parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

static void interpret(const Str &source) {
    IInterperter *intp = NG::intp::interpreter();

    auto &&ast = parse(source);

    ast->accept(intp);

    intp->summary();

    delete intp;
}

TEST_CASE("interpreter should accept simple definitions", "[InterpreterTest]") {

    interpret(R"(
        val x = 1;
        val y = 2;
        val name = "ng";
        fun hello() {
          return "fuck";
        }
        val z = x + y;

        val g = hello();

        fun times(a, b) {
            return a * b;
        }

        val h = times(x, y);

        assert(z == 3, h == 2);
    )");
}

TEST_CASE("interpreter should run statements", "[InterpreterTest]") {

    interpret(R"(
        fun max(a, b) {
            if (a > b) {
                return a;
            }
            return b;
        }

        val x = max(1, 2);
        val y = max(5, 4);
        val g = max(x, y);
        val h = max(g, 10);

        assert(x == 2, y == 5, g == 5, h == 10);
    )");
}

TEST_CASE("interpreter should run recursion", "[InterpreterTest]") {

    interpret(R"(
        fun fact(x) {
            if (x > 0) {
                return x * fact(x-1);
            }
            return 1;
        }

        val z = fact(5);

        assert(z == 120);
    )");
}


TEST_CASE("interpreter should run complex recursion", "[InterpreterTest]") {

    interpret(R"(
        fun gcd(a, b) {
            val c = a % b;
            if (c == 0) {
                return b;
            }
            return gcd(b, c);
        }

        val g = gcd(60, 33);

        assert(g == 3);
    )");
}

TEST_CASE("shoud be able to interpret array literal", "[InterpreterTest]") {

    interpret(R"(
        val arr = [1, 2, 3, 4, 5];

        print(arr);
    )");
}

TEST_CASE("shoud be able to interpret array index", "[InterpreterTest]") {

    interpret(R"(
        val arr = [1, 2, 3, 4, 5];


        assert(arr[3] == 4);
        print(arr[3]);

        assert(arr[4] == 5);
        arr[4] = 6;
        assert(arr[4] == 6);

        print(arr);
    )");
}

TEST_CASE("should be able interpret string member function", "[InterpreterTest]") {
    interpret(R"(
        val x = "123";

        val y = x.size();

        assert(y == 3);
        print(y);

        assert(x.charAt(1) == 50);
        print(x.charAt(2));
    )");
}


TEST_CASE("should be able interpret simple type definition", "[InterpreterTest]") {
    interpret(R"(
type Person {
    property firstName;
    property lastName;
    property kid;

    fun name() {
        return self.firstName + " " + lastName;
    }
}

val person = new Person {
    firstName: "Kimmy",
    lastName: "Leo",
    kid: new Person {
        firstName: "Tiny",
        lastName: "Leo"
    }
};

assert(person.name().size() == 9);

print(person.kid.name());
)");
}


