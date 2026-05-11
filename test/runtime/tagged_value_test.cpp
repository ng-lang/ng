#include "../test.hpp"
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>

using namespace NG::runtime;

TEST_CASE("NGTaggedValue exposes payload members and metadata", "[RuntimeTest][TaggedValue]")
{
  NGTaggedValue tagged{
      "Result",
      "Ok",
      0,
      Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(42)},
      {"value"},
  };

  auto ctx = makert<NGContext>();
  auto self = makert<NGUnit>();
  NGArgs args{};

  REQUIRE(tagged.show() == "Ok(42)");

  auto typeA = tagged.type();
  auto typeB = tagged.type();
  REQUIRE(typeA != nullptr);
  REQUIRE(typeB != nullptr);
  REQUIRE(typeA->name == "Result");
  REQUIRE(*typeA == *typeB);

  auto value = std::dynamic_pointer_cast<NumeralBase>(tagged.respond(self, "value", ctx, args));
  REQUIRE(value != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(value.get()) == 42);

  auto positional = std::dynamic_pointer_cast<NumeralBase>(tagged.respond(self, "0", ctx, args));
  REQUIRE(positional != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(positional.get()) == 42);

  auto tag = std::dynamic_pointer_cast<NGString>(tagged.respond(self, "tag", ctx, args));
  REQUIRE(tag != nullptr);
  REQUIRE(tag->payload_value() == "Ok");

  auto index = std::dynamic_pointer_cast<NumeralBase>(tagged.respond(self, "index", ctx, args));
  REQUIRE(index != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(index.get()) == 0);

  REQUIRE(tagged.payload_items().size() == 1);
  REQUIRE(tagged.payload_store().get(tagged.payload_cell()).layout.name == "Tagged.payload");

  auto valueSlot = tagged.payload_slot(0);
  REQUIRE(valueSlot != nullptr);
  REQUIRE(std::static_pointer_cast<StorageCell>(tagged.payload_store().get(tagged.payload_cell()).opaqueRefs[0]) == valueSlot);
  runtime_sync_storage_cell(valueSlot, makert<NGIntegral<int32_t>>(99));

  auto slotted = std::dynamic_pointer_cast<NumeralBase>(tagged.respond(self, "value", ctx, args));
  REQUIRE(slotted != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(slotted.get()) == 99);
  REQUIRE(NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(tagged.payload_items()[0]).get()) == 99);
}

TEST_CASE("NGTaggedValue falls back to NGObject for unknown members", "[RuntimeTest][TaggedValue]")
{
  NGTaggedValue tagged{"Result", "Err", 1, {}, {}};
  auto ctx = makert<NGContext>();
  auto self = makert<NGUnit>();
  NGArgs args{};

  REQUIRE_THROWS_AS(tagged.respond(self, "missing", ctx, args), NotImplementedException);
}
