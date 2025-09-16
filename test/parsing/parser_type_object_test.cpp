#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse builtin types", "[Parser][Type][Builtin][ValueDefinition]")
{
    auto ast = parse(R"(
        val x: int = 1;

        val y: bool = false;
        val z: float = 1;

        type SomeType {}

        val some_object: SomeType = new SomeType {};
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse simple type definition", "[Parser][Type][Declaration]")
{
    auto ast = parse(R"(
        type Simple {}
        type WithProperties {
            property name;
        }
        type WithMultipleProperties {
            property name;
            property password;
        }
        type MixedPropertiesAndMembers {
            property name;
            property password;
            fun validate(name, password) {
                return self.password == password;
            }
        }
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("parser should parse new object creation", "[Parser][Object]")
{
    auto ast = parse(R"(
        val person = new Person {
            firstName: "Kimmy",
            lastName: "Leo"
        };
    )");
    REQUIRE(ast != nullptr);
    destroyast(ast);
}

TEST_CASE("Parser should parse type checking", "[Parser][Type]")
{
    auto ast = parse(R"(
        if (x is Type) {
            valid(x);
        }

        if (x is some_module.Type) {
            valid(x);
        }
    )");

    REQUIRE(ast != nullptr);
    destroyast(ast);
}
