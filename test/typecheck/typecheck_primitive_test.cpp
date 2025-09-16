#include "typecheck_utils.hpp"

TEST_CASE("should be able check primitive definitions", "[TypeCheck][Primitive][ValueDefinition]")
{
    auto ast = parse(R"(
            val x: int = 100;
            val y: long = 100 + 200;
            val greater: bool = 10 > 1;
            val z: short = 3i8 % 2i16;
        )");

    REQUIRE(ast != nullptr);

    auto index = type_check(ast);

    REQUIRE(index["x"]->tag() == typeinfo_tag::I32);
    check_type_tag(*index["x"], typeinfo_tag::I32);
    check_type_tag(*index["y"], typeinfo_tag::I64);
    check_type_tag(*index["greater"], typeinfo_tag::BOOL);
    check_type_tag(*index["z"], typeinfo_tag::I16);

    destroyast(ast);
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
    typecheck_failure("val x: bool = 2.0 != 1;", "Mismatch type on comparison operators");
    typecheck_failure("val x = false % 1;", "Invalid type for modulus");
    typecheck_failure("val x = 1 % false;", "Mismatch type on arithmetic operation");
    typecheck_failure("val x: int = y;", "Unknown type for object: y");
}