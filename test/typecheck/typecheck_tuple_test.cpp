#include "../test.hpp"

#include <typecheck/typecheck.hpp>
#include <typecheck/typeinfo.hpp>

#include "typecheck_utils.hpp"

using namespace NG::typecheck;

TEST_CASE("should type tuples and unit", "[TypeCheck][Tuple]")
{
  auto ast = parse(R"(
            val x: unit = unit;
            val y: (int, unit) = (1, x);
            val z: (string, int, unit) = ("hello", ...y);
            val (a, b, ...f) = ("hello", ...y);
            val (c: int, d: unit) = (1, ...f);
            val (e, ...) = z;
            val (i, ...j: (int, unit)) = z;
        )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  check_type_tag(*index["x"], typeinfo_tag::UNIT);
  check_type_tag(*index["y"], typeinfo_tag::TUPLE);
  check_type_tag(*index["z"], typeinfo_tag::TUPLE);
  check_type_tag(*index["a"], typeinfo_tag::STRING);
  check_type_tag(*index["b"], typeinfo_tag::I32);
  check_type_tag(*index["c"], typeinfo_tag::I32);
  check_type_tag(*index["d"], typeinfo_tag::UNIT);
  check_type_tag(*index["f"], typeinfo_tag::TUPLE);
  check_type_tag(*index["i"], typeinfo_tag::STRING);
  check_type_tag(*index["j"], typeinfo_tag::TUPLE);
  destroyast(ast);
}

TEST_CASE("should preserve tuple slice types", "[TypeCheck][Tuple][Range]")
{
  auto ast = parse(R"(
            val tup: (int, string, bool, int) = (1, "two", true, 4);
            val mid: (string, bool) = tup[1..3];
            val tail: (bool, int) = tup[^2..];
        )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  REQUIRE(index["mid"]->repr() == "(string, bool)");
  REQUIRE(index["tail"]->repr() == "(bool, i32)");
  destroyast(ast);
}

TEST_CASE("should type check tuple numeric accessors", "[TypeCheck][Tuple][Accessor]")
{
  auto ast = parse(R"(
            val tup: (int, string, bool) = (1, "two", true);
            val first: int = tup.0;
            val second: string = tup.1;
            val third: bool = tup.2;
        )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  check_type_tag(*index["first"], typeinfo_tag::I32);
  check_type_tag(*index["second"], typeinfo_tag::STRING);
  check_type_tag(*index["third"], typeinfo_tag::BOOL);
  destroyast(ast);
}

TEST_CASE("should type check tuple fail", "[TypeCheck][Tuple][Failure]")
{
  typecheck_failure("val arr: unit = 1;", "Value Define Type Mismatch: i32 to unit");
  typecheck_failure("val arr: (int, bool) = 1;", "Value Define Type Mismatch: i32 to (i32, bool)");

  typecheck_failure("val arr: (int, bool, string) = (1, true);",
                    "Value Define Type Mismatch: (i32, bool) to (i32, bool, string)");
  typecheck_failure("val (a, b, c) = (1, false);", "Too many bindings in tuple unpack: 3 to 2");
  typecheck_failure("val (a: int, b: bool, c) = (false, false, 1);", "Value Binding Type Mismatch: bool to i32");
  typecheck_failure("val (a: bool, ...b: (string, int)) = (false, false, 1);",
                    "Value Binding Type Mismatch: (bool, i32) to (string, i32)");
  typecheck_failure("val (a: bool, ...b: (string, int)) = 1;", "Value Binding Type Mismatch: i32 to tuple");
  typecheck_failure("val bad = (1, 2)[0..3];", "Tuple slice bounds out of range");
  typecheck_failure("val bad: bool = (1, \"two\").0;", "Value Define Type Mismatch: i32 to bool");
  typecheck_failure("val bad = (1, \"two\").2;", "Tuple element index out of range");
}
