#include "typecheck_utils.hpp"

// Type Alias Tests

TEST_CASE("typealias is transparent - same type", "[TypeCheck][Nominal][TypeAlias]")
{
  auto ast = parse(R"(
        type Meters = f64;
        val x: Meters = 1.0;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::TYPE_ALIAS);
  destroyast(ast);
}

TEST_CASE("typealias is transparent - f64 assigned to alias", "[TypeCheck][Nominal][TypeAlias]")
{
  auto ast = parse(R"(
        type Meters = f64;
        val x: Meters = 1.0;
        val y: f64 = x;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  // y should be f64 since Meters is transparent
  check_type_tag(*index["y"], typeinfo_tag::F64);
  destroyast(ast);
}

TEST_CASE("typealias mismatch fails", "[TypeCheck][Nominal][TypeAlias][Failure]")
{
  typecheck_failure(R"(
        type Meters = f64;
        val x: Meters = 42;
    )", "Type Mismatch");
}

// NewType Tests

TEST_CASE("newtype definition", "[TypeCheck][Nominal][NewType]")
{
  auto ast = parse(R"(
        type UserId wraps i64;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["UserId"], typeinfo_tag::NEW_TYPE);
  destroyast(ast);
}

// Cast Expression Tests

TEST_CASE("cast to newtype wraps", "[TypeCheck][Nominal][Cast]")
{
  auto ast = parse(R"(
        type UserId wraps i64;
        val x = cast<UserId>(42);
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::NEW_TYPE);
  destroyast(ast);
}

TEST_CASE("cast primitive to primitive", "[TypeCheck][Nominal][Cast]")
{
  auto ast = parse(R"(
        val x = cast<i64>(42);
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::I64);
  destroyast(ast);
}

// Bidirectional Inference Tests

TEST_CASE("bidirectional inference - val with i64 annotation", "[TypeCheck][Nominal][Inference]")
{
  auto ast = parse(R"(
        val x: i64 = 42;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::I64);
  destroyast(ast);
}

TEST_CASE("bidirectional inference - val with f64 annotation", "[TypeCheck][Nominal][Inference]")
{
  auto ast = parse(R"(
        val x: f64 = 3.14f64;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::F64);
  destroyast(ast);
}

TEST_CASE("bidirectional inference - function return type", "[TypeCheck][Nominal][Inference]")
{
  auto ast = parse(R"(
        fun foo() -> i64 { return 42; }
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  // foo should have return type i64
  REQUIRE(index.contains("foo"));
  auto fooType = index["foo"];
  check_type_tag(*fooType, typeinfo_tag::FUNCTION);
  destroyast(ast);
}

TEST_CASE("bidirectional inference for function params", "[TypeCheck][Nominal][Inference]")
{
  // Function with typed return and Untyped param: argument type matches return expectation
  auto ast = parse(R"(
        fun id(x) -> i64 { return x; }
        val result = id(42);
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  // id returns i64, so result is i64
  check_type_tag(*index["result"], typeinfo_tag::I64);
  destroyast(ast);
}

TEST_CASE("inference defaults to i32 without annotation", "[TypeCheck][Nominal][Inference]")
{
  auto ast = parse(R"(
        val x = 42;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::I32);
  destroyast(ast);
}

TEST_CASE("inference defaults to f32 without annotation", "[TypeCheck][Nominal][Inference]")
{
  auto ast = parse(R"(
        val x = 3.14;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::F32);
  destroyast(ast);
}

// Multiple type definitions

TEST_CASE("multiple type definitions coexist", "[TypeCheck][Nominal]")
{
  auto ast = parse(R"(
        type Meters = f64;
        type Seconds = f64;
        type UserId wraps i64;
        type OrderId wraps i64;
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["Meters"], typeinfo_tag::TYPE_ALIAS);
  check_type_tag(*index["Seconds"], typeinfo_tag::TYPE_ALIAS);
  check_type_tag(*index["UserId"], typeinfo_tag::NEW_TYPE);
  check_type_tag(*index["OrderId"], typeinfo_tag::NEW_TYPE);
  destroyast(ast);
}

// Newtype Opacity Tests

TEST_CASE("newtype is opaque - cannot assign to wrapped type", "[TypeCheck][Nominal][NewType][Failure]")
{
  typecheck_failure(R"(
        type UserId wraps i64;
        val x = cast<UserId>(42);
        val y: i64 = x;
    )", "Type Mismatch");
}

TEST_CASE("newtype explicit unwrap via cast", "[TypeCheck][Nominal][Cast]")
{
  auto ast = parse(R"(
        type UserId wraps i64;
        val x = cast<UserId>(42);
        val y: i64 = cast<i64>(x);
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::NEW_TYPE);
  check_type_tag(*index["y"], typeinfo_tag::I64);
  destroyast(ast);
}

// Empty Array Inference

TEST_CASE("empty array inferred from annotation", "[TypeCheck][Nominal][Inference]")
{
  auto ast = parse(R"(
        val x: [i32] = [];
    )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["x"], typeinfo_tag::VECTOR);
  destroyast(ast);
}
