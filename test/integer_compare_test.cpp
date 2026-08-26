// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-4 (F1①): ordering comparisons must follow operand signedness.
// Oracle: host C++ integer semantics on the same-width operands, per the
// plan's property "NG comparison == host reference semantics".

#include "pbt.hpp"
#include "test.hpp"

#include "driver.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
  struct WidthSpec
  {
    const char *name;
    bool isSigned;
    int64_t min;
    uint64_t max;
  };

  [[nodiscard]] auto widthSpecs()
  {
    return std::vector<WidthSpec>{
      {"i8", true, -128, 127},
      {"i16", true, -32768, 32767},
      {"i32", true, -2147483648LL, 2147483647ULL},
      {"i64", true, -9223372036854775807LL - 1, 9223372036854775807ULL},
      {"u8", false, 0, 255},
      {"u16", false, 0, 65535},
      {"u32", false, 0, 4294967295ULL},
      {"u64", false, 0, 18446744073709551615ULL},
    };
  }

  /// Boundary-weighted in-domain operand for one width. The signed minimum
  /// is excluded: its only spelling negates an overflowing literal (covered
  /// by the dedicated i64::min tests), and naive emission would produce
  /// invalid `--N` text.
  [[nodiscard]] auto drawOperand(pbt::Rng &rng, const WidthSpec &width) -> uint64_t
  {
    const auto roll = rng.nextU64InRange(0, 99);
    if (roll < 15)
      return width.max;
    if (roll < 25)
      return width.isSigned ? static_cast<uint64_t>(width.min) + 1 : static_cast<uint64_t>(width.min);
    if (!width.isSigned && roll < 35)
      return rng.nextU64InRange(0, std::min<uint64_t>(width.max, 8));
    if (roll < 45)
      return width.max / 2;
    if (roll < 55)
      return rng.below(0.5) ? 0 : 1;
    if (width.isSigned)
    {
      const auto span = static_cast<uint64_t>(width.max) - static_cast<uint64_t>(width.min);
      // Skip INT_MIN itself: its only spelling is a negated overflowing
      // literal, covered separately by dedicated tests.
      const auto offset = rng.nextU64InRange(1, span - 1);
      return static_cast<uint64_t>(width.min) + offset;
    }
    return rng.nextU64InRange(0, width.max);
  }

  [[nodiscard]] auto literalFor(const WidthSpec &width, uint64_t raw) -> std::string
  {
    if (!width.isSigned || static_cast<int64_t>(raw) >= 0)
      return std::to_string(raw);
    // A bare negative literal adopts the declared width; parenthesizing
    // would turn it into an i64 unary-minus expression.
    return "-" + std::to_string(-static_cast<int64_t>(raw));
  }

  template <typename T>
  [[nodiscard]] auto referenceCompare(uint64_t rawLeft, uint64_t rawRight, uint32_t payload) -> bool
  {
    const T left = static_cast<T>(rawLeft);
    const T right = static_cast<T>(rawRight);
    switch (payload)
    {
    case 8:
      return left < right;
    case 9:
      return left <= right;
    case 10:
      return left > right;
    default:
      return left >= right;
    }
  }
} // namespace

TEST_CASE("INC-4 NG ordering matches host reference semantics across widths", "[INC4][Numeric][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("ordering");
  constexpr uint32_t pairsPerWidth = 12;
  constexpr uint32_t kPayloads[] = {8, 9, 10, 11}; // < <= > >=

  struct Statement
  {
    bool expected;
    std::string source;
  };
  std::vector<Statement> statements;
  pbt::Rng rng(seed);

  for (const auto &width : widthSpecs())
    for (uint32_t pair = 0; pair < pairsPerWidth; ++pair)
    {
      const auto rawLeft = drawOperand(rng, width);
      const auto rawRight = drawOperand(rng, width);
      const auto payload = kPayloads[rng.nextU64InRange(0, 3)];
      const auto opText = payload == 8 ? "<" : payload == 9 ? "<=" : payload == 10 ? ">" : ">=";
      const auto index = statements.size();

      const auto expected = width.isSigned ? referenceCompare<int64_t>(rawLeft, rawRight, payload)
                                           : referenceCompare<uint64_t>(rawLeft, rawRight, payload);
      statements.push_back(
          Statement{expected, std::format("    let a{}: {} = {};\n    let b{}: {} = {};\n    print(a{} {} b{});", index,
                                          width.name, literalFor(width, rawLeft), index, width.name,
                                          literalFor(width, rawRight), index, opText, index)});
    }

  std::string source = "native fun print(value: bool) -> unit;\nfun main() -> unit {\n";
  for (const auto &statement : statements)
    source += statement.source + "\n";
  source += "}\n";

  const auto modulePath = std::filesystem::temp_directory_path() / "ng_inc4_ordering.ng";
  {
    std::ofstream stream(modulePath);
    stream << source;
  }
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({modulePath.string()}, output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);

  size_t index = 0;
  size_t mismatches = 0;
  size_t begin = 0;
  auto lineText = output.str();
  while (begin <= lineText.size() && index < statements.size())
  {
    const auto end = lineText.find('\n', begin);
    if (end == std::string::npos)
      break;
    const auto line = lineText.substr(begin, end - begin);
    begin = end + 1;
    if (line.empty() || line.starts_with("compiled ") || line.starts_with("native main exited"))
      continue;
    const auto actual = line == "true";
    if (actual != statements[index].expected)
    {
      WARN("mismatch at statement " << index << ": got " << line << ", want "
                                    << (statements[index].expected ? "true" : "false"));
      ++mismatches;
    }
    ++index;
  }
  CHECK(index == statements.size());
  CHECK(mismatches == 0);
}

TEST_CASE("INC-4 probe: u64 maximum compares greater than five", "[INC4][Numeric]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status =
      NG::runDriver({"--source", "native fun print(value: bool) -> unit; "
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
