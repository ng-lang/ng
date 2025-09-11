#include "typecheck_utils.hpp"

TEST_CASE("should be able parse and check function type", "[Function][TypeCheck]")
{
    auto astResult = parse(R"(
            fun add(a: int, b: int) -> int => a + b;

            fun greater_than(a: int, b: int) -> bool => a > b;

            fun consume(a: int) -> unit = native;

            val s: int = add(1, 2);
            val g: bool = greater_than(s, 1);
        )");

    REQUIRE(astResult.has_value());

    auto ast = *astResult;

    try
    {
        auto typeIndex = type_check(ast);
        REQUIRE(typeIndex.contains("add"));
        REQUIRE(typeIndex.contains("greater_than"));
        REQUIRE(typeIndex.contains("consume"));
        REQUIRE(typeIndex.contains("s"));
        REQUIRE(typeIndex.contains("g"));

        REQUIRE(typeIndex["add"]->repr() == "fun (i32, i32) -> i32");
        REQUIRE(typeIndex["greater_than"]->repr() == "fun (i32, i32) -> bool");
        REQUIRE(typeIndex["consume"]->repr() == "fun (i32) -> unit");
        REQUIRE(typeIndex["s"]->repr() == "i32");
        REQUIRE(typeIndex["g"]->repr() == "bool");
    }
    catch (TypeCheckingException &ex)
    {
        debug_log("Typechecking Exception", ex.what());
    }
}

TEST_CASE("should be able to check function with default value", "[Function][TypeCheck]")
{
    auto astResult = parse(R"(
            fun sum(x: int, acc: int = 0) -> int {
                if (x == 0) {
                    return acc;
                }
                next x-1, acc + x;
            }
            
            val x = sum(10);
        )");

    REQUIRE(astResult.has_value());

    auto ast = *astResult;

    auto typeIndex = type_check(ast);
    REQUIRE(typeIndex.contains("sum"));
    REQUIRE(typeIndex.contains("x"));

    REQUIRE(typeIndex["sum"]->repr() == "fun (i32, i32 = default) -> i32");
    REQUIRE(typeIndex["x"]->repr() == "i32");
}

TEST_CASE("shoud type function check fail", "[Function][TypeCheck][Failure]")
{
    typecheck_failure("fun f(x: int = 1.0) -> unit = native;", "Invalid default value");
    typecheck_failure("fun f(x: int) -> int = native; val x = f(false);", "Invalid argument");
    typecheck_failure("fun f(x: int) -> unit = native; val x: int = f(1);", "Type Mismatch");
    typecheck_failure("fun f(x: int = 1) -> int = native; val x: int = f(1.0);", "Invalid argument");
}
