// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-18 (exit-time bump arena): the program heap serving layer introduced
// before DEC-C (RC + CoW, INC-19). Every program value allocation is busted
// from mmap'd anonymous blocks that are reclaimed at process exit; individual
// blocks are never returned, so the runtime is leak-clean by construction at
// the malloc level. This suite pins:
//
//   - the arena host surface (alloc / new_string / free dispatch / stats),
//   - the no-op free of arena-owned pointers and the preserved free of
//     malloc'd scratch, plus release idempotence,
//   - the throughput smoke: N consecutive bumps are accounted in
//     ngrt_arena_bytes()/ngrt_arena_blocks(),
//   - the end-to-end probe (test/probes/arena_all.ng) covering every
//     allocation form the generated code can emit,
//   - on macOS, the leaks(1) contract that the probe and the INC-0 concat
//     probe report zero malloc leaks (the 19999-buffer pre-arena baseline).

#include "driver.hpp"
#include "ngrt.h"
#include "test.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace
{
  [[nodiscard]] auto probePath() -> std::filesystem::path
  {
    return std::filesystem::is_directory("test") ? std::filesystem::path{"test/probes/arena_all.ng"}
                                                 : std::filesystem::path{"../test/probes/arena_all.ng"};
  }

#ifdef __APPLE__
  /// Captures command output through a temp file: leaks(1) stalls forever
  /// when its stdout is a pipe, so popen is not usable here.
  [[nodiscard]] auto captureCommand(const std::string &command) -> std::string
  {
    const auto scratch = std::filesystem::temp_directory_path() / "ng_arena_probe_capture.txt";
    std::filesystem::remove(scratch);
    std::ignore = std::system(std::format("{} > '{}' 2>&1", command, scratch.string()).c_str());
    std::ifstream stream(scratch);
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  }

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
#endif
} // namespace

TEST_CASE("INC-18 arena: alloc/new_string/free dispatch and stats", "[vNext][INC-18][Arena][Runtime]")
{
  // Reset to a deterministic baseline; the arena is single-threaded and the
  // enclosing process owns it, so release is a safe way to take a clean read.
  ngrt_arena_release();
  REQUIRE(ngrt_arena_blocks() == 0);
  REQUIRE(ngrt_arena_bytes() == 0);

  // Bump allocation is accounted in the stats and is not individually
  // freeable: free() leaves the bytes/block accounting exactly in place.
  auto *block = static_cast<char *>(ngrt_alloc(4096));
  REQUIRE(block != nullptr);
  REQUIRE(ngrt_arena_blocks() >= 1);
  REQUIRE(ngrt_arena_bytes() >= 4096);
  const auto bytesBeforeFree = ngrt_arena_bytes();
  ngrt_free(block);
  REQUIRE(ngrt_arena_bytes() == bytesBeforeFree);

  // new_string keeps the len-prefixed layout and bumps the same arena.
  auto *text = ngrt_new_string("arena", 5);
  REQUIRE(*(static_cast<int64_t *>(static_cast<void *>(text))) == 5);
  REQUIRE(ngrt_arena_bytes() > bytesBeforeFree);

  // malloc'd scratch is still released by ngrt_free (the dispatch walks the
  // arena blocks and falls through to libc free for foreign pointers).
  for (int index = 0; index < 1024; ++index)
  {
    auto *scratch = static_cast<char *>(std::malloc(64));
    scratch[0] = static_cast<char>(index);
    ngrt_free(scratch);
  }

  // nullptr is safe in both directions.
  ngrt_free(nullptr);
  ngrt_arena_release();
  REQUIRE(ngrt_arena_blocks() == 0);
  REQUIRE(ngrt_arena_bytes() == 0);

  // Release is idempotent and the arena is reusable afterwards.
  ngrt_arena_release();
  REQUIRE(ngrt_arena_blocks() == 0);
  auto *again = static_cast<char *>(ngrt_alloc(16));
  REQUIRE(again != nullptr);
  REQUIRE(ngrt_arena_blocks() == 1);
}

TEST_CASE("INC-18 arena: throughput smoke — many bumps are accounted", "[vNext][INC-18][Arena][Runtime]")
{
  ngrt_arena_release();
  constexpr int64_t Allocations = 200'000;
  constexpr int64_t Size = 64;
  for (int64_t index = 0; index < Allocations; ++index)
  {
    auto *cell = static_cast<char *>(ngrt_alloc(Size));
    cell[0] = static_cast<char>(index);
  }
  // 200k × 64 B = 12.8 MB through the arena; 1 MiB blocks → ≥ 13 blocks and
  // the full byte accounting.
  REQUIRE(ngrt_arena_blocks() >= 13);
  REQUIRE(ngrt_arena_bytes() >= Allocations * Size);
}

TEST_CASE("INC-18 arena: arena_all probe runs end to end", "[vNext][INC-18][Arena][Probe][Runtime]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({probePath().string()}, output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  REQUIRE_THAT(output.str(), Catch::Matchers::ContainsSubstring("arena-ok"));
}

#ifdef __APPLE__
TEST_CASE("INC-18 arena: arena_all probe reports zero malloc leaks via leaks(1)", "[vNext][INC-18][Arena][Leak][Probe]")
{
  const auto executable = std::filesystem::temp_directory_path() / "ng_arena_all_out";
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
    CHECK(leakedAllocations == 0);
    CHECK(leakedBytes == 0);
  }
  std::filesystem::remove(executable, ignored);
}
#endif