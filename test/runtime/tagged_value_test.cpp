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

TEST_CASE("tagged storage cell helpers expose metadata and reject non-tagged values", "[RuntimeTest][TaggedValue][Failure]")
{
  auto tagged = make_runtime_tagged_cell(
      "Result", "Err", 2,
      {
          make_runtime_string("boom"),
          numeral_cell_from_value<int32_t>(500),
      },
      {"message", "code"});

  REQUIRE(runtime_is_tagged_value(tagged));
  REQUIRE(runtime_tagged_type(tagged)->name == "Result");
  REQUIRE(runtime_tagged_union_name(tagged) == "Result");
  REQUIRE(runtime_tagged_variant_name(tagged) == "Err");
  REQUIRE(runtime_tagged_variant_index(tagged) == 2);
  REQUIRE(runtime_tagged_payload_names(tagged) == Vec<Str>{"message", "code"});
  REQUIRE(runtime_tagged_slots(tagged).size() == 2);
  REQUIRE(runtime_tagged_slot(tagged, 1) == runtime_cell_slot_ref(tagged, 1));
  REQUIRE(runtime_tagged_payload_index(tagged, "message").value() == 0);
  REQUIRE(runtime_tagged_payload_index(tagged, "1").value() == 1);
  REQUIRE(runtime_tagged_payload_index(tagged, "missing") == std::nullopt);
  REQUIRE(runtime_string_value(runtime_tagged_read_member(tagged, "message")) == "boom");
  REQUIRE(read_inline_cell_bytes<int32_t>(runtime_tagged_read_member(tagged, "index")) == 2);

  auto plain = make_runtime_string("plain");
  REQUIRE_FALSE(runtime_is_tagged_value(plain));
  REQUIRE_THROWS_WITH(runtime_tagged_type(plain), Catch::Matchers::ContainsSubstring("Expected tagged runtime value"));
  REQUIRE_THROWS_WITH(runtime_tagged_slots(plain), Catch::Matchers::ContainsSubstring("Expected tagged runtime value"));
  REQUIRE_THROWS_WITH(runtime_tagged_slot(plain, 0), Catch::Matchers::ContainsSubstring("Expected tagged runtime value"));
  REQUIRE_THROWS_WITH(runtime_tagged_payload_index(plain, "0"),
                      Catch::Matchers::ContainsSubstring("Expected tagged runtime value"));
}

TEST_CASE("tagged payload names use the active variant layout", "[RuntimeTest][TaggedValue]")
{
  TypeLayout layout{.name = "Result", .kind = LayoutKind::TAGGED_UNION};
  layout.variants.push_back(VariantLayout{.name = "Ok", .fields = {FieldLayout{.name = "value"}}});
  layout.variants.push_back(VariantLayout{.name = "Err", .fields = {FieldLayout{.name = "message"},
                                                                     FieldLayout{.name = "code"}}});

  auto errType = makert<NGType>(NGType{
      .name = "Result",
      .layout = layout,
      .variantName = "Err",
      .variantIndex = 1,
  });
  auto err = make_runtime_tagged_cell(errType, {make_runtime_string("boom"), numeral_cell_from_value<int32_t>(500)});

  REQUIRE(runtime_tagged_payload_names(err) == Vec<Str>{"message", "code"});
}
