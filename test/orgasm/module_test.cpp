#include "../test.hpp"
#include <orgasm/module.hpp>

using namespace NG::orgasm;

namespace
{
auto read_u16(const Vec<uint8_t> &code, size_t offset) -> uint16_t
{
  return static_cast<uint16_t>(code[offset]) | (static_cast<uint16_t>(code[offset + 1]) << 8);
}
} // namespace

TEST_CASE("bytecode module merge remaps tagged union type operands", "[OrgasmTest][Module]")
{
  BytecodeModule base;
  base.types.push_back(Type{.name = "Base"});

  BytecodeModule other;
  other.types.push_back(Type{.name = "Result", .variants = Vec<Variant>{{.name = "Ok"}}});

  Function fn;
  fn.name = "make_result";
  fn.code = {
      static_cast<uint8_t>(OpCode::CONSTRUCT_TAGGED),
      0, 0, // type index
      0, 0, // variant index
      0, 0, // payload count
      static_cast<uint8_t>(OpCode::RETURN),
  };
  other.functions.push_back(std::move(fn));

  base.merge(other);

  REQUIRE(base.types.size() == 2);
  REQUIRE(base.functions.size() == 1);
  REQUIRE(read_u16(base.functions[0].code, 1) == 1);
  REQUIRE(read_u16(base.functions[0].code, 3) == 0);
  REQUIRE(read_u16(base.functions[0].code, 5) == 0);
}
