// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-19 S1: generic affine Box<T> (std.memory) — value semantics through a
// session-owned native handle table, deep-copy read/write, affine moves, and
// drop-exactly-once live-cell accounting via `outstanding()`.
#include "driver.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace
{
  // `--source` mode resolves only the imported function surface, not qualified
  // type names (`memory.Box<T>`); write the fixture to a temp file so the
  // module loader applies the full visible-name surface.
  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    const auto directory = std::filesystem::temp_directory_path();
    std::string stem = "box_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(&output));
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

  [[nodiscard]] auto runExample(std::string_view filename, std::string &output, std::string &errors) -> int
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example"))
      path = std::string{"../"} + path;
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({path}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext memory.Box<T> new/read/write round-trip word payloads", "[vNext][Box][INC19][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "struct Point { x: i64, y: i64 } "
              "fun main() -> i64 { "
              "  let a = new(7); write(ref a, 9); "
              "  if (read(ref a) != 9) { return 1; } "
              "  let s = new(\"hi\"); if (read(ref s) != \"hi\") { return 2; } "
              "  let arr = new([1, 2, 3]); if (read(ref arr)[1] != 2) { return 4; } "
              "  let p = new(Point { x: 3, y: 4 }); if (read(ref p).y != 4) { return 8; } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext memory.Box<T> box<T> alias and float bit-preserving round-trip", "[vNext][Box][INC19][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "fun main() -> i64 { "
              "  let b = box(42); "
              "  if (read(ref b) != 42) { return 1; } "
              "  let d = new(3.5); if (read(ref d) != 3.5) { return 2; } "
              "  let f = new(2.25f32); if (read(ref f) != 2.25f32) { return 4; } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext memory.Box<T> write snapshots; read never aliases the cell", "[vNext][Box][INC19][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "fun main() -> i64 { "
              "  let b = new([1, 2, 3]); "
              "  let mut source = [7, 8]; "
              "  write(ref b, source); "
              "  source := [1000, -1]; "
              "  if (read(ref b)[0] != 7) { return 1; } "
              "  let got = read(ref b); "
              "  if (got[1] != 8) { return 2; } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext memory.Box<T> affine move transfers ownership and drops exactly once",
          "[vNext][Box][INC19][PBT][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "fun consume(b: Box<i64>) -> i64 { "
              "  return read(ref b); } "
              "fun main() -> i64 { "
              "  let before = outstanding(); "
              "  let a = new(40); "
              "  let moved = a; "
              "  let after = outstanding(); "
              "  if (after != before + 1) { return 1; } "
              "  let got = consume(moved); "
              "  if (got != 40) { return 2; } "
              "  if (outstanding() != before) { return 3; } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext memory.Box<T> loops and early returns release every cell once", "[vNext][Box][INC19][PBT][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "fun earlyReturn(value: i64) -> i64 { "
              "  let a = new(2); "
              "  if (value < 0) { return -7; } "
              "  let b = new(3); "
              "  return read(ref a) + read(ref b); } "
              "fun main() -> i64 { "
              "  let before = outstanding(); "
              "  let mut index = 0; "
              "  while (index < 16) { "
              "    let cell = new(index); "
              "    if (read(ref cell) != index) { return 1; } "
              "    index := index + 1; "
              "  } "
              "  if (outstanding() != before) { return 2; } "
              "  if (earlyReturn(-1) != -7) { return 4; } "
              "  if (outstanding() != before) { return 8; } "
              "  if (earlyReturn(3) != 5) { return 16; } "
              "  if (outstanding() != before) { return 32; } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext memory.Box<T> read on a moved-from cell is rejected", "[vNext][Box][INC19][Compile]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "fun main() -> i64 { "
              "  let a = new(5); "
              "  let moved = a; "
              "  return read(ref a); }",
              output, errors) != 0);
  REQUIRE_THAT(errors, ContainsSubstring("use of moved value"));
}

TEST_CASE("vNext memory.Box<T> clone() deep-copies; originals stay live and drops balance",
          "[vNext][Box][INC20][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "fun main() -> i64 { "
              "  let a = new([1, 2, 3]); "
              "  let b = a.clone(); "
              "  write(ref b, [7, 8, 9]); "
              "  if (read(ref a)[0] != 1) { return 1; } "
              "  if (read(ref b)[0] != 7) { return 2; } "
              "  if (read(ref a)[2] != 3) { return 4; } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext memory.Box<T> clone() balances outstanding() through a helper", "[vNext][Box][INC20][PBT][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "struct Point { x: i64, y: i64 } "
              "fun cloneTemps() -> i64 { "
              "  let a = new(Point { x: 1, y: 2 }); "
              "  let b = a.clone(); "
              "  write(ref b, Point { x: 5, y: 6 }); "
              "  if (read(ref a).x != 1) { return 1; } "
              "  if (read(ref b).x != 5) { return 2; } "
              "  return 0; } "
              "fun main() -> i64 { "
              "  let before = outstanding(); "
              "  if (cloneTemps() != 0) { return 10; } "
              "  if (outstanding() != before) { return 20; } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext memory.Box<T> clone() recurses into enum/Box payloads", "[vNext][Box][INC20][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory; "
              "enum E { A(i64, Box<E>), B } "
              "fun main() -> i64 { "
              "  let leaf = new<E>(E.B); "
              "  let inner = new<E>(E.A(2, leaf)); "
              "  let root = new<E>(E.A(1, inner)); "
              "  let other = root.clone(); "
              "  switch (read(ref other)) { "
              "    case A(v, rest) { if (v != 1) { return 1; } "
              "      switch (read(ref rest)) { "
              "        case A(w, deeper) { if (w != 2) { return 2; } "
              "          switch (read(ref deeper)) { "
              "            case B { } "
              "            case A(u, w) { return 4; } } } "
              "        case B { return 8; } } } "
              "    case B { return 16; } } "
              "  return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 0") != std::string::npos);
}

TEST_CASE("vNext example/box.ng runs the box corpus end to end", "[vNext][Box][INC19][Sweep]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/box.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("compiled") != std::string::npos);
}