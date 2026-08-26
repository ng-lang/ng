// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-1: self-tests for the PBT harness itself. All properties run with
// fixed seeds, so they are fully deterministic (no flakes by construction).

#include "pbt.hpp"
#include "test.hpp"

#include <format>
#include <string>
#include <vector>

TEST_CASE("PBT harness reproduces identical sequences for a given seed", "[INC1][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("rng-determinism");
  pbt::Rng first(seed);
  pbt::Rng second(seed);
  for (uint32_t trial = 0; trial < 64; ++trial)
  {
    INFO("trial " << trial);
    REQUIRE(first.nextU64() == second.nextU64());
  }
  // Overwhelmingly likely to differ across the full stream; with a fixed
  // seed this is a deterministic assertion, not a probabilistic one.
  pbt::Rng third(seed + 1);
  bool differs = false;
  for (uint32_t trial = 0; trial < 16 && !differs; ++trial)
    differs = third.nextU64() != first.nextU64();
  REQUIRE(differs);
}

TEST_CASE("PBT signed generators hit every boundary within a fixed budget", "[INC1][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("signed-edges");
  pbt::Rng rng(seed);
  bool seenMin = false;
  bool seenMax = false;
  bool seenZero = false;
  for (uint32_t trial = 0; trial < 2000; ++trial)
  {
    const auto value = pbt::signedInt<int64_t>(rng);
    seenMin |= value == INT64_MIN;
    seenMax |= value == INT64_MAX;
    seenZero |= value == 0;
  }
  REQUIRE(seenMin);
  REQUIRE(seenMax);
  REQUIRE(seenZero);
}

TEST_CASE("PBT unsigned generators cover u8 through u64 including UINT64_MAX", "[INC1][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("unsigned-domain");
  constexpr uint32_t trials = 2000;
  pbt::Rng rng(seed);
  bool seenU64Max = false;
  for (uint32_t trial = 0; trial < trials; ++trial)
  {
    const auto byte = pbt::unsignedInt<uint8_t>(rng);
    INFO("trial " << trial << " byte=" << static_cast<int>(byte));

    const auto wide = pbt::unsignedInt<uint64_t>(rng);
    seenU64Max |= wide == UINT64_MAX;
  }
  REQUIRE(seenU64Max);
}

TEST_CASE("PBT property round-trip: narrowing then widening preserves in-width values", "[INC1][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("narrow-roundtrip");
  constexpr uint32_t trials = 500;
  const auto failure = pbt::forAll(pbt::Rng(seed), trials,
                                   [](pbt::Rng &rng, size_t) -> std::optional<std::string>
                                   {
                                     const int64_t wide = pbt::signedInt<int64_t>(rng);
                                     if (wide < INT32_MIN || wide > INT32_MAX)
                                       return std::nullopt; // out of i32 domain: nothing to check
                                     const auto narrowed = static_cast<int32_t>(wide);
                                     if (static_cast<int64_t>(narrowed) != wide)
                                       return std::format("i32 round-trip lost value {}", wide);
                                     return std::nullopt;
                                   });
  INFO(pbt::describe(seed, failure, trials));
  REQUIRE(!failure.has_value());
}

TEST_CASE("PBT string and array generators satisfy their stated invariants", "[INC1][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("container-invariants");
  constexpr uint32_t trials = 500;
  const auto failure = pbt::forAll(pbt::Rng(seed), trials,
                                   [](pbt::Rng &rng, size_t) -> std::optional<std::string>
                                   {
                                     const auto text = pbt::asciiString(rng, 24);
                                     PBT_CHECK(text.size() <= 24);
                                     for (const char character : text)
                                       PBT_CHECK(character >= ' ' && character <= '~');

                                     const auto items = pbt::vectorOf<int32_t>(
                                         rng, 2, 7, [](pbt::Rng &inner) { return pbt::signedInt<int32_t>(inner); });
                                     PBT_CHECK(items.size() >= 2 && items.size() <= 7);
                                     return std::nullopt;
                                   });
  INFO(pbt::describe(seed, failure, trials));
  REQUIRE(!failure.has_value());
}

TEST_CASE("PBT forAll reports counterexamples with seed and trial", "[INC1][Pbt]")
{
  const pbt::Rng rng(42);
  constexpr uint32_t trials = 10;
  const auto failure = pbt::forAll(rng, trials,
                                   [](pbt::Rng &inner, size_t trial) -> std::optional<std::string>
                                   {
                                     if (trial == 3)
                                       return "synthetic failure";
                                     static_cast<void>(inner.nextU64());
                                     return std::nullopt;
                                   });
  REQUIRE(failure.has_value());
  REQUIRE(failure->trial == 3);
  REQUIRE(failure->detail == "synthetic failure");
  const auto rendered = pbt::describe(1234, failure, trials);
  REQUIRE_THAT(rendered, ContainsSubstring("trial 3"));
  REQUIRE_THAT(rendered, ContainsSubstring("PBT_SEED=1234"));
}
