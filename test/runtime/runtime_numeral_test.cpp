#include "../test.hpp"
#include <intp/runtime_numerals.hpp>
#include <runtime/value_ops.hpp>

using namespace NG::runtime;
using namespace NG::runtime::ops;

TEST_CASE("buffered numeral cells store inline bytes without object cache", "[Numeral][Runtime][Buffered]")
{
  auto left = numeral_cell_from_value<int32_t>(4);
  auto right = numeral_cell_from_value<int32_t>(2);

  REQUIRE(left->runtimeType != nullptr);
  REQUIRE(left->runtimeType->name == "i32");
  REQUIRE(read_inline_cell_bytes<int32_t>(left) == 4);
  REQUIRE(runtime_value_show(left) == "4");
  REQUIRE(runtime_value_bool(left));
  REQUIRE(value_greater_than(left, right));

  auto summed = value_add(left, right);
  REQUIRE(read_inline_cell_bytes<int32_t>(summed) == 6);
}

TEST_CASE("buffered numeral cells reject division and modulus by zero", "[Numeral][Runtime][Failure]")
{
  auto value = numeral_cell_from_value<int32_t>(1);
  auto zero = numeral_cell_from_value<int32_t>(0);

  REQUIRE_THROWS_MATCHES(value_divide(value, zero), NG::RuntimeException,
                         MessageMatches(ContainsSubstring("Division by zero")));
  REQUIRE_THROWS_MATCHES(value_modulus(value, zero), NG::RuntimeException,
                         MessageMatches(ContainsSubstring("Modulus by zero")));
}

TEST_CASE("buffered numeral cells compare floats", "[Numeral][Runtime]")
{
  auto left = numeral_cell_from_value<float>(1.0F);
  auto right = numeral_cell_from_value<double>(2.0);

  REQUIRE(!value_greater_than(left, right));
  REQUIRE(!value_less_than(right, left));
}

TEST_CASE("buffered numeral cells negate", "[Numeral][Runtime]")
{
  auto value = numeral_cell_from_value<float>(-3.5F);
  auto negated = negate_numeric_cell(value);
  REQUIRE(read_inline_cell_bytes<float>(negated) == 3.5F);

  REQUIRE_THROWS_MATCHES(negate_numeric_cell(numeral_cell_from_value<unsigned int>(42U)), RuntimeException,
                         MessageMatches(ContainsSubstring("Cannot negate unsigned integers")));
}

TEST_CASE("buffered bool cells use inline slot handlers", "[Numeral][Runtime][Buffered]")
{
  auto truthy = make_runtime_boolean(true);
  auto falsy = make_runtime_boolean(false);

  REQUIRE(runtime_value_show(truthy) == "true");
  REQUIRE(runtime_value_bool(truthy));
  REQUIRE_FALSE(runtime_value_bool(falsy));
  REQUIRE(value_greater_than(truthy, falsy));
}
