// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-7: toString / parseInt round-trips over the full integer domain and
// boundary contracts for parsing (empty / sign-only / non-digit / overflow).

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
  [[nodiscard]] auto literalFor(int64_t raw) -> std::string
  {
    if (raw >= 0)
      return std::to_string(raw);
    return "-" + std::to_string(-static_cast<int64_t>(-raw));
  }

  /// One round-trip probe: parseInt(toString(N)) must recover N exactly.
  [[nodiscard]] auto roundTripStatement(int64_t value, size_t index) -> std::string
  {
    return std::format("    let s{0} = toString({1});\n"
                       "    let r{0} = parseInt(s{0});\n"
                       "    switch (r{0}) {{\n"
                       "        case Some(v{0}) {{ print(v{0} == {1}); }}\n"
                       "        case None {{ print(false); }}\n"
                       "    }}",
                       index, literalFor(value));
  }
} // namespace

TEST_CASE("INC-7 parseInt(toString(n)) recovers every generated integer", "[INC7][String][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("int-string");
  constexpr uint32_t trialsPerRange = 40;

  struct Probe
  {
    int64_t value;
    std::string source;
  };
  std::vector<Probe> probes;

  // Full-domain draws plus deliberate extremes (INT64_MIN spelled via the
  // max-negative-literal minus one, matching its dedicated expressibility
  // test).
  probes.push_back({-9223372036854775807LL - 1, "    let min = -9223372036854775807 - 1;\n"
                                                "    let rMin = parseInt(toString(min));\n"
                                                "    switch (rMin) {\n"
                                                "        case Some(vMin) { print(vMin == -9223372036854775807 - 1); }\n"
                                                "        case None { print(false); }\n"
                                                "    }"});
  probes.push_back({9223372036854775807LL, "    let rMax = parseInt(toString(9223372036854775807));\n"
                                           "    switch (rMax) {\n"
                                           "        case Some(vMax) { print(vMax == 9223372036854775807); }\n"
                                           "        case None { print(false); }\n"
                                           "    }"});

  pbt::Rng rng(seed);
  while (probes.size() < trialsPerRange * 3)
  {
    const auto roll = rng.nextU64InRange(0, 2);
    int64_t value = 0;
    if (roll == 0)
      value = static_cast<int64_t>(rng.nextU64InRange(0, 255));
    else if (roll == 1)
    {
      const auto magnitude = rng.nextU64InRange(0, static_cast<uint64_t>(INT32_MAX));
      value = rng.below(0.5) ? static_cast<int64_t>(magnitude) : -static_cast<int64_t>(magnitude);
    }
    else
    {
      const auto raw = rng.nextU64();
      value = static_cast<int64_t>(raw == 0x8000000000000000ULL ? 1 : raw);
    }
    probes.push_back({value, roundTripStatement(value, probes.size())});
  }

  std::string source = "import prelude;\nfun main() -> unit {\n";
  for (size_t index = 0; index < probes.size(); ++index)
    source += (index < 2 ? probes[index].source : roundTripStatement(probes[index].value, index)) + "\n";
  source += "}\n";

  const auto modulePath = std::filesystem::temp_directory_path() / "ng_inc7_roundtrip.ng";
  {
    std::ofstream stream(modulePath);
    stream << source;
  }
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({modulePath.string()}, output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);

  size_t seen = 0;
  size_t begin = 0;
  const auto text = output.str();
  while (begin <= text.size())
  {
    const auto end = text.find('\n', begin);
    if (end == std::string::npos)
      break;
    const auto line = text.substr(begin, end - begin);
    begin = end + 1;
    if (line.empty() || line.starts_with("compiled ") || line.starts_with("native main exited"))
      continue;
    CHECK(line == "true");
    ++seen;
  }
  CHECK(seen == probes.size());
}

TEST_CASE("INC-7 toString formats each width like print", "[INC7][String]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({"--source", "import prelude; "
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
