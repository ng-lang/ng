#include "typecheck_utils.hpp"

TEST_CASE("should be able check unary expression", "[TypeCheck][UnaryExpression][ValueDefinition]")
{
    auto astResult = parse(R"(
            val x: int = 100;
            val y: int = -x;
            val z = -5.0;
            val result = !z;
        )");

    REQUIRE(astResult.has_value());

    auto index = type_check(*astResult);

    REQUIRE(index["x"]->tag() == typeinfo_tag::PRIMITIVE);
    check_primitive_type(*index["x"], primitive_tag::I32);
    check_primitive_type(*index["y"], primitive_tag::I32);
    check_primitive_type(*index["z"], primitive_tag::F32);
    check_primitive_type(*index["result"], primitive_tag::BOOL);
}

TEST_CASE("should type check unary expression fail", "[UnaryExpression][TypeCheck][Failure]")
{
    typecheck_failure("val x: int = -5u8;", "Invalid operand type");
    typecheck_failure("val x: int = -false;", "Invalid operand type");
    typecheck_failure("val x: int = ?false;", "Not supported operator");
}