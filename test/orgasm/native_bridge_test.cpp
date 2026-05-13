#include "../test.hpp"
#include <orgasm/native_bridge.hpp>

using namespace NG::orgasm;
using namespace NG::runtime;

namespace
{
  auto add_i32(int32_t left, int32_t right) -> int32_t
  {
    return left + right;
  }
} // namespace

TEST_CASE("native bridge converts primitive values", "[OrgasmTest][NativeBridge]")
{
  REQUIRE(from_ng<int8_t>(to_ng(static_cast<int8_t>(-8))) == -8);
  REQUIRE(from_ng<int16_t>(to_ng(static_cast<int16_t>(-16))) == -16);
  REQUIRE(from_ng<int32_t>(to_ng(42)) == 42);
  REQUIRE(from_ng<int64_t>(to_ng(static_cast<int64_t>(64))) == 64);
  REQUIRE(from_ng<uint8_t>(to_ng(static_cast<uint8_t>(8))) == 8);
  REQUIRE(from_ng<uint16_t>(to_ng(static_cast<uint16_t>(16))) == 16);
  REQUIRE(from_ng<uint32_t>(to_ng(static_cast<uint32_t>(32))) == 32);
  REQUIRE(from_ng<uint64_t>(to_ng(static_cast<uint64_t>(64))) == 64);
  REQUIRE(from_ng<float>(to_ng(1.5F)) == 1.5F);
  REQUIRE(from_ng<double>(to_ng(2.5)) == 2.5);
  REQUIRE(from_ng<bool>(to_ng(true)));
  REQUIRE(from_ng<Str>(to_ng("ng")) == "ng");

  Str moved = "moved";
  REQUIRE(runtime_string_value(to_ng(std::move(moved))) == "moved");
  auto cell = numeral_cell_from_value<int32_t>(7);
  REQUIRE(from_ng<RuntimeRef<StorageCell>>(to_ng(cell)) == cell);
}

TEST_CASE("native bridge wraps function pointers", "[OrgasmTest][NativeBridge]")
{
  auto wrapped = wrap_native(&add_i32);
  auto result = wrapped({to_ng(20), to_ng(22)});

  REQUIRE(read_numeric_cell_as<int32_t>(result) == 42);
}

TEST_CASE("native bridge wraps lambdas and void returns", "[OrgasmTest][NativeBridge]")
{
  auto greet = wrap_native([](Str name, bool excited) -> Str { return excited ? name + "!" : name; });
  auto greeting = greet({to_ng("ng"), to_ng(true)});
  REQUIRE(runtime_string_value(greeting) == "ng!");

  bool called = false;
  auto mark = wrap_native([&called]() { called = true; });
  auto unit = mark({});
  REQUIRE(called);
  REQUIRE(runtime_value_type(unit)->name == "unit");
}
