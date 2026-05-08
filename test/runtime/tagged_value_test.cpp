#include "../test.hpp"
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

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
  auto inv = makert<NGInvocationContext>();
  inv->target = makert<NGUnit>();

  REQUIRE(tagged.show() == "Ok(42)");

  auto typeA = tagged.type();
  auto typeB = tagged.type();
  REQUIRE(typeA == typeB);
  REQUIRE(typeA->name == "Result");

  auto value = std::dynamic_pointer_cast<NumeralBase>(tagged.respond("value", ctx, inv));
  REQUIRE(value != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(value.get()) == 42);

  auto tag = std::dynamic_pointer_cast<NGString>(tagged.respond("tag", ctx, inv));
  REQUIRE(tag != nullptr);
  REQUIRE(tag->value == "Ok");

  auto index = std::dynamic_pointer_cast<NumeralBase>(tagged.respond("index", ctx, inv));
  REQUIRE(index != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(index.get()) == 0);
}

TEST_CASE("NGTaggedValue falls back to NGObject for unknown members", "[RuntimeTest][TaggedValue]")
{
  NGTaggedValue tagged{"Result", "Err", 1, {}, {}};
  auto ctx = makert<NGContext>();
  auto inv = makert<NGInvocationContext>();
  inv->target = makert<NGUnit>();

  REQUIRE_THROWS_AS(tagged.respond("missing", ctx, inv), NotImplementedException);
}
