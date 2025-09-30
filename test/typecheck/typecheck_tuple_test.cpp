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
}