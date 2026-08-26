// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-5 (F1③): checked arithmetic follows operand signedness — u64 wrap
// attempts trap instead of passing silently, and every other width keeps
// trapping exactly when the host reference says "overflow".

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
    case Op::Add:
      return "+";
    case Op::Sub:
      return "-";
    case Op::Mul:
      return "*";
    case Op::Div:
      return "/";
    default:
      return "%";
    }
  }

  /// Host reference: does `left op right` overflow/trap at this width?
  [[nodiscard]] auto referenceTraps(const WidthSpec &width, Op op, uint64_t rawLeft, uint64_t rawRight) -> bool
  {
    if (op == Op::Div || op == Op::Rem)
      return rawRight == 0 || (width.isSigned && rawLeft == static_cast<uint64_t>(width.min) &&
                               rawRight == static_cast<uint64_t>(UINT64_MAX));
    // Overflow checks via widening arithmetic in 128-bit space.
    const __int128 wideLeft =
        width.isSigned ? static_cast<__int128>(static_cast<int64_t>(rawLeft)) : static_cast<__int128>(rawLeft);
    __int128 wideRight =
        width.isSigned ? static_cast<__int128>(static_cast<int64_t>(rawRight)) : static_cast<__int128>(rawRight);
    const __int128 result = op == Op::Add   ? wideLeft + wideRight
                            : op == Op::Sub ? wideLeft - wideRight
                                            : wideLeft * wideRight;
    return result < static_cast<__int128>(width.min) || result > static_cast<__int128>(width.max);
  }

  /// Host reference: exact in-width result of a non-trapping operation.
  [[nodiscard]] auto referenceResult(const WidthSpec &width, Op op, uint64_t rawLeft, uint64_t rawRight) -> int64_t
  {
    if (width.isSigned)
    {
      const auto left = static_cast<int64_t>(rawLeft);
      const auto right = static_cast<int64_t>(rawRight);
      switch (op)
      {
      case Op::Add:
        return static_cast<int64_t>(static_cast<uint64_t>(left) + static_cast<uint64_t>(right));
      case Op::Sub:
        return static_cast<int64_t>(static_cast<uint64_t>(left) - static_cast<uint64_t>(right));
      case Op::Mul:
        return static_cast<int64_t>(static_cast<uint64_t>(left) * static_cast<uint64_t>(right));
      case Op::Div:
        return left / right;
      default:
        return left % right;
      }
    }
    switch (op)
    {
    case Op::Add:
      return static_cast<int64_t>(rawLeft + rawRight);
    case Op::Sub:
      return static_cast<int64_t>(rawLeft - rawRight);
    case Op::Mul:
      return static_cast<int64_t>(rawLeft * rawRight);
    case Op::Div:
      return static_cast<int64_t>(rawLeft / rawRight);
    default:
      return static_cast<int64_t>(rawLeft % rawRight);
    }
  }

  /// Boundary-weighted operand inside one width's domain (INT_MIN spelling
  /// excluded; see INC-4 notes).
  [[nodiscard]] auto drawOperand(pbt::Rng &rng, const WidthSpec &width) -> uint64_t
  {
    const auto roll = rng.nextU64InRange(0, 99);
    if (roll < 15)
      return width.max;
    if (roll < 25)
      return width.isSigned ? static_cast<uint64_t>(width.min) + 1 : 0;
    if (roll < 40)
      return rng.below(0.5) ? 1 : width.max / 2;
    if (width.isSigned)
    {
      const auto span = static_cast<uint64_t>(width.max) - static_cast<uint64_t>(width.min);
      return static_cast<uint64_t>(width.min) + rng.nextU64InRange(0, span);
    }
    return rng.nextU64InRange(0, width.max);
  }

  [[nodiscard]] auto literalFor(const WidthSpec &width, uint64_t raw) -> std::string
  {
    // Unsigned widths always spell plain decimals; only signed widths may
    // render a bare negation (which then adopts the declared width).
    if (!width.isSigned || static_cast<int64_t>(raw) >= 0)
      return std::to_string(raw);
    return "-" + std::to_string(-static_cast<int64_t>(raw));
  }
} // namespace

TEST_CASE("INC-5 arithmetic matches host reference and never wraps silently", "[INC5][Numeric][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("overflow");
  constexpr uint32_t triplesPerWidth = 14;
  pbt::Rng rng(seed);

  struct Statement
  {
    bool expected;
    std::string source;
  };
  std::vector<Statement> statements;

  while (statements.size() < static_cast<size_t>(triplesPerWidth) * widthSpecs().size())
  {
    for (const auto &width : widthSpecs())
    {
      if (statements.size() >= static_cast<size_t>(triplesPerWidth) * widthSpecs().size())
        break;
      const auto op = static_cast<Op>(rng.nextU64InRange(0, 4));
      const auto rawLeft = drawOperand(rng, width);
      const auto rawRight = drawOperand(rng, width);
      if (referenceTraps(width, op, rawLeft, rawRight))
        continue; // trap cases are exercised by dedicated probes below
      const auto index = statements.size();
      const auto expectedLiteral =
          literalFor(width, static_cast<uint64_t>(referenceResult(width, op, rawLeft, rawRight)));
      // Compare within the width via an annotated binding: bare literals in
      // comparisons default to i64 and reject full-range unsigned values.
      // A silent wrap shows up as a false boolean rather than a wrong value.
      statements.push_back(
          Statement{true, std::format("    let l{0}: {4} = {1};\n    let r{0}: {4} = {2};\n    let e{0}: {4} = {3};\n"
                                      "    print(l{0} {5} r{0} == e{0});",
                                      index, literalFor(width, rawLeft), literalFor(width, rawRight), expectedLiteral,
                                      width.name, opText(op))});
    }
  }

  std::string source = "native fun print(value: bool) -> unit;\nfun main() -> unit {\n";
  for (const auto &statement : statements)
    source += statement.source + "\n";
  source += "}\n";

  const auto modulePath = std::filesystem::temp_directory_path() / "ng_inc5_arith.ng";
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
  const auto text = output.str();
  while (begin <= text.size() && index < statements.size())
  {
    const auto end = text.find('\n', begin);
    if (end == std::string::npos)
      break;
    const auto line = text.substr(begin, end - begin);
    begin = end + 1;
    if (line.empty() || line.starts_with("compiled ") || line.starts_with("native main exited"))
      continue;
    const auto actual = line == "true";
    if (!actual)
    {
      WARN("mismatch at statement " << index << ": got " << line << ", want true (expected result "
                                    << statements[index].expected << ")");
      ++mismatches;
    }
    ++index;
  }
  CHECK(index == statements.size());
  CHECK(mismatches == 0);
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
    const bool trapped = errorText.find("killed by a signal") != std::string::npos ||
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
