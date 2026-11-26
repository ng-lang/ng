#include "../test.hpp"
#include <parser.hpp>
#include <typecheck/typecheck.hpp>

using namespace NG;
using namespace NG::ast;

TEST_CASE("tagged union variant should know its parent type", "[TypecheckTest][TaggedUnion]")
{
  auto ast = parse(R"(
    type Shit = Fuck() | Dick();

    fun main() {
      val f = Fuck();
      val d = Dick();
    }
  )");
  REQUIRE(ast != nullptr);

  // Should not throw - Fuck() and Dick() are valid variants of Shit
  REQUIRE_NOTHROW(NG::typecheck::type_check(ast));
  destroyast(ast);
}

TEST_CASE("tagged union variant with payload should work", "[TypecheckTest][TaggedUnion]")
{
  auto ast = parse(R"(
    type Option = Some(value: i32) | None();

    fun main() {
      val s = Some(42);
      val n = None();
    }
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(NG::typecheck::type_check(ast));
  destroyast(ast);
}

TEST_CASE("variant should match its parent union type", "[TypecheckTest][TaggedUnion]")
{
  auto ast = parse(R"(
    type MyResult = Success(value: i32) | Failure(msg: string);

    fun main() {
      val r: MyResult = Success(42);
    }
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(NG::typecheck::type_check(ast));
  destroyast(ast);
}
