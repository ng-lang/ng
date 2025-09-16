#include "typecheck_utils.hpp"

TEST_CASE("should be able to parse and check function types", "[Function][TypeCheck]")
{
    auto ast = parse(R"(
            fun add(a: int, b: int) -> int => a + b;

            fun greater_than(a: int, b: int) -> bool => a > b;

            fun consume(a: int) -> unit = native;

            val s: int = add(1, 2);
            val g: bool = greater_than(s, 1);
        )");

    REQUIRE(ast != nullptr);

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

    destroyast(ast);
}

TEST_CASE("should be able to check function with default value", "[Function][TypeCheck]")
{
    auto ast = parse(R"(
            fun sum(x: int, acc: int = 0) -> int {
                if (x == 0) {
                    return acc;
                }
                next x-1, acc + x;
            }
            
            val x = sum(10);

            fun f(x = 1) -> int = native;
        )");

    REQUIRE(ast != nullptr);

    auto typeIndex = type_check(ast);
    REQUIRE(typeIndex.contains("sum"));
    REQUIRE(typeIndex.contains("x"));
    REQUIRE(typeIndex.contains("f"));

    REQUIRE(typeIndex["sum"]->repr() == "fun (i32, i32 = default) -> i32");
    REQUIRE(typeIndex["x"]->repr() == "i32");
    REQUIRE(typeIndex["f"]->repr() == "fun (i32 = default) -> i32");
    destroyast(ast);
}

TEST_CASE("should be able to check function with body", "[Function][TypeCheck]")
{
    auto ast = parse(R"(
            fun id(x: int) -> int {
                return x;
            }

            fun add(x: int, y: int) -> int {
                return x + y;
            }
            
            fun sum(x: int, acc: int = 0) -> int {
                if (x == 0) {
                    return acc;
                }
                next x-1, acc + x;
            }

            fun repeat_n(n: int, arr: [int]) -> [int] {
                val result: [int] = [];
                loop i = 0 {
                    if (i < n) {
                        next i + 1;
                    }
                    loop j = 0 {
                        if (j < arr.length) {
                            result << arr[j];
                            next j + 1;
                        }
                    }
                }
                return result;
            }

            fun sum_loop(n: int) -> int {
                loop n, acc = 0 {
                    if (n == 0) {
                        return acc;
                    }
                    next n - 1, acc + n;
                }
            }

            fun do_nothing() -> unit {
            }

            fun greater_than(a: int, b: int) -> bool {
                if (a > b) {
                    return true;
                }
                return false;
            }
        )");

    REQUIRE(ast != nullptr);

    auto typeIndex = type_check(ast);
    REQUIRE(typeIndex.contains("id"));
    REQUIRE(typeIndex.contains("add"));
    REQUIRE(typeIndex.contains("sum"));
    REQUIRE(typeIndex.contains("repeat_n"));
    REQUIRE(typeIndex.contains("sum_loop"));
    REQUIRE(typeIndex.contains("do_nothing"));
    REQUIRE(typeIndex.contains("greater_than"));

    REQUIRE(typeIndex["id"]->repr() == "fun (i32) -> i32");
    REQUIRE(typeIndex["add"]->repr() == "fun (i32, i32) -> i32");
    REQUIRE(typeIndex["sum"]->repr() == "fun (i32, i32 = default) -> i32");
    REQUIRE(typeIndex["repeat_n"]->repr() == "fun (i32, [i32]) -> [i32]");
    REQUIRE(typeIndex["sum_loop"]->repr() == "fun (i32) -> i32");
    REQUIRE(typeIndex["do_nothing"]->repr() == "fun () -> unit");
    REQUIRE(typeIndex["greater_than"]->repr() == "fun (i32, i32) -> bool");
    destroyast(ast);
}

TEST_CASE("should fail on function type-checking", "[Function][TypeCheck][Failure]")
{
    typecheck_failure("fun f(x: int = 1.0) -> unit = native;", "Invalid default value");
    typecheck_failure("fun f(x: int) -> int = native; val x = f(false);", "Invalid argument");
    typecheck_failure("fun f(x: int) -> unit = native; val x: int = f(1);", "Type Mismatch");
    typecheck_failure("fun f(x: int = 1) -> int = native; val x: int = f(1.0);", "Invalid argument");
    typecheck_failure("fun f(x: int) -> int = native; val x = f();", "Invalid argument");
    typecheck_failure("fun f(x: invalid) -> int = native; val x = f();", "Unknown type");
}

TEST_CASE("should fail on function body type-checking", "[Function][TypeCheck][Failure]")
{
    typecheck_failure("fun f(x: int = 1) -> unit { return 1; }", "Return Type Mismatch");
    typecheck_failure("fun f(x: int) -> unit { val b: bool = x; };", "Value Define Type Mismatch");
    typecheck_failure("fun f(x: int) -> int { }", "Return Type Mismatch");
    typecheck_failure("fun f(x: int = 1) -> int { return 1; next; }", "Next statement argument count mismatch");
    typecheck_failure("fun f(x: int) -> int { next 1; return; }", "Return Type Mismatch");
    typecheck_failure("fun f(x: int) -> int { if(true) {return 1;} return false; }", "Mismatched return types in compound statement");
}
