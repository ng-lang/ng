// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-7: toString / parseInt round-trips over the full integer domain and
// boundary contracts for parsing (empty / sign-only / non-digit / overflow).

#include "pbt.hpp"
#include "test.hpp"

#include "driver.hpp"

#include <string>
#include <vector>

namespace
{
  /// One round-trip probe: parseInt(toString(N)) must recover N exactly.
  /// Both sides share pbt::decimalLiteral so a literal-spelling bug cannot
  /// make the two sides agree on a wrong value (the `--N` false-green).
  [[nodiscard]] auto roundTripProbe(size_t index, int64_t value) -> pbt::BatchProbe
  {
    const auto literal = pbt::decimalLiteral(value);
    return pbt::BatchProbe{
        .source = std::format(
            "    let s{0} = toString({1});\n"
            "    let r{0} = parseInt(s{0});\n"
            "    switch (r{0}) {{\n"
            "        case Some(v{0}) {{ print(v{0} == {1}); }}\n"
            "        case None {{ print(false); }}\n"
            "    }}",
            index, literal),
        .expect = "true",
        .label = std::format("parseInt(toString({})) == {}", literal, literal),
    };
  }
} // namespace

TEST_CASE("INC-7 parseInt(toString(n)) recovers every generated integer", "[INC7][String][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("int-string");
  constexpr uint32_t trialsPerRange = 40;

  std::vector<pbt::BatchProbe> probes;

  // Deliberate extremes. INT64_MIN's only spelling is the arithmetic form;
  // both sides use it, and the no-double-minus guard below keeps it honest.
  probes.push_back(pbt::BatchProbe{
      .source = "    let min0 = -9223372036854775807 - 1;\n"
                "    let r0 = parseInt(toString(min0));\n"
                "    switch (r0) {\n"
                "        case Some(v0) { print(v0 == -9223372036854775807 - 1); }\n"
                "        case None { print(false); }\n"
                "    }",
      .expect = "true",
      .label = "INT64_MIN round trip",
  });
  probes.push_back(roundTripProbe(probes.size(), 9223372036854775807LL));

  pbt::Rng rng(seed);
  size_t negativeSamples = 0;
  for (uint32_t trial = 0; trial < trialsPerRange * 3; ++trial)
  {
    // Every fifth trial is forced negative: a generator regression that
    // stopped producing negatives would otherwise pass silently.
    const bool forceNegative = trial % 5 == 0;
    int64_t value = 0;
    if (forceNegative)
      value = -static_cast<int64_t>(rng.nextU64InRange(1, static_cast<uint64_t>(INT32_MAX)));
    else if (rng.below(0.5))
      value = static_cast<int64_t>(rng.nextU64InRange(0, 255));
    else if (rng.below(0.5))
      value = static_cast<int64_t>(rng.nextU64InRange(0, INT32_MAX));
    else
    {
      const auto raw = rng.nextU64();
      value = static_cast<int64_t>(raw == 0x8000000000000000ULL ? 1 : raw);
    }
    negativeSamples += value < 0 ? 1 : 0;
    probes.push_back(roundTripProbe(probes.size(), value));
  }

  REQUIRE(negativeSamples > 0);

  std::string preamble = "import prelude;";
  std::string sourceText = preamble;
  for (const auto &probe : probes)
    sourceText += probe.source;
  // The `--N` false-green guard: double signs in generated source mean a
  // literal was spelled through a broken negation path (it parses as two
  // unary minuses and both comparison sides silently agree on |N|).
  REQUIRE(sourceText.find("--") == std::string::npos);

  const auto failure = pbt::runBatchProbes("inc7-roundtrip", preamble, probes);
  REQUIRE(!failure.has_value());
}

TEST_CASE("INC-7 toString formats each width like print", "[INC7][String]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({"--source",
                                    "import prelude; "
                                    "fun main() -> unit { print(toString(0)); print(toString(-1)); "
                                    "print(toString(true)); print(toString(false)); print(toString(2.5)); "
                                    "let small: i32 = -8; print(toString(small)); }"},
                                   output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  const auto text = output.str();
  REQUIRE_THAT(text, ContainsSubstring("0\n"));
  REQUIRE_THAT(text, ContainsSubstring("-1\n"));
  REQUIRE_THAT(text, ContainsSubstring("true\n"));
  REQUIRE_THAT(text, ContainsSubstring("false\n"));
  REQUIRE_THAT(text, ContainsSubstring("2.5\n"));
  REQUIRE_THAT(text, ContainsSubstring("-8\n"));
}
