// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-2: conformance corpus runner (contract layers 1+2).
//
// Layer 1 — positive behavior contracts: `test/conformance/<case>.ng` plus
// `<case>.expected` holds the exact stdout lines the NG program must emit.
// Driver banner lines ("compiled ...", "native main exited with code ...")
// are filtered before comparison; a case passes only when compilation
// succeeds and the remaining stdout matches line for line.
//
// Layer 2 — negative diagnostic contracts: `<case>.neg.ng` plus
// `<case>.neg.expected` made of `key: value` directives:
//     kind:    type | resolution | syntax   (diagnostic class)
//     pattern: substring the message must contain
//     span:    [begin, end)                (exact byte interval)
// The driver must exit non-zero and emit exactly that diagnostic.
//
// This corpus is the authoritative executable specification for language
// behavior; later increments (INC-4, INC-6, ...) extend it rather than
// duplicating assertions in C++ tests. INC-9 upgrades the directives to
// stable error codes.

#include "driver.hpp"
#include "test.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace
{
  [[nodiscard]] auto conformanceDirectory() -> std::filesystem::path
  {
    return std::filesystem::is_directory("test") ? std::filesystem::path{"test/conformance"}
                                                 : std::filesystem::path{"../test/conformance"};
  }

  [[nodiscard]] auto listCases(const std::filesystem::path &directory, std::string_view suffix)
      -> std::vector<std::string>
  {
    std::vector<std::string> cases;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
      if (entry.is_regular_file() && entry.path().string().ends_with(suffix))
        cases.push_back(entry.path().filename().string());
    std::sort(cases.begin(), cases.end());
    return cases;
  }

  [[nodiscard]] auto readFile(const std::filesystem::path &path) -> std::string
  {
    std::ifstream stream(path);
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  }

  [[nodiscard]] auto splitLines(std::string_view text) -> std::vector<std::string>
  {
    std::vector<std::string> lines;
    size_t begin = 0;
    while (begin <= text.size())
    {
      const size_t end = text.find('\n', begin);
      lines.emplace_back(text.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin));
      if (end == std::string_view::npos)
        break;
      begin = end + 1;
    }
    while (!lines.empty() && lines.back().empty())
      lines.pop_back();
    return lines;
  }

  /// Strips driver banner lines so `.expected` files contain only what the
  /// NG program itself printed to stdout.
  [[nodiscard]] auto programLines(const std::string &driverOutput) -> std::vector<std::string>
  {
    std::vector<std::string> kept;
    for (auto &line : splitLines(driverOutput))
    {
      if (line.starts_with("compiled ") && line.ends_with(" vNext function(s)"))
        continue;
      if (line.starts_with("native main exited with code "))
        continue;
      kept.push_back(std::move(line));
    }
    return kept;
  }

  struct NegativeContract
  {
    std::string kind;
    std::string pattern;
    std::optional<std::pair<size_t, size_t>> span;
  };

  [[nodiscard]] auto parseNegativeContract(const std::string &text) -> std::optional<NegativeContract>
  {
    NegativeContract contract;
    bool sawKind = false;
    for (const auto &line : splitLines(text))
    {
      const auto colon = line.find(':');
      if (colon == std::string::npos)
        continue;
      const auto key = line.substr(0, colon);
      auto value = line.substr(colon + 1);
      while (!value.empty() && value.front() == ' ')
        value.erase(value.begin());
      if (key == "kind")
      {
        contract.kind = value;
        sawKind = true;
      }
      else if (key == "pattern")
        contract.pattern = value;
      else if (key == "span" && value.starts_with('['))
      {
        const auto separator = value.find(',');
        const auto close = value.rfind(')');
        if (separator == std::string::npos || close == std::string::npos)
          return std::nullopt;
        const auto beginValue = value.substr(1, separator - 1);
        const auto endValue = value.substr(separator + 1, close - separator - 1);
        contract.span =
            std::make_pair(static_cast<size_t>(std::stoull(beginValue)), static_cast<size_t>(std::stoull(endValue)));
      }
    }
    if (!sawKind)
      return std::nullopt;
    return contract;
  }
} // namespace

TEST_CASE("INC-2 positive contract corpus matches recorded behavior", "[INC2][Contract]")
{
  const auto directory = conformanceDirectory();
  const auto cases = listCases(directory, ".ng");
  REQUIRE(!cases.empty());

  for (const auto &caseName : cases)
  {
    if (caseName.ends_with(".neg.ng"))
      continue;
    INFO("positive case: " << caseName);
    const auto sourcePath = directory / caseName;
    const auto expectedPath = sourcePath.string().substr(0, sourcePath.string().size() - 3) + ".expected";

    std::ostringstream output;
    std::ostringstream errors;
    const int status = NG::runDriver({sourcePath.string()}, output, errors);
    INFO(errors.str());
    CHECK(status == 0);

    const auto expected = readFile(expectedPath);
    const auto expectedLines = splitLines(expected);
    const auto actualLines = programLines(output.str());
    CHECK(actualLines.size() == expectedLines.size());
    for (size_t index = 0; index < std::min(actualLines.size(), expectedLines.size()); ++index)
      CHECK(actualLines[index] == expectedLines[index]);
  }
}

TEST_CASE("INC-2 negative diagnostic contracts assert kind, pattern, and span", "[INC2][Contract][Negative]")
{
  const auto directory = conformanceDirectory();
  const auto cases = listCases(directory, ".neg.ng");
  REQUIRE(!cases.empty());

  for (const auto &caseName : cases)
  {
    INFO("negative case: " << caseName);
    const auto sourcePath = directory / caseName;
    // "<case>.neg.ng" -> "<case>.neg.expected"
    const auto expectedPath = sourcePath.string().substr(0, sourcePath.string().size() - 3) + ".expected";
    const auto contract = parseNegativeContract(readFile(expectedPath));
    REQUIRE(contract.has_value());

    std::ostringstream output;
    std::ostringstream errors;
    const int status = NG::runDriver({sourcePath.string()}, output, errors);
    CHECK(status != 0);

    // Find the diagnostic of the contracted kind and verify its shape:
    // "<kind> error at bytes [begin, end): <message>".
    bool matchedDiagnostic = false;
    for (const auto &line : splitLines(errors.str()))
    {
      const auto prefix = contract->kind + " error at bytes [";
      if (!line.starts_with(prefix))
        continue;
      matchedDiagnostic = true;

      const auto separator = line.find(", ");
      const auto closeParen = line.find("): ");
      CHECK(separator != std::string::npos);
      CHECK(closeParen != std::string::npos);
      if (separator == std::string::npos || closeParen == std::string::npos || !contract->span.has_value())
        break;

      const auto beginValue = static_cast<size_t>(std::stoull(line.substr(prefix.size(), separator - prefix.size())));
      const auto endValue = static_cast<size_t>(std::stoull(line.substr(separator + 2, closeParen - separator - 2)));
      INFO("reported span: [" << beginValue << ", " << endValue << ")");
      CHECK(beginValue == contract->span->first);
      CHECK(endValue == contract->span->second);

      const auto message = line.substr(closeParen + 3);
      INFO("reported message: " << message);
      CHECK(message.contains(contract->pattern));
      break;
    }
    CHECK(matchedDiagnostic);
  }
}
