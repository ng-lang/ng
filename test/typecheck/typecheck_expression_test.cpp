#include "typecheck_utils.hpp"

TEST_CASE("should be able check unary expression", "[TypeCheck][UnaryExpression][ValueDefinition]")
{
    auto ast = parse(R"(
            val x: int = 100;
            val y: int = -x;
            val z = -5.0;
            val result = !z;
        )");

    REQUIRE(ast != nullptr);

    auto index = type_check(ast);

    check_type_tag(*index["x"], typeinfo_tag::I32);
    check_type_tag(*index["y"], typeinfo_tag::I32);
    check_type_tag(*index["z"], typeinfo_tag::F32);
    check_type_tag(*index["result"], typeinfo_tag::BOOL);

    destroyast(ast);
}

TEST_CASE("should type check unary expression fail", "[UnaryExpression][TypeCheck][Failure]")
{
    typecheck_failure("val x: int = -5u8;", "Invalid operand type");
    typecheck_failure("val x: int = -false;", "Invalid operand type");
    typecheck_failure("val x: int = ?false;", "Not supported operator");
}

TEST_CASE("should type check binary expression of unsupported type failure", "[BinaryExpression][TypeCheck][Failure]")
{

    typecheck_failure(
        R"(
            fun something_unit(x: int) -> unit = native;
            val x = something_unit(1);
            val y = x + 1;
        )",
        "Unsupported type for binary expression: unit");
}