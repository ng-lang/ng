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
  REQUIRE(from_ng<int32_t>(to_ng(42)) == 42);
  REQUIRE(from_ng<bool>(to_ng(true)));
  REQUIRE(from_ng<Str>(to_ng("ng")) == "ng");
}

TEST_CASE("native bridge wraps function pointers", "[OrgasmTest][NativeBridge]")
{
  auto wrapped = wrap_native(&add_i32);
  auto result = wrapped({to_ng(20), to_ng(22)});

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 42);
}

TEST_CASE("native bridge wraps lambdas and void returns", "[OrgasmTest][NativeBridge]")
{
  auto greet = wrap_native([](Str name, bool excited) -> Str {
    return excited ? name + "!" : name;
  });
  auto greeting = std::dynamic_pointer_cast<NGString>(greet({to_ng("ng"), to_ng(true)}));
  REQUIRE(greeting != nullptr);
  REQUIRE(greeting->value == "ng!");

  bool called = false;
  auto mark = wrap_native([&called]() { called = true; });
  auto unit = mark({});
  REQUIRE(called);
  REQUIRE(std::dynamic_pointer_cast<NGUnit>(unit) != nullptr);
}
