#include "../test.hpp"
#include <typecheck/typecheck.hpp>

using namespace NG::typecheck;

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