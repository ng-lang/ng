#include "../test.hpp"

using namespace NG;
using namespace NG::parsing;
using namespace NG::intp;
using namespace NG::ast;

static ParseResult<ASTRef<ASTNode>> parse(const Str &source)
{
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

static void interpret(const Str &source)
{
    Interpreter *intp = NG::intp::stupid();

    auto astResult = parse(source);

    if (!astResult)
    {
        ParseError error = astResult.error();
        auto &&position = error.token.position;
        Str location = std::format("Location: {} / {}", position.line, position.col);

        debug_log("Error parse result:",
                  error.message,
                  location);
    }
    REQUIRE(astResult.has_value());

    auto &ast = *astResult;

    ast->accept(intp);

    intp->summary();

    delete intp;
    destroyast(ast);
}

TEST_CASE("interpreter should accept simple definitions", "[InterpreterTest]")
{

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

TEST_CASE("interpreter should run statements", "[InterpreterTest]")
{

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

TEST_CASE("interpreter should run recursion", "[InterpreterTest]")
{

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

TEST_CASE("interpreter should run complex recursion", "[InterpreterTest]")
{

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

TEST_CASE("shoud be able to interpret array literal", "[InterpreterTest]")
{

    interpret(R"(
        val arr = [1, 2, 3, 4, 5];

        print(arr);
    )");
}

TEST_CASE("shoud be able to interpret array index", "[InterpreterTest]")
{

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

TEST_CASE("should be able interpret string member function", "[InterpreterTest]")
{
    interpret(R"(
        val x = "123";

        val y = x.size();

        assert(y == 3);
        print(y);

        assert(x.charAt(1) == 50);
        print(x.charAt(2));
    )");
}

TEST_CASE("should be able interpret simple type definition", "[InterpreterTest]")
{
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

print(person.firstName);
)");
}

TEST_CASE("should be able interpret exports", "[InterpreterTestImport]")
{
    interpret(R"(
module hello exports (hello, a);

fun hello() {
  print("hello world");
}

val a = 1;

module main;

import "hello" (*);
hello();

module main2;

import "hello" hel;

hel.hello();

module main3;

import hello;

hello.hello();

)");
}

TEST_CASE("should be able interpret integral values", "[InterpreterTest]")
{
    interpret(R"(
val x = 1i8;

val y = 2u16;

val z = x + y;

print(z);

)");
}

TEST_CASE("should be able interpret integral & floating values", "[InterpreterTest]")
{
    interpret(R"(
val x = 1f32;

val y = 2f64;

val a = 3i32;

val z = x + y + a;

print(z);
)");
}

TEST_CASE("basic loop (single variable)", "[InterpreterTest]")
{
    interpret(R"(
fun sum(n) {
  val s = 0;
  loop i = 0 {
    s = s + i;
    if (i < n) {
      next i + 1;
    }
  }
  return s;
}

val result = sum(10);

assert(result == 55);

)");
}

TEST_CASE("basic tail recursion (with default parameters)", "[InterpreterTest]")
{
    interpret(R"(

fun sum(i, n = 0) {
  if (i == 0) {
    return n;
  }
  next i - 1, n + i;
}

val result = sum(10);

assert(result == 55);
)");
}

TEST_CASE("type checking", "[InterpreterTestChecking]")
{
    interpret(R"(
type SomeType {}

type OtherType {}

val some_obj = new SomeType {};

assert(some_obj is SomeType);
assert(not(some_obj is OtherType));
)");
}

TEST_CASE("object property update", "[InterpreterTestChecking]")
{
    interpret(R"(
type SomeType {
  property a;
}

val some_obj = new SomeType { a: 1 };

assert(some_obj.a == 1);

some_obj.a = 2;

assert(some_obj.a == 2);
)");
}