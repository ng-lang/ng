#include "typecheck_utils.hpp"

void check_primitive_type(TypeInfo &typeInfo, primitive_tag primitive_tag)
{
    PrimitiveType &primitive = static_cast<PrimitiveType &>(typeInfo);

    REQUIRE(primitive.primitive() == primitive_tag);
}

TEST_CASE("should be able check primitive definitions", "[TypeCheck][Primitive][ValueDefinition]")
{
    auto astResult = parse(R"(
            val x: int = 100;
            val y: long = 100 + 200;
            val greater: bool = 10 > 1;
            val z: short = 3i8 % 2i16;
        )");

    REQUIRE(astResult.has_value());

    auto index = type_check(*astResult);

    debug_log("index", index.size());

    REQUIRE(index["x"]->tag() == typeinfo_tag::PRIMITIVE);
    check_primitive_type(*index["x"], primitive_tag::I32);
    check_primitive_type(*index["y"], primitive_tag::I64);
    check_primitive_type(*index["greater"], primitive_tag::BOOL);
    check_primitive_type(*index["z"], primitive_tag::I16);
}

TEST_CASE("should type check primitives fail", "[Primitive][TypeCheck][Failure]")
{
    typecheck_failure("val x: int = 1.0;", "Type Mismatch");
    typecheck_failure("val x: int = 1 + 2.0;", "Mismatch type on arithmetic operation");
    typecheck_failure("val x: int = 1 - 2.0;", "Mismatch type on arithmetic operation");
    typecheck_failure("val x: int = 1 * 2.0;", "Mismatch type on arithmetic operation");
    typecheck_failure("val x: int = 1 / 2.0;", "Mismatch type on arithmetic operation");
    typecheck_failure("val x: bool = 2.0 < 1;", "Mismatch type on comparison operators");
    typecheck_failure("val x: bool = 2.0 <= 1;", "Mismatch type on comparison operators");
    typecheck_failure("val x: bool = 2.0 > 1;", "Mismatch type on comparison operators");
    typecheck_failure("val x: bool = 2.0 >= 1;", "Mismatch type on comparison operators");
    typecheck_failure("val x: bool = 2.0 == 1;", "Mismatch type on comparison operators");
    // todo: fix `!` lexing issue
    // typecheck_failure("val x: bool = 2.0 != 1;", "Mismatch type on comparison operators");
    typecheck_failure("val x = false % 1;", "Invalid type for modulus");
    typecheck_failure("val x = 1 % false;", "Mismatch type on arithmetic operation");
}