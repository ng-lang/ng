// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/value.hpp"

namespace vnext = NG::vnext;

TEST_CASE("vNext tagged values preserve scalar and string alternatives", "[vNext][Value]")
{
  const auto integer = vnext::Value::integer(42);
  REQUIRE(integer.isInteger());
  REQUIRE_FALSE(integer.isString());
  REQUIRE(integer.asInteger() == 42);
  REQUIRE(integer == 42);

  const auto string = vnext::Value::string("hello");
  REQUIRE_FALSE(string.isInteger());
  REQUIRE(string.isString());
  REQUIRE(string.asString() == "hello");
  REQUIRE(string == vnext::Value::string("hello"));
  REQUIRE_FALSE(string == vnext::Value::string("goodbye"));
}

TEST_CASE("vNext tagged values distinguish arrays and tuples", "[vNext][Value]")
{
  const auto array = vnext::Value::array({vnext::Value::integer(1)});
  const auto tuple = vnext::Value::tuple({vnext::Value::integer(1), vnext::Value::string("two")});
  REQUIRE(array.isArray());
  REQUIRE_FALSE(array.isTuple());
  REQUIRE(tuple.isTuple());
  REQUIRE_FALSE(tuple.isArray());
  REQUIRE(tuple.asTuple().size() == 2);
  REQUIRE(tuple.asTuple()[1].asString() == "two");
}

TEST_CASE("vNext tagged values reject invalid alternative access", "[vNext][Value]")
{
  REQUIRE_THROWS_WITH(vnext::Value::integer(1).asString(), "runtime value is not a string");
  REQUIRE_THROWS_WITH(vnext::Value::string("one").asInteger(), "runtime value is not an i64");
}
