// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-0 + INC-18: leak probe corpus and the arena leak contract.
//
// The probe program (test/probes/leak_concat.ng) retains every intermediate
// string buffer, so a non-arena runtime reports ~one leaked malloc block per
// concatenation. INC-18 routes every program value allocation through the
// exit-time bump arena (mmap'd anonymous blocks, released at exit), which is
// never visible to malloc-based leak reporters — so the deterministic
// contract is now ZERO malloc leaks for the same probe. LeakSanitizer is
// unavailable on arm64 macOS ("detect_leaks is not supported"), so the count
// comes from the platform `leaks` tool (MallocStackLogging) run against the
// generated executable; the ASan/UBSan build variant guards memory errors
// separately.

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
TEST_CASE("INC-18 arena: leak probe reports zero malloc leaks via leaks(1)", "[INC18][Leak][Probe]")
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
    // Every program allocation is arena-owned (INC-18), so the 19999 leaked
    // concatenation buffers are gone: the mallocs left at exit are at most
    // dyld/libc one-time allocations (originally a handful of nodes).
    CHECK(leakedAllocations == 0);
    CHECK(leakedBytes == 0);
  }
  WARN("INC-18: leaks(1) reported " << leakedAllocations << " leaked allocations / " << leakedBytes
                                    << " bytes (arena-reclaimed; was 19999 / ~205 MB pre-INC-18)");
  std::filesystem::remove(executable, ignored);
}
#endif
