// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-4 (F1①): ordering comparisons must follow operand signedness.
// Oracle: host C++ integer semantics on the same-width operands, per the
// plan's property "NG comparison == host reference semantics".

#include "pbt.hpp"
#include "test.hpp"

#include "driver.hpp"

#include <string>
#include <vector>

namespace
{
  template <typename T>
  [[nodiscard]] auto referenceCompare(uint64_t rawLeft, uint64_t rawRight, uint32_t payload) -> bool
  {
    const T left = static_cast<T>(rawLeft);
    const T right = static_cast<T>(rawRight);
    switch (payload)
    {
    case 8: return left < right;
    case 9: return left <= right;
    case 10: return left > right;
    default: return left >= right;
    }
  }
} // namespace

TEST_CASE("INC-4 NG ordering matches host reference semantics across widths", "[INC4][Numeric][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("ordering");
  constexpr uint32_t pairsPerWidth = 12;
  constexpr uint32_t kPayloads[] = {8, 9, 10, 11}; // < <= > >=

  std::vector<pbt::BatchProbe> probes;
  pbt::Rng rng(seed);

  for (const auto &width : pbt::integerWidths())
    for (uint32_t pair = 0; pair < pairsPerWidth; ++pair)
    {
      const auto rawLeft = pbt::drawOperand(rng, width);
      const auto rawRight = pbt::drawOperand(rng, width);
      const auto payload = kPayloads[rng.nextU64InRange(0, 3)];
      const auto opText = payload == 8 ? "<" : payload == 9 ? "<=" : payload == 10 ? ">" : ">=";

      const bool expected = width.isSigned ? referenceCompare<int64_t>(rawLeft, rawRight, payload)
                                           : referenceCompare<uint64_t>(rawLeft, rawRight, payload);
      const auto index = probes.size();
      const auto leftLiteral = pbt::widthLiteral(width, rawLeft);
      const auto rightLiteral = pbt::widthLiteral(width, rawRight);
      probes.push_back(pbt::BatchProbe{
          .source = std::format("    let a{0}: {1} = {2};\n    let b{0}: {1} = {3};\n    print(a{0} {4} b{0});",
                                index, width.name, leftLiteral, rightLiteral, opText),
          .expect = expected ? "true" : "false",
          .label = std::format("{} {} {}", leftLiteral, opText, rightLiteral),
      });
    }

  const auto failure = pbt::runBatchProbes("inc4-ordering", "native fun print(value: bool) -> unit;", probes);
  REQUIRE(!failure.has_value());
}

TEST_CASE("INC-4 probe: u64 maximum compares greater than five", "[INC4][Numeric]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver(
      {"--source",
       "native fun print(value: bool) -> unit; "
       "fun main() -> unit { let big: u64 = 18446744073709551615; print(big > 5); }"},
      output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  REQUIRE_THAT(output.str(), ContainsSubstring("true"));
}

TEST_CASE("INC-4 lowering emits unsigned QBE comparisons for u64 operands", "[INC4][Numeric][Qbe]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({"--emit=ssa", "--source",
                                    "fun probe(a: u64, b: u64) -> bool => a > b;"
                                    "fun main() -> bool => probe(9, 5);"},
                                   output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  REQUIRE_THAT(output.str(), ContainsSubstring("cugtl"));
}
