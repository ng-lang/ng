#include "../test.hpp"
#include <intp/runtime_numerals.hpp>
#include <runtime/value_ops.hpp>

#include <limits>

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

TEST_CASE("buffered numeral cells cover supported inline widths and invalid reads", "[Numeral][Runtime][Buffered][Failure]")
{
  REQUIRE(numeral_type_name<int8_t>() == "i8");
  REQUIRE(numeral_type_name<uint8_t>() == "u8");
  REQUIRE(numeral_type_name<int16_t>() == "i16");
  REQUIRE(numeral_type_name<uint16_t>() == "u16");
  REQUIRE(numeral_type_name<int64_t>() == "i64");
  REQUIRE(numeral_type_name<uint64_t>() == "u64");
  REQUIRE(numeral_type_name<double>() == "f64");

  REQUIRE(read_numeric_cell_as<int64_t>(numeral_cell_from_value<int8_t>(-8)) == -8);
  REQUIRE(read_numeric_cell_as<uint64_t>(numeral_cell_from_value<uint8_t>(8)) == 8);
  REQUIRE(read_numeric_cell_as<int64_t>(numeral_cell_from_value<int16_t>(-16)) == -16);
  REQUIRE(read_numeric_cell_as<uint64_t>(numeral_cell_from_value<uint16_t>(16)) == 16);
  REQUIRE(read_numeric_cell_as<int64_t>(numeral_cell_from_value<int64_t>(-64)) == -64);
  REQUIRE(read_numeric_cell_as<uint64_t>(numeral_cell_from_value<uint64_t>(64)) == 64);
  REQUIRE(read_numeric_cell_as<int32_t>(numeral_cell_from_value<float>(3.5F)) == 3);
  REQUIRE(read_numeric_cell_as<int32_t>(numeral_cell_from_value<double>(4.5)) == 4);

  auto shortCell = make_storage_cell(numeral_type_layout<int32_t>());
  shortCell->bytes = {1, 2};
  REQUIRE_THROWS_MATCHES(read_inline_cell_bytes<int32_t>(shortCell), RuntimeException,
                         MessageMatches(ContainsSubstring("requested inline value")));
  REQUIRE_THROWS_MATCHES(read_inline_cell_bytes<int32_t>(nullptr), RuntimeException,
                         MessageMatches(ContainsSubstring("requested inline value")));
  REQUIRE_THROWS_MATCHES(write_inline_cell_bytes<int32_t>(nullptr, 1), RuntimeException,
                         MessageMatches(ContainsSubstring("Cannot write null storage cell")));
  REQUIRE_THROWS_MATCHES(read_numeric_cell_as<int32_t>(make_runtime_string("not-number")), RuntimeException,
                         MessageMatches(ContainsSubstring("Not a buffered numeral cell")));
}

TEST_CASE("buffered numeral cells compare floats", "[Numeral][Runtime]")
{
  auto left = numeral_cell_from_value<float>(1.0F);
  auto right = numeral_cell_from_value<double>(2.0);

  REQUIRE(!value_greater_than(left, right));
  REQUIRE(!value_less_than(right, left));

  auto nan = numeral_cell_from_value<float>(std::numeric_limits<float>::quiet_NaN());
  REQUIRE(value_order(nan, numeral_cell_from_value<float>(1.0F)) == Orders::UNORDERED);
  REQUIRE_THROWS_MATCHES(value_less_than(nan, numeral_cell_from_value<float>(1.0F)), RuntimeException,
                         MessageMatches(ContainsSubstring("Unsupported binary operator")));
}

TEST_CASE("buffered numeral cells negate", "[Numeral][Runtime]")
{
  auto value = numeral_cell_from_value<float>(-3.5F);
  auto negated = negate_numeric_cell(value);
  REQUIRE(read_inline_cell_bytes<float>(negated) == 3.5F);

  REQUIRE(read_inline_cell_bytes<int8_t>(negate_numeric_cell(numeral_cell_from_value<int8_t>(-3))) == 3);
  REQUIRE(read_inline_cell_bytes<int16_t>(negate_numeric_cell(numeral_cell_from_value<int16_t>(-4))) == 4);
  REQUIRE(read_inline_cell_bytes<int32_t>(negate_numeric_cell(numeral_cell_from_value<int32_t>(-5))) == 5);
  REQUIRE(read_inline_cell_bytes<int64_t>(negate_numeric_cell(numeral_cell_from_value<int64_t>(-6))) == 6);
  REQUIRE(read_inline_cell_bytes<double>(negate_numeric_cell(numeral_cell_from_value<double>(-7.5))) == 7.5);
  REQUIRE_THROWS_MATCHES(negate_numeric_cell(numeral_cell_from_value<unsigned int>(42U)), RuntimeException,
                         MessageMatches(ContainsSubstring("Cannot negate unsigned integers")));
  REQUIRE_THROWS_MATCHES(negate_numeric_cell(make_runtime_string("x")), RuntimeException,
                         MessageMatches(ContainsSubstring("Cannot negate a non-number")));
  REQUIRE_THROWS_AS(value_modulus(numeral_cell_from_value<float>(1.0F), numeral_cell_from_value<float>(1.0F)),
                    std::logic_error);
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
