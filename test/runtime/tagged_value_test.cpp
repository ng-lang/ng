#include "../test.hpp"
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/tagged_layout_access.hpp>
#include <runtime/value_access.hpp>

using namespace NG::runtime;

TEST_CASE("tagged storage cells expose payload members and metadata", "[RuntimeTest][TaggedValue]")
{
  auto selfSlot = make_runtime_tagged_cell(
      "Result", "Ok", 0,
      {
          numeral_cell_from_value<int32_t>(42),
      },
      {"value"});

  auto env = make_runtime_env();
  NGArgs args{};

  REQUIRE(runtime_value_show(selfSlot) == "Ok(42)");

  auto typeA = runtime_value_type(selfSlot);
  auto typeB = runtime_value_type(selfSlot);
  REQUIRE(typeA != nullptr);
  REQUIRE(typeB != nullptr);
  REQUIRE(typeA->name == "Result");
  REQUIRE(*typeA == *typeB);

  auto value = runtime_value_respond(selfSlot, "value", env, args);
  REQUIRE(value != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(value) == 42);

  auto positional = runtime_value_respond(selfSlot, "0", env, args);
  REQUIRE(positional != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(positional) == 42);

  auto tag = runtime_value_respond(selfSlot, "tag", env, args);
  REQUIRE(tag != nullptr);
  REQUIRE(runtime_string_value(tag) == "Ok");

  auto index = runtime_value_respond(selfSlot, "index", env, args);
  REQUIRE(index != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(index) == 0);

  auto valueSlot = runtime_cell_slot_ref(selfSlot, 0);
  REQUIRE(valueSlot != nullptr);
  REQUIRE(tagged_member_slot(selfSlot, "value") == valueSlot);
  runtime_copy_storage_cell(valueSlot, numeral_cell_from_value<int32_t>(99));

  auto slotted = runtime_value_respond(selfSlot, "value", env, args);
  REQUIRE(slotted != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(slotted) == 99);
}

TEST_CASE("tagged storage cells reject unknown members", "[RuntimeTest][TaggedValue]")
{
  auto selfSlot = make_runtime_tagged_cell("Result", "Err", 1, {}, {});
  auto env = make_runtime_env();
  NGArgs args{};

  REQUIRE_THROWS_AS(runtime_value_respond(selfSlot, "missing", env, args), NotImplementedException);
}
