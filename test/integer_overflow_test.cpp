// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-5 (F1③): checked arithmetic follows operand signedness — u64 wrap
// attempts trap instead of passing silently, and every other width keeps
// trapping exactly when the host reference says "overflow".

#include "pbt.hpp"
#include "test.hpp"

#include "driver.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
  enum class Op
  {
    Add,
    Sub,
    Mul,
    Div,
    Rem
  };

  [[nodiscard]] auto opText(Op op) -> const char *
  {
    switch (op)
    {
    case Op::Add: return "+";
    case Op::Sub: return "-";
    case Op::Mul: return "*";
    case Op::Div: return "/";
    default: return "%";
    }
  }

  /// Host reference: does `left op right` overflow/trap at this width?
  /// Overflow checks widen into 128-bit space.
  [[nodiscard]] auto referenceTraps(const pbt::IntegerWidth &width, Op op, uint64_t rawLeft, uint64_t rawRight) -> bool
  {
    if (op == Op::Div || op == Op::Rem)
      return rawRight == 0 || (width.isSigned && rawLeft == static_cast<uint64_t>(width.min) &&
                               rawRight == static_cast<uint64_t>(UINT64_MAX));
    const __int128 wideLeft = width.isSigned ? static_cast<__int128>(static_cast<int64_t>(rawLeft))
                                             : static_cast<__int128>(rawLeft);
    const __int128 wideRight = width.isSigned ? static_cast<__int128>(static_cast<int64_t>(rawRight))
                                              : static_cast<__int128>(rawRight);
    const __int128 result = op == Op::Add   ? wideLeft + wideRight
                            : op == Op::Sub ? wideLeft - wideRight
                                            : wideLeft * wideRight;
    return result < static_cast<__int128>(width.min) || result > static_cast<__int128>(width.max);
  }

  /// Host reference: exact in-width result of a non-trapping operation.
  [[nodiscard]] auto referenceResult(const pbt::IntegerWidth &width, Op op, uint64_t rawLeft,
                                     uint64_t rawRight) -> int64_t
  {
    if (width.isSigned)
    {
      const auto left = static_cast<int64_t>(rawLeft);
      const auto right = static_cast<int64_t>(rawRight);
      switch (op)
      {
      case Op::Add: return static_cast<int64_t>(static_cast<uint64_t>(left) + static_cast<uint64_t>(right));
      case Op::Sub: return static_cast<int64_t>(static_cast<uint64_t>(left) - static_cast<uint64_t>(right));
      case Op::Mul: return static_cast<int64_t>(static_cast<uint64_t>(left) * static_cast<uint64_t>(right));
      case Op::Div: return left / right;
      default: return left % right;
      }
    }
    switch (op)
    {
    case Op::Add: return static_cast<int64_t>(rawLeft + rawRight);
    case Op::Sub: return static_cast<int64_t>(rawLeft - rawRight);
    case Op::Mul: return static_cast<int64_t>(rawLeft * rawRight);
    case Op::Div: return static_cast<int64_t>(rawLeft / rawRight);
    default: return static_cast<int64_t>(rawLeft % rawRight);
    }
  }
} // namespace

TEST_CASE("INC-5 arithmetic matches host reference and never wraps silently", "[INC5][Numeric][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("overflow");
  constexpr size_t triplesPerWidth = 14;
  pbt::Rng rng(seed);

  std::vector<pbt::BatchProbe> probes;
  while (probes.size() < triplesPerWidth * pbt::integerWidths().size())
  {
    for (const auto &width : pbt::integerWidths())
    {
      if (probes.size() >= triplesPerWidth * pbt::integerWidths().size())
        break;
      const auto op = static_cast<Op>(rng.nextU64InRange(0, 4));
      const auto rawLeft = pbt::drawOperand(rng, width);
      const auto rawRight = pbt::drawOperand(rng, width);
      if (referenceTraps(width, op, rawLeft, rawRight))
        continue; // trap cases are exercised by dedicated probes below

      // Compare within the width via an annotated binding: bare literals in
      // comparisons default to i64 and reject full-range unsigned values.
      // A silent wrap shows up as a false boolean rather than a wrong value.
      const auto index = probes.size();
      probes.push_back(pbt::BatchProbe{
          .source = std::format("    let l{0}: {4} = {1};\n    let r{0}: {4} = {2};\n"
                                "    let e{0}: {4} = {3};\n    print(l{0} {5} r{0} == e{0});",
                                index, pbt::widthLiteral(width, rawLeft), pbt::widthLiteral(width, rawRight),
                                pbt::widthLiteral(width, static_cast<uint64_t>(
                                                             referenceResult(width, op, rawLeft, rawRight))),
                                width.name, opText(op)),
          .expect = "true",
          .label = std::format("{} {} {} == {}", pbt::widthLiteral(width, rawLeft), opText(op),
                               pbt::widthLiteral(width, rawRight),
                               pbt::widthLiteral(width, static_cast<uint64_t>(
                                                            referenceResult(width, op, rawLeft, rawRight)))),
      });
    }
  }

  const auto failure = pbt::runBatchProbes("inc5-arith", "native fun print(value: bool) -> unit;", probes);
  REQUIRE(!failure.has_value());
}

namespace
{
  /// Runs one program expected to hit an arithmetic trap. The trap must fire
  /// while evaluating the binding, before main completes.
  void requireTrap(std::string_view body)
  {
    std::string source = "fun main() -> unit { ";
    source += body;
    source += " }\n";
    std::ostringstream output;
    std::ostringstream errors;
    const int status = NG::runDriver({"--source", source}, output, errors);
    INFO("source: " << source << "\nerrors: " << errors.str());
    const auto errorText = errors.str();
    REQUIRE(status != 0);
    const bool trapped =
        errorText.find("killed by a signal") != std::string::npos ||
        errorText.find("exited with code") != std::string::npos;
    REQUIRE(trapped);
  }
} // namespace

TEST_CASE("INC-5 u64 wrap attempts trap instead of passing silently", "[INC5][Numeric]")
{
  // The research probe: u64(2^64 - 8) + 20 used to yield 12 silently.
  requireTrap("let big: u64 = 18446744073709551615; let wrapped = big + 20;");
  requireTrap("let a: u64 = 18446744073709551615; let b: u64 = 20; let c = a + b;");
  requireTrap("let a: u64 = 5; let b: u64 = 7; let c = a - b;");
  requireTrap("let a: u64 = 9223372036854775808; let b: u64 = 3; let c = a * b;");
}

TEST_CASE("INC-5 every width traps exactly at its overflow boundaries", "[INC5][Numeric]")
{
  requireTrap("let a: i64 = 9223372036854775807; let b = a + 1;");
  requireTrap("let a: i64 = -9223372036854775807; let b = a - 2;");
  requireTrap("let a: i32 = 2147483647; let b = a + 1;");
  requireTrap("let a: i16 = 32767; let b = a + 1;");
  requireTrap("let a: i8 = 127; let b: i8 = 1; let c = a + b;");
  requireTrap("let a: u32 = 4294967295; let b = a + 1;");
  requireTrap("let a: u16 = 65535; let b = a + 1;");
  requireTrap("let a: u8 = 255; let b: u8 = 1; let c = a + b;");
  requireTrap("let a: u8 = 200; let b: u8 = 100; let c = a + b;");
  requireTrap("let a: u8 = 30; let b: u8 = 9; let c = a * b;");
  requireTrap("let a: i8 = -100; let b: i8 = 100; let c = a - b;");
}

TEST_CASE("INC-5 lowering selects unsigned checked helpers for u64", "[INC5][Numeric][Qbe]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({"--emit=ssa", "--source",
                                    "fun probe(a: u64, b: u64) -> u64 => a + b;"
                                    "fun main() -> u64 => probe(9, 5);"},
                                   output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  REQUIRE_THAT(output.str(), ContainsSubstring("$ngrt_checked_uadd"));
}
