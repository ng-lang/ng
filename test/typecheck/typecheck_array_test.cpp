#include "../test.hpp"

#include <typecheck/typecheck.hpp>
#include <typecheck/typeinfo.hpp>

#include "typecheck_utils.hpp"

using namespace NG::typecheck;

TEST_CASE("should match vector types", "[TypeCheck][Type][Array]")
{
  auto intType = makecheck<PrimitiveType>(typeinfo_tag::I32);
  auto floatType = makecheck<PrimitiveType>(typeinfo_tag::F32);
  auto vectorOfInt = makecheck<VectorType>(intType);
  auto vectorOfFloat = makecheck<VectorType>(floatType);
  auto anotherVectorOfInt = makecheck<VectorType>(intType);
  auto vectorOfVectorOfInt = makecheck<VectorType>(vectorOfInt);
  auto vectorOfVectorOfFloat = makecheck<VectorType>(vectorOfFloat);
  auto emptyVector = makecheck<VectorType>(makecheck<Untyped>());

  REQUIRE(vectorOfInt->match(*anotherVectorOfInt));
  REQUIRE(!vectorOfInt->match(*vectorOfFloat));
  REQUIRE(!vectorOfInt->match(*vectorOfVectorOfInt));
  REQUIRE(!vectorOfVectorOfInt->match(*vectorOfVectorOfFloat));

  REQUIRE(emptyVector->match(*vectorOfInt));
  REQUIRE(emptyVector->match(*vectorOfFloat));
  REQUIRE(emptyVector->match(*vectorOfVectorOfInt));
  REQUIRE(emptyVector->match(*vectorOfVectorOfFloat));

  REQUIRE(vectorOfInt->containing(*intType));
  REQUIRE(!vectorOfInt->containing(*floatType));
}

TEST_CASE("should match fixed array types by element and length", "[TypeCheck][Type][Array]")
{
  auto intType = makecheck<PrimitiveType>(typeinfo_tag::I32);
  auto len3 = makecheck<ConstValueType>("3", "i64");
  auto anotherLen3 = makecheck<ConstValueType>("3", "i64");
  auto len4 = makecheck<ConstValueType>("4", "i64");

  auto array3 = makecheck<ArrayType>(intType, len3);
  auto anotherArray3 = makecheck<ArrayType>(intType, anotherLen3);
  auto array4 = makecheck<ArrayType>(intType, len4);

  REQUIRE(array3->match(*anotherArray3));
  REQUIRE(!array3->match(*array4));
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
  check_type_tag(*index["empty"], typeinfo_tag::VECTOR);
  check_type_tag(*index["twoDimension"], typeinfo_tag::VECTOR);
  check_type_tag(*index["arr"], typeinfo_tag::VECTOR);

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
  // element and rest checks
  check_type_tag(*index["a"], typeinfo_tag::I32);
  check_type_tag(*index["b"], typeinfo_tag::I32);
  check_type_tag(*index["rest"], typeinfo_tag::VECTOR);

  // arrays constructed via spread
  check_type_tag(*index["arr2"], typeinfo_tag::VECTOR);
  check_type_tag(*index["arr3"], typeinfo_tag::VECTOR);

  // unpack with explicit rest type
  check_type_tag(*index["d"], typeinfo_tag::I32);
  check_type_tag(*index["e"], typeinfo_tag::I32);
  check_type_tag(*index["f"], typeinfo_tag::VECTOR);

  // unpack with explicit typed second element
  check_type_tag(*index["g"], typeinfo_tag::I32);
  check_type_tag(*index["h"], typeinfo_tag::I32);
  destroyast(ast);
}

TEST_CASE("should type check postfix fold expressions", "[TypeCheck][Array][Fold]")
{
  auto ast = parse(R"(
    fun inc(value: i32) -> i32 {
      return value + 1;
    }

    fun even(value: i32) -> bool {
      return (value % 2) == 0;
    }

    fun foldRightStep(value: i32, acc: i32) -> i32 {
      return value - acc;
    }

    fun foldLeftStep(acc: i32, value: i32) -> i32 {
      return acc - value;
    }

    val xs = [1, 2, 3];
    val ys = [inc(xs)...];
    val evens = [even(xs)?...];
    val mixed = [0, inc(xs)..., 9];
    val r: i32 Range = 0..4;
    val u: u32 Range = 1u32..10u32;
    val fromRange = [inc(r)...];
    val sp: i32 span = xs[0..2];
    val fromSpan = [inc(sp)...];
    val right = foldRightStep(xs..., 0);
    val left = foldLeftStep(0, xs...);
    val rangeRight = foldRightStep(r..., 0);
    val spanLeft = foldLeftStep(0, sp...);
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["ys"], typeinfo_tag::VECTOR);
  check_type_tag(*index["evens"], typeinfo_tag::VECTOR);
  check_type_tag(*index["mixed"], typeinfo_tag::VECTOR);
  check_type_tag(*index["r"], typeinfo_tag::RANGE);
  check_type_tag(*index["u"], typeinfo_tag::RANGE);
  check_type_tag(*index["fromRange"], typeinfo_tag::VECTOR);
  check_type_tag(*index["sp"], typeinfo_tag::SPAN);
  check_type_tag(*index["fromSpan"], typeinfo_tag::VECTOR);
  check_type_tag(*index["right"], typeinfo_tag::I32);
  check_type_tag(*index["left"], typeinfo_tag::I32);
  check_type_tag(*index["rangeRight"], typeinfo_tag::I32);
  check_type_tag(*index["spanLeft"], typeinfo_tag::I32);
  destroyast(ast);
}

TEST_CASE("should type check array fail", "[TypeCheck][Array][Failure]")
{
  typecheck_failure("val arr: [int] = [1, 2.0, 3];", "Mismatched element type in array literal");
  typecheck_failure("val arr: [int] = [1, true, 3];", "Mismatched element type in array literal");
  typecheck_failure(R"(
    fun inc(value: i32) -> i32 { return value + 1; }
    val xs = [1, 2, 3];
    val bad = [inc(xs)?...];
  )", "Filter fold expression must return bool");
  typecheck_failure(R"(
    fun plus(left: i32, right: i32) -> i32 { return left + right; }
    val xs = [1, 2, 3];
    val bad = plus(0, xs..., 4);
  )", "Fold call expects `op(xs..., init)` or `op(init, xs...)`");
  // typecheck_failure("val arr: [int] = [1, 2, 3u8];", "Mismatched element type in array literal");
  typecheck_failure("val arr: [int] = [1, 2, 3]; val x: int = arr[1.0];", "Invalid index type for vector");
  typecheck_failure("val arr: [int] = [1, 2, 3]; val x: int = arr[true];", "Invalid index type for vector");
  typecheck_failure("val arr: [int] = [1, 2, 3]; val x: int = arr[\"hello\"];", "Invalid index type for vector");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr[1.0] := 10;", "Invalid index type for vector");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr[true] := 10;", "Invalid index type for vector");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr[\"hello\"] := 10;", "Invalid index type for vector");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr[0] := 5.0;", "Invalid value type for vector assignment");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr[0] := true;", "Invalid value type for vector assignment");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr[0] := \"hello\";", "Invalid value type for vector assignment");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr << 5.0;", "Invalid element type for array push");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr << true;", "Invalid element type for array push");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr << \"hello\";", "Invalid element type for array push");
  typecheck_failure("val arr: [int] = [1, 2, 3]; arr + \"hello\";", "Unsupported operator for vector types");
  typecheck_failure("val arr = 1; arr[0] := 1;", "Index assignment on non-contiguous sequence type");
  typecheck_failure("val [a, b: bool, ...] = [1, 2, 3];", "Value Binding Type Mismatch: i32 to bool");
  typecheck_failure("val [a, b, ...c: [bool]] = [1, 2, 3];", "Value Binding Type Mismatch: vector<i32> to vector<bool>");
  typecheck_failure("val [a, b, ...c: [bool]] = 1;", "Value Binding Type Mismatch: i32 to array");
}

TEST_CASE("should type check fixed array const generic length", "[TypeCheck][Array][ConstGeneric]")
{
  auto ast = parse(R"(
            val fixed: array<int, 3> = [1, 2, 3];
            val first: int = fixed[0];
        )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["fixed"], typeinfo_tag::ARRAY);
  check_type_tag(*index["first"], typeinfo_tag::I32);
  destroyast(ast);

  typecheck_failure("val fixed: array<int, 2> = [1, 2, 3];", "Array literal length mismatch");
  typecheck_failure("val bad: array<int> = [1];", "Fixed array type expects 2 generic arguments");
  typecheck_failure("val bad: i32 array = [1];", "Fixed array type expects 2 generic arguments");
}

TEST_CASE("range expressions and slice syntax replace legacy std.array helpers", "[TypeCheck][Array][Range]")
{
  auto ast = parse(R"(
            val r: i32 Range = 0..5;
            val xs: i32 vector = [...r];
            val view: i32 span = xs[1..4];
            val ys: i32 vector = [...view];
        )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["r"], typeinfo_tag::RANGE);
  check_type_tag(*index["xs"], typeinfo_tag::VECTOR);
  check_type_tag(*index["view"], typeinfo_tag::SPAN);
  check_type_tag(*index["ys"], typeinfo_tag::VECTOR);
  destroyast(ast);

  typecheck_failure("val xs = range(0, 3);", "Unknown type for object: range");
  typecheck_failure("val xs = [1, 2, 3]; val ys = slice(xs, 0, 2);", "Unknown type for object: slice");
}
