#include "../test.hpp"
#include <parser.hpp>

using namespace NG;
using namespace NG::ast;

// Helper to parse and verify AST is not null
static auto parseOrFail(const Str &source) -> ASTRef<ASTNode>
{
  auto ast = parse(source);
  REQUIRE(ast != nullptr);
  return ast;
}

TEST_CASE("parser should parse error propagation expression", "[ParserTest][ErrorPropagation]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val x = foo()?;
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse lambda with capture list", "[ParserTest][Lambda]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val x = 10;
      val f = [x] (a: i32) -> i32 => a + x;
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse lambda with by-ref capture", "[ParserTest][Lambda]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val x = 10;
      val f = [&x] (a: i32) -> i32 => a + x;
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse lambda with capture all by ref", "[ParserTest][Lambda]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val x = 10;
      val f = [&] (a: i32) -> i32 => a + x;
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse lambda with capture all by value", "[ParserTest][Lambda]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val x = 10;
      val f = [] (a: i32) -> i32 => a + x;
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse lambda without capture list", "[ParserTest][Lambda]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val f = (x: i32) -> i32 => x * 2;
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse lambda without type annotations", "[ParserTest][Lambda]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val f = (x) => x * 2;
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse lambda with block body", "[ParserTest][Lambda]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val f = (x: i32) -> i32 {
        return x * 2;
      };
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse switch expression with integer cases", "[ParserTest][SwitchExpr]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val x = 42;
      val myResult = switch (x) {
        case 0 => 0;
        case 1 => 1;
        otherwise => 99;
      };
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse switch expression with when guard", "[ParserTest][SwitchExpr]")
{
  auto ast = parseOrFail(R"(
    fun main() {
      val n = 15;
      val myResult = switch (n) {
        case 0 => 0;
        case x when x < 10 => 1;
        otherwise => 99;
      };
    }
  )");
  destroyast(ast);
}

TEST_CASE("parser should parse switch expression with tagged union", "[ParserTest][SwitchExpr]")
{
  auto ast = parseOrFail(R"(
    type MyResult = Success(value: i32) | Failure(msg: string);

    fun main() {
      val myR = Success(42);
      val myResult = switch (r) {
        case Success(v) => v;
        case Failure(_) => -1;
        otherwise => -2;
      };
    }
  )");
  destroyast(ast);
}
