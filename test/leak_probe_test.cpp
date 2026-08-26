// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-0: leak probe corpus + deterministic leak-count baseline.
//
// The probe program (test/probes/leak_concat.ng) retains every intermediate
// string buffer, so a correct accounting reports ~one leaked allocation per
// concatenation. LeakSanitizer is unavailable on arm64 macOS (Apple clang:
// "detect_leaks is not supported"), so the deterministic count comes from
// the platform `leaks` tool (MallocStackLogging) run against the generated
// executable; the ASan/UBSan build variant guards memory errors separately.

#include "driver.hpp"
#include "test.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace
{
  [[nodiscard]] auto probePath() -> std::filesystem::path
  {
    return std::filesystem::is_directory("test") ? std::filesystem::path{"test/probes/leak_concat.ng"}
                                                 : std::filesystem::path{"../test/probes/leak_concat.ng"};
  }

  /// Captures command output through a temp file: leaks(1) stalls forever
  /// when its stdout is a pipe, so popen is not usable here.
  [[nodiscard]] auto captureCommand(const std::string &command) -> std::string
  {
    const auto scratch = std::filesystem::temp_directory_path() / "ng_leak_probe_capture.txt";
    std::filesystem::remove(scratch);
    // leaks(1) exits non-zero when it finds leaks, which is expected here;
    // the captured report is what matters, not the status.
    std::ignore = std::system(std::format("{} > '{}' 2>&1", command, scratch.string()).c_str());
    std::ifstream stream(scratch);
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  }

  /// Extracts `<n>` and `<m>` from the leaks(1) summary line
  /// "Process <pid>: <n> leak[s] for <m> total leaked bytes."
  /// Returns {-1, -1} when the summary is absent.
  [[nodiscard]] auto parseLeaks(const std::string &report) -> std::pair<long long, long long>
  {
    static constexpr std::string_view marker = "total leaked bytes";
    const auto markerBegin = report.find(marker);
    if (markerBegin == std::string::npos)
      return {-1, -1};
    const auto separator = report.rfind(" for ", markerBegin);
    const auto leakWord = separator == std::string::npos ? std::string::npos : report.rfind(" leak", separator);
    if (separator == std::string::npos || leakWord == std::string::npos)
      return {-1, -1};
    auto digitsBefore = [&](size_t end) -> long long
    {
      auto start = end;
      while (start > 0 && std::isdigit(static_cast<unsigned char>(report[start - 1])) != 0)
        --start;
      return start == end ? -1 : std::stoll(report.substr(start, end - start));
    };
    const auto allocations = digitsBefore(leakWord);
    const auto bytesBegin = separator + 5;
    auto bytesEnd = bytesBegin;
    while (bytesEnd < report.size() && std::isdigit(static_cast<unsigned char>(report[bytesEnd])) != 0)
      ++bytesEnd;
    const auto bytes = bytesEnd == bytesBegin ? -1 : std::stoll(report.substr(bytesBegin, bytesEnd - bytesBegin));
    return {allocations, bytes};
  }
} // namespace

TEST_CASE("INC-0 leak probe corpus compiles and runs cleanly", "[INC0][Leak][Probe]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({probePath().string()}, output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  REQUIRE_THAT(output.str(), ContainsSubstring("native main exited with code 0"));
}

#ifdef __APPLE__
TEST_CASE("INC-0 leak probe yields a deterministic leak count via leaks(1)", "[INC0][Leak][Probe]")
{
  const auto executable = std::filesystem::temp_directory_path() / "ng_leak_probe_out";
  std::error_code ignored;
  std::filesystem::remove(executable, ignored);
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({"--output", executable.string(), probePath().string()}, output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  REQUIRE(std::filesystem::exists(executable));

  const auto report = captureCommand(std::format("/usr/bin/leaks -atExit -- '{}'", executable.string()));
  INFO(report);
  const auto [leakedAllocations, leakedBytes] = parseLeaks(report);
  if (leakedAllocations < 0)
  {
    FAIL("leaks(1) produced no parsable summary:\n" << report);
  }
  else
  {
    // 19999 concatenations each strand the previous buffer; allow slack for
    // allocator coalescing and runtime bookkeeping around both bounds.
    CHECK(leakedAllocations >= 15000);
    CHECK(leakedBytes >= 100'000'000);
  }
  WARN("INC-0 baseline: leaks(1) reported " << leakedAllocations << " leaked allocations / " << leakedBytes
                                            << " bytes");
  std::filesystem::remove(executable, ignored);
}
#endif
