#include "typecheck_utils.hpp"

TEST_CASE("should be able to check control and result types", "[Function][TypeCheck]")
{
  auto ast = parse(R"(
        {
            val x = 1;
            val y = 2;
            x + 1;
            y + 2;
        }
        loop x = 1, y = 2 {
            next x + 1, y + 1;
        }
        if (true) {
            return 1;
        } else {
            return 2;
        }

        if (true) {
            return 1u8;
        } else {
            return 2;
        }
        
        if (false) {
            return 1;
        }
        
        fun print(x: int) -> unit = native;
        
        if (1 == 1) {
            print(1);
        } else {
            return 2;
        }

        if (2 == 1) {
            return 1;
        } else {
            print(2);
        }

        {
            if (3 > 2) {
                return 1u8;
            }

            return 2u16;
        }
        )");

  REQUIRE(ast != nullptr);

  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("should fail when check control flow with incompatible type", "[Function][TypeCheck]")
{

  typecheck_failure(
      R"(
            if (1) {}
    )",
      "Condition expression must be boolean");

  typecheck_failure(
      R"(
            if (true) { return 1; } else { return 2.0;}
    )",
      "Mismatched return types in if-else branches");

  typecheck_failure(
      R"(
            val x = 1;
            loop x {
                next;
            }
    )",
      "Next statement argument count mismatch");

  typecheck_failure(
      R"(
            val x = 1;
            loop x {
                next false;
            }
    )",
      "Next statement argument type mismatch");

  typecheck_failure(
      R"(
            loop n: int = false {
                next n + 1;
            }
    )",
      "Loop Binding Type Mismatch");

}

TEST_CASE("const if with boolean literal true", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (true) {
            val x: i32 = 1;
        } else {
            val y: f64 = 2.0;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with boolean literal false", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            val x: i32 = 1;
        } else {
            val y: f64 = 2.0;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with negation", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (!false) {
            val x: i32 = 1;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with binary AND", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (true && false) {
            val x: i32 = 1;
        } else {
            val y: i32 = 2;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with binary OR", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false || true) {
            val x: i32 = 1;
        } else {
            val y: f64 = 2.0;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if eliminates dead branch with incompatible types", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (true) {
            return 42;
        } else {
            return 3.14;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if false eliminates then branch", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            return 42;
        } else {
            return 3;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if without else branch", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            val x: i32 = 1;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if false with dead code inside", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            val x = 1 + 2;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}
