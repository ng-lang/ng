// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-22 (M5): generic affine `Cell<T>` (std.memory) — handle-style interior
// mutability. A Cell owns its handle (Drop release, drop-exactly-once) and is
// affine, but `cellGet`/`cellSet` take a *shared* `ref`: many shared borrowers
// may read and write the same cell without a `ref mut`, while ref-mut
// exclusivity still applies whenever an actual mutable borrow is taken.
//
// * PBT: get/set round-trip over generated i64 value pairs (48 trials in one
//   fixture), with live-cell accounting balanced across the loop.
// * Fixed behavior: word/string/array/struct payload round-trips; shared-ref
//   interior mutability (a bound `ref` observes a later `cellSet`); mutation
//   across call boundaries through shared refs.
// * Negative contracts: `ref mut` conflicts in both orderings; affine moves
//   reject late reads (`use of moved value`).
#include "test.hpp"

#include "driver.hpp"
#include "pbt.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
  // `--source` mode resolves only the imported function surface, not qualified
  // type names; write fixtures to a temp file for the full visible-name surface
  // (same rationale as box_test.cpp).
  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    const auto directory = std::filesystem::temp_directory_path();
    std::string stem = "cell_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(&output));
    std::filesystem::create_directories(directory);
    const auto path = directory / (stem + ".ng");
    std::ofstream file{path};
    file << source;
    file.close();

    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({path.string()}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return status;
  }

  auto requireExitCode(std::string_view source, std::string &output, std::string &errors) -> void
  {
    const int status = run(source, output, errors);
    INFO("errors: " << errors);
    REQUIRE(status == 0);
    REQUIRE(errors.empty());
    REQUIRE(output.find("code 0") != std::string::npos);
  }
} // namespace

TEST_CASE("vNext memory.Cell<T> get/set round-trip over generated i64 pairs (PBT)", "[vNext][Cell][INC22][Pbt][Runtime]")
{
  const uint64_t seed = pbt::currentSeed("cell");
  pbt::Rng rng(seed);
  constexpr int64_t kRange = 1 << 20;

  std::string seeds;
  std::string updates;
  constexpr int64_t kCount = 48;
  for (int64_t index = 0; index < kCount; ++index)
  {
    if (index > 0)
    {
      seeds += ", ";
      updates += ", ";
    }
    seeds += std::to_string(static_cast<int64_t>(rng.nextU64InRange(0, static_cast<uint64_t>(kRange))) - kRange / 2);
    updates += std::to_string(static_cast<int64_t>(rng.nextU64InRange(0, static_cast<uint64_t>(kRange))) - kRange / 2);
  }

  const std::string source = "import prelude; import memory; "
                             "fun main() -> i64 { "
                             "  let before = outstanding(); "
                             "  let seeds = [" + seeds + "]; "
                             "  let updates = [" + updates + "]; "
                             "  let mut i = 0; "
                             "  while (i < " + std::to_string(kCount) + ") { "
                             "    let c = newCell(seeds[i]); "
                             "    if (cellGet(ref c) != seeds[i]) { return 2; } "
                             "    cellSet(ref c, updates[i]); "
                             "    if (cellGet(ref c) != updates[i]) { return 3; } "
                             "    i := i + 1; "
                             "  } "
                             "  if (outstanding() != before) { return 4; } "
                             "  return 0; }";

  std::string output;
  std::string errors;
  requireExitCode(source, output, errors);
}

TEST_CASE("vNext memory.Cell<T> word/string/array/struct payload round-trips", "[vNext][Cell][INC22][Runtime]")
{
  const std::string source = "import memory; "
                             "struct Point { x: i64, y: i64 } "
                             "fun main() -> i64 { "
                             "  let a = newCell(7); "
                             "  if (cellGet(ref a) != 7) { return 1; } "
                             "  cellSet(ref a, 9); "
                             "  if (cellGet(ref a) != 9) { return 2; } "
                             "  let s = newCell(\"hi\"); "
                             "  cellSet(ref s, \"yo\"); "
                             "  if (cellGet(ref s) != \"yo\") { return 4; } "
                             "  let arr = newCell([1, 2, 3]); "
                             "  cellSet(ref arr, [7, 8]); "
                             "  if (cellGet(ref arr)[1] != 8) { return 8; } "
                             "  let p = newCell(Point { x: 3, y: 4 }); "
                             "  if (cellGet(ref p).y != 4) { return 16; } "
                             "  return 0; }";

  std::string output;
  std::string errors;
  requireExitCode(source, output, errors);
}

TEST_CASE("vNext memory.Cell<T> shared refs observe later writes (interior mutability)", "[vNext][Cell][INC22][Runtime]")
{
  const std::string source = "import memory; "
                             "fun bump(c: Cell<i64> ref, n: i64) -> unit { "
                             "  cellSet(c, cellGet(c) + n); } "
                             "fun main() -> i64 { "
                             "  let c = newCell(0); "
                             "  cellSet(ref c, 5); "
                             "  if (cellGet(ref c) != 5) { return 1; } "
                             "  let r = ref c; "
                             "  cellSet(ref c, 9); "
                             "  if (cellGet(r) != 9) { return 2; } "
                             "  bump(ref c, 3); "
                             "  if (cellGet(ref c) != 12) { return 4; } "
                             "  if (cellGet(r) != 12) { return 8; } "
                             "  return 0; }";

  std::string output;
  std::string errors;
  requireExitCode(source, output, errors);
}

TEST_CASE("vNext memory.Cell<T> shared writes do not dodge ref-mut exclusivity", "[vNext][Cell][INC22][Compile]")
{
  std::string output;
  std::string errors;

  REQUIRE(run("import memory; "
              "fun main() -> i64 { "
              "  let mut c = newCell(1); "
              "  let m = ref mut c; "
              "  cellSet(ref c, 2); "
              "  return cellGet(m); }",
              output, errors) != 0);
  INFO("errors: " << errors);
  REQUIRE_THAT(errors, ContainsSubstring("mutably borrowed"));

  REQUIRE(run("import memory; "
              "fun main() -> i64 { "
              "  let mut c = newCell(1); "
              "  let r = ref c; "
              "  let m = ref mut c; "
              "  return cellGet(r) + cellGet(m); }",
              output, errors) != 0);
  INFO("errors: " << errors);
  REQUIRE_THAT(errors, ContainsSubstring("shared-borrowed"));
}

TEST_CASE("vNext memory.Cell<T> affine moves reject late reads", "[vNext][Cell][INC22][Compile]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "fun main() -> i64 { "
              "  let c = newCell(7); "
              "  let d = c; "
              "  return cellGet(ref c); }",
              output, errors) != 0);
  INFO("errors: " << errors);
  REQUIRE_THAT(errors, ContainsSubstring("use of moved value"));
}