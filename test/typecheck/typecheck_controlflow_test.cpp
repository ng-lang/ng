#include "typecheck_utils.hpp"

TEST_CASE("should be able to check control and result types", "[Function][TypeCheck]")
{
    auto astResult = parse(R"(
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

    REQUIRE(astResult.has_value());

    auto ast = *astResult;

    auto typeIndex = type_check(ast);
    destroyast(*astResult);
}

TEST_CASE("should be able to should fail when unexpected things hapens", "[Function][TypeCheck]")
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