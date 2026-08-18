// AI-generated code; reviewed for this repository's vNext rewrite.
#include "driver.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
  auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext ngi driver parses an expression without loading the legacy runtime", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--expr", "2 * 3 + 4"}, output, errors) == 0);
  REQUIRE(output == "parsed vNext expression at bytes [0, 9)\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver runs the complete replacement compile-verify-execute pipeline", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() { let value = 1; value }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s)\nnative main exited with code 0\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver runs a parameterless typed i64 main", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() -> i64 { return 6 * 7; }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s)\nnative main exited with code 42\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver passes string arguments and reports string returns", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() -> string { return \"hello, Ada\"; }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s)\nhello, Ada\nnative main exited with code 0\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver executes direct calls through the module VM", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(
      run({"--source", "fun helper(value: i64) -> i64 { return value + 1; } fun main() -> i64 { return helper(41); }"},
          output, errors) == 0);
  REQUIRE(output == "compiled 2 vNext function(s)\nnative main exited with code 42\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver exposes a concrete main return value", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() -> i64 { return 6 * 7; }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s)\nnative main exited with code 42\n");
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver reports typed pipeline errors through its new frontend boundary", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() { if 1 { return; } }"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE(errors == "type error at bytes [16, 17): if condition type mismatch: expected bool, got i64\n");
}

TEST_CASE("vNext ngi driver reports syntax errors through its new frontend boundary", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--expr", "a + @"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE(errors == "syntax error at bytes [4, 5): unexpected character `@`\n");
}

TEST_CASE("vNext ngi driver reports syntax errors in --source units instead of aborting", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() { return if (true) { 1 } else { 0 }; }"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE_THAT(errors, ContainsSubstring("syntax error at bytes ["));
  REQUIRE_THAT(errors, ContainsSubstring("expected an expression"));
}

TEST_CASE("vNext ngi driver reports syntax errors in file mode instead of aborting", "[vNext][Driver]")
{
  const auto path = std::filesystem::temp_directory_path() / "ng_driver_syntax_error.ng";
  {
    std::ofstream file{path};
    file << "fun main() { let x = ; }\n";
  }
  std::string output;
  std::string errors;
  REQUIRE(run({path.string()}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE_THAT(errors, ContainsSubstring("syntax error at bytes ["));
  std::filesystem::remove(path);
}

TEST_CASE("vNext ngi driver provides deterministic command-line diagnostics", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--expr"}, output, errors) == 1);
  REQUIRE(output.empty());
  REQUIRE(errors == "--expr requires exactly one expression argument\n");
}

TEST_CASE("vNext ngi driver lifts the fuel budget with --fuel 0", "[vNext][Driver][Fuel]")
{
  std::string output;
  std::string errors;
  REQUIRE(
      run({"--source", "fun main() -> unit { loop (i = 0) { if (i == 9) { return; } next (i + 1); } }", "--fuel", "0"},
          output, errors) == 0);
  REQUIRE_THAT(output, ContainsSubstring("native main exited"));
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver prints usage with no arguments and for --help", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({}, output, errors) == 1);
  REQUIRE_THAT(output, ContainsSubstring("Usage: ngi --expr <expression>"));
  REQUIRE(errors.empty());

  REQUIRE(run({"--help"}, output, errors) == 0);
  REQUIRE_THAT(output, ContainsSubstring("Usage: ngi"));
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver reports f64 main return values", "[vNext][Driver]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() -> f64 { return 1.5; }"}, output, errors) == 0);
  REQUIRE_THAT(output, ContainsSubstring("1.5"));
  REQUIRE(errors.empty());
}

TEST_CASE("vNext ngi driver compiles and links an imgui program without running it", "[vNext][Driver][Imgui]")
{
  // Compiling with --output exercises the whole native link path for an
  // imgui program: libngrt_imgui.a plus the SDL3/Dear ImGui archives resolved
  // through NG_LIBRARY_PATH / the build-tree defaults, without opening a
  // window.
  const auto outputPath = std::filesystem::temp_directory_path() / "ng_imgui_compile_test";
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import imgui; fun main() -> unit { imguiInit(); imguiCleanup(); }", "--output",
               outputPath.string()},
              output, errors) == 0);
  REQUIRE(output.find("native executable written to") != std::string::npos);
  REQUIRE(errors.empty());
  std::filesystem::remove(outputPath);
}
