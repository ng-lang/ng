// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-6 (F1②/DEC-A): every generated comparison expression has exactly one
// of two outcomes — it compiles with precise same-width semantics, or the
// checker produces a deterministic mixed-comparison diagnostic. There is no
// third state where a mixed comparison silently compiles with lossy rules.

#include "pbt.hpp"
#include "test.hpp"

#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  enum class Outcome
  {
    Legal,
    MixedSign,
    MixedKind,
    LiteralRange // e.g. a negative literal cannot adopt u64: still deterministic
  };

  struct GeneratedComparison
  {
    std::string leftDeclaration;
    std::string rightDeclaration;
    std::string expressionText;
    Outcome expected;
  };

  [[nodiscard]] auto generateCase(pbt::Rng &rng, size_t index) -> GeneratedComparison
  {
    const uint64_t kind = rng.nextU64InRange(0, 4);
    const auto name = "v" + std::to_string(index);
    switch (kind)
    {
    case 0:
      // Same signed width pair: legal, host semantics.
      return {.leftDeclaration = "let " + name + ": i64 = -7;",
              .rightDeclaration = "let w" + std::to_string(index) + ": i64 = 9;",
              .expressionText = name + " < w" + std::to_string(index),
              .expected = Outcome::Legal};
    case 1:
      // Same-sign cross width: legal under DEC-A.
      return {.leftDeclaration = "let " + name + ": i32 = -3;",
              .rightDeclaration = "let w" + std::to_string(index) + ": i64 = 5;",
              .expressionText = name + " <= w" + std::to_string(index),
              .expected = Outcome::Legal};
    case 2:
      // Signed vs unsigned variables: must diagnose.
      return {.leftDeclaration = "let " + name + ": i64 = 1;",
              .rightDeclaration = "let w" + std::to_string(index) + ": u64 = 2;",
              .expressionText = name + " != w" + std::to_string(index),
              .expected = Outcome::MixedSign};
    case 3:
      // Integer variable vs float variable: must diagnose.
      return {.leftDeclaration = "let " + name + ": i64 = 3;",
              .rightDeclaration = "let w" + std::to_string(index) + ": f64 = 2.5;",
              .expressionText = name + " >= w" + std::to_string(index),
              .expected = Outcome::MixedKind};
    default:
      // Integer literal adopts float context: legal.
      return {.leftDeclaration = "let " + name + ": f64 = 1.5;",
              .rightDeclaration = "",
              .expressionText = name + " == 1",
              .expected = Outcome::Legal};
    }
  }

  /// Classifies what the checker actually does with one comparison.
  [[nodiscard]] auto classify(const GeneratedComparison &comparison) -> Outcome
  {
    std::string source = "fun probe() -> bool {\n";
    source += "    " + comparison.leftDeclaration + "\n";
    if (!comparison.rightDeclaration.empty())
      source += "    " + comparison.rightDeclaration + "\n";
    source += "    return " + comparison.expressionText + ";\n}\n";
    try
    {
      const auto module = hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
      static_cast<void>(typecheck::TypeChecker{}.check(module));
      return Outcome::Legal;
    }
    catch (const typecheck::TypeError &error)
    {
      const auto message = std::string{error.what()};
      if (message.find("mixed-sign") != std::string::npos)
        return Outcome::MixedSign;
      if (message.find("mixed numeric-kind") != std::string::npos)
        return Outcome::MixedKind;
      return Outcome::LiteralRange;
    }
  }
} // namespace

TEST_CASE("INC-6 comparisons are either legal or deterministically diagnosed", "[INC6][Numeric][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("dec-a");
  constexpr uint32_t trials = 120;
  pbt::Rng rng(seed);
  for (uint32_t trial = 0; trial < trials; ++trial)
  {
    const auto comparison = generateCase(rng, trial);
    INFO("trial " << trial << ": " << comparison.expressionText);
    const auto actual = classify(comparison);
    if (comparison.expected == Outcome::Legal || comparison.expected == Outcome::LiteralRange)
      CHECK(actual == Outcome::Legal);
    else
      CHECK(actual == comparison.expected);
  }
}

TEST_CASE("INC-6 explicit conversions keep full-range values comparable", "[INC6][Numeric]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({"--source", "import prelude; "
                                                "fun main() -> unit { let big: u64 = 18446744073709551615; "
                                                "print(toSigned(big) < 0); print(toFloat(3) == 3.0); }"},
                                   output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  REQUIRE_THAT(output.str(), ContainsSubstring("true\ntrue"));
}
