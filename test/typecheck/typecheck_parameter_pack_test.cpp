#include "typecheck_utils.hpp"

TEST_CASE("generic function with pack parameter should type check", "[ParameterPack][TypeCheck]")
{
  auto ast = parse(R"(
            fun print_first<T...>(args: T...) -> unit {
                args;
            }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  REQUIRE(typeIndex.contains("print_first"));
}

TEST_CASE("pack parameter infers VarargsType on call", "[ParameterPack][TypeCheck]")
{
  auto ast = parse(R"(
            fun wrap<T...>(args: T...) -> unit {
                args;
            }

            val r = wrap(1, "hello", true);
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  REQUIRE(typeIndex.contains("wrap"));
  REQUIRE(typeIndex.contains("r"));
}

TEST_CASE("pack parameter with zero arguments", "[ParameterPack][TypeCheck]")
{
  auto ast = parse(R"(
            fun noop<T...>(args: T...) -> unit {
                args;
            }

            val r = noop();
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  REQUIRE(typeIndex.contains("noop"));
  REQUIRE(typeIndex.contains("r"));
}

TEST_CASE("pack parameter with single argument", "[ParameterPack][TypeCheck]")
{
  auto ast = parse(R"(
            fun identity<T...>(args: T...) -> unit {
                args;
            }

            val r = identity(42);
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  REQUIRE(typeIndex.contains("identity"));
  REQUIRE(typeIndex.contains("r"));
}