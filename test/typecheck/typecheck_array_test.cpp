#include "../test.hpp"

#include <typecheck/typeinfo.hpp>
#include <typecheck/typecheck.hpp>

#include "typecheck_utils.hpp"

using namespace NG::typecheck;

TEST_CASE("should match array types", "[TypeCheck][Type][Array]")
{
    auto intType = makecheck<PrimitiveType>(typeinfo_tag::I32);
    auto floatType = makecheck<PrimitiveType>(typeinfo_tag::F32);
    auto arrayOfInt = makecheck<ArrayType>(intType);
    auto arrayOfFloat = makecheck<ArrayType>(floatType);
    auto anotherArrayOfInt = makecheck<ArrayType>(intType);
    auto arrayOfArrayOfInt = makecheck<ArrayType>(arrayOfInt);
    auto arrayOfArrayOfFloat = makecheck<ArrayType>(arrayOfFloat);
    auto emptyArray = makecheck<ArrayType>(makecheck<Untyped>());

    REQUIRE(arrayOfInt->match(*anotherArrayOfInt));
    REQUIRE(!arrayOfInt->match(*arrayOfFloat));
    REQUIRE(!arrayOfInt->match(*arrayOfArrayOfInt));
    REQUIRE(!arrayOfArrayOfInt->match(*arrayOfArrayOfFloat));

    REQUIRE(emptyArray->match(*arrayOfInt));
    REQUIRE(emptyArray->match(*arrayOfFloat));
    REQUIRE(emptyArray->match(*arrayOfArrayOfInt));
    REQUIRE(emptyArray->match(*arrayOfArrayOfFloat));

    REQUIRE(arrayOfInt->containing(*intType));
    REQUIRE(!arrayOfInt->containing(*floatType));
}

TEST_CASE("should type check arrays", "[TypeCheck][Array]")
{
    auto ast = parse(R"(
            val arr: [int] = [1, 2, 3];
            val empty: [int] = [];
            val twoDimension: [[int]] = [[1, 2u8], [3u8, 4]];
            val x: int = arr[0];
            val y: int = arr[x];
            val z: int = twoDimension[1][0];
            arr[0] := 10;
            arr[x] := 20;
            arr << 100;
        )");

    REQUIRE(ast != nullptr);

    auto index = type_check(ast);

    check_type_tag(*index["x"], typeinfo_tag::I32);
    check_type_tag(*index["y"], typeinfo_tag::I32);
    check_type_tag(*index["z"], typeinfo_tag::I32);
    check_type_tag(*index["empty"], typeinfo_tag::ARRAY);
    check_type_tag(*index["twoDimension"], typeinfo_tag::ARRAY);
    check_type_tag(*index["arr"], typeinfo_tag::ARRAY);

    destroyast(ast);
}

TEST_CASE("should type check array with spread and unpacking", "[TypeCheck][Array]")
{
    auto ast = parse(R"(
            val arr: [int] = [1, 2, 3];
            val [a, b, ...rest] = arr;
            val arr2: [int] = [0, ...arr, 4, 5];
            val arr3: [int] = [...arr, 6, 7, 8];
            val [d, e, ...f: [int]] = arr2;
            val [g, h: int, ...] = arr3;
        )");

    REQUIRE(ast != nullptr);

    auto index = type_check(ast);

    destroyast(ast);
}

TEST_CASE("should type check array fail", "[TypeCheck][Array][Failure]")
{
    typecheck_failure("val arr: [int] = [1, 2.0, 3];", "Mismatched element type in array literal");
    typecheck_failure("val arr: [int] = [1, true, 3];", "Mismatched element type in array literal");
    // typecheck_failure("val arr: [int] = [1, 2, 3u8];", "Mismatched element type in array literal");
    typecheck_failure("val arr: [int] = [1, 2, 3]; val x: int = arr[1.0];", "Invalid index type for array");
    typecheck_failure("val arr: [int] = [1, 2, 3]; val x: int = arr[true];", "Invalid index type for array");
    typecheck_failure("val arr: [int] = [1, 2, 3]; val x: int = arr[\"hello\"];", "Invalid index type for array");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr[1.0] := 10;", "Invalid index type for array");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr[true] := 10;", "Invalid index type for array");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr[\"hello\"] := 10;", "Invalid index type for array");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr[0] := 5.0;", "Invalid value type for array assignment");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr[0] := true;", "Invalid value type for array assignment");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr[0] := \"hello\";", "Invalid value type for array assignment");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr << 5.0;", "Invalid element type for array push");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr << true;", "Invalid element type for array push");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr << \"hello\";", "Invalid element type for array push");
    typecheck_failure("val arr: [int] = [1, 2, 3]; arr + \"hello\";", "Unsupported operator for array types");
    typecheck_failure("val arr = 1; arr[0] := 1;", "Index assignment on non-array type");
    typecheck_failure("val [a, b: bool, ...] = [1, 2, 3];", "Value Binding Type Mismatch: i32 to bool");
    typecheck_failure("val [a, b, ...c: [bool]] = [1, 2, 3];", "Value Binding Type Mismatch: [i32] to [bool]");
    typecheck_failure("val [a, b, ...c: [bool]] = 1;", "Value Binding Type Mismatch: i32 to array");
}