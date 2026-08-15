// AI-generated code; reviewed for this repository's vNext rewrite.
// M1/M2 slices of the QBE native backend: FlowIR -> QBE IL emission for
// scalar arithmetic, control flow, and direct calls, plus end-to-end round
// trips through the vendored qbe and the system toolchain.
#include "test.hpp"

#include "driver.hpp"

#include <fstream>
#include <sstream>
#include <string>

#if defined(NG_QBE_PATH) && !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace
{
  [[nodiscard]] auto emitSsa(std::string_view source) -> std::string
  {
    std::ostringstream output;
    std::ostringstream errors;
    const int status = NG::runDriver({"--emit=ssa", "--source", source}, output, errors);
    INFO(errors.str());
    REQUIRE(status == 0);
    return output.str();
  }

#if defined(NG_QBE_PATH) && !defined(_WIN32)
  /// Emits IL, assembles it with the vendored qbe, links it with the system
  /// cc, runs the executable, and returns its exit code (-1 on a crash).
  [[nodiscard]] auto runNative(std::string_view source, std::string_view label) -> int
  {
    const auto il = emitSsa(source);
    const auto directory = std::filesystem::temp_directory_path() / std::format("ng_qbe_e2e_{}", label);
    std::filesystem::create_directories(directory);
    const auto ilFile = directory / "main.ssa";
    const auto asmFile = directory / "main.s";
    const auto executable = directory / "main";
    {
      std::ofstream stream(ilFile);
      stream << il;
    }
    const auto run = [](const std::string &command) -> int { return std::system(command.c_str()); };
    INFO(il);
    REQUIRE(run(std::format("'{}' -o '{}' '{}'", NG_QBE_PATH, asmFile.string(), ilFile.string())) == 0);
    REQUIRE(run(std::format("cc '{}' -o '{}'", asmFile.string(), executable.string())) == 0);
    const int status = run(std::format("'{}'", executable.string()));
    REQUIRE(WIFEXITED(status));
    return WEXITSTATUS(status);
  }
#endif
} // namespace

TEST_CASE("vNext native lowering emits QBE IL for scalar arithmetic", "[vNext][Native][Qbe]")
{
  const auto il = emitSsa("fun main() -> i64 => 6 * 7;");
  REQUIRE_THAT(il, ContainsSubstring("export function l $main"));
  REQUIRE_THAT(il, ContainsSubstring("=l mul "));
  REQUIRE_THAT(il, ContainsSubstring("ret %"));
}

TEST_CASE("vNext native lowering emits branches, jumps, and phis for loops", "[vNext][Native][Qbe]")
{
  const auto il = emitSsa("fun main() -> i64 {\n"
                          "    let mut total = 0;\n"
                          "    loop (i = 0) {\n"
                          "        total := total + i;\n"
                          "        if (i == 5) { return total; }\n"
                          "        next (i + 1);\n"
                          "    }\n"
                          "}");
  REQUIRE_THAT(il, ContainsSubstring("phi"));
  REQUIRE_THAT(il, ContainsSubstring("jnz %"));
  REQUIRE_THAT(il, ContainsSubstring("storel"));
}

TEST_CASE("vNext native lowering emits direct calls with collision-free symbols", "[vNext][Native][Qbe]")
{
  const auto il = emitSsa("fun fib(n: i64) -> i64 {\n"
                          "    if (n <= 1) { return n; }\n"
                          "    return fib(n - 1) + fib(n - 2);\n"
                          "}\n"
                          "fun main() -> i64 => fib(10);");
  REQUIRE_THAT(il, ContainsSubstring("export function l $main"));
  REQUIRE_THAT(il, ContainsSubstring("function l $fib_"));
  REQUIRE_THAT(il, ContainsSubstring("call $fib_"));
}

TEST_CASE("vNext native lowering emits string data items and ngrt helpers", "[vNext][Native][Qbe]")
{
  const auto il = emitSsa("fun main() -> i64 { let a = \"hi\"; let b = a + \"!\"; if (b == \"hi!\") { return 1; } return 0; }");
  REQUIRE_THAT(il, ContainsSubstring("data $ngstr_"));
  REQUIRE_THAT(il, ContainsSubstring("$ngrt_str_concat"));
  REQUIRE_THAT(il, ContainsSubstring("$ngrt_str_eq"));
  REQUIRE_THAT(il, ContainsSubstring("$malloc"));
}

TEST_CASE("vNext native lowering rejects unsupported M1 constructs with a clear error", "[vNext][Native][Qbe]")
{
  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver(
      {"--emit=ssa", "--source", "struct Point { x: i64, y: i64 } fun main() -> i64 { let p = Point { x: 1, y: 2 }; 7 }"},
      output, errors);
  REQUIRE(status == 1);
  REQUIRE_THAT(errors.str(), ContainsSubstring("native lowering (M"));
}

#if defined(NG_QBE_PATH) && !defined(_WIN32)
TEST_CASE("vNext native lowering round-trips a loop through qbe and the system toolchain", "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun main() -> i64 {\n"
                                 "    let mut total = 0;\n"
                                 "    loop (i = 0) {\n"
                                 "        total := total + i;\n"
                                 "        if (i == 5) { return total; }\n"
                                 "        next (i + 1);\n"
                                 "    }\n"
                                 "}",
                                 "loop");
  CHECK(exitCode == 15); // 0+1+2+3+4+5
}

TEST_CASE("vNext native lowering round-trips recursion through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun fib(n: i64) -> i64 {\n"
                                 "    if (n <= 1) { return n; }\n"
                                 "    return fib(n - 1) + fib(n - 2);\n"
                                 "}\n"
                                 "fun main() -> i64 => fib(10);",
                                 "recursion");
  CHECK(exitCode == 55);
}

TEST_CASE("vNext native lowering round-trips tail recursion through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{  const auto il = emitSsa("fun sumTo(n: i64, acc: i64) -> i64 {\n"
                          "    if (n == 0) { return acc; }\n"
                          "    next (n - 1, acc + n);\n"
                          "}\n"
                          "fun main() -> i64 => sumTo(10, 0);");
  // The self call lowers to a loop (TailRecur): QBE forbids jumping back to
  // `@start`, so the one-shot entry logic lives there and the loop jumps
  // back to `@body0`.
  REQUIRE_THAT(il, ContainsSubstring("jmp @body0"));
  const auto directory = std::filesystem::temp_directory_path() / "ng_qbe_e2e_tailrec";
  std::filesystem::create_directories(directory);
  const auto ilFile = directory / "main.ssa";
  const auto asmFile = directory / "main.s";
  const auto executable = directory / "main";
  {
    std::ofstream stream(ilFile);
    stream << il;
  }
  const auto run = [](const std::string &command) -> int { return std::system(command.c_str()); };
  INFO(il);
  REQUIRE(run(std::format("'{}' -o '{}' '{}'", NG_QBE_PATH, asmFile.string(), ilFile.string())) == 0);
  REQUIRE(run(std::format("cc '{}' -o '{}'", asmFile.string(), executable.string())) == 0);
  const int status = run(std::format("'{}'", executable.string()));
  REQUIRE(WIFEXITED(status));
  CHECK(WEXITSTATUS(status) == 55);
}

TEST_CASE("vNext native lowering round-trips strings through qbe and the system toolchain", "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun main() -> i64 {\n"
                                 "    let a = \"hello\";\n"
                                 "    let b = a + \", world\";\n"
                                 "    if (b == \"hello, world\" && a != b) { return 1; }\n"
                                 "    return 0;\n"
                                 "}",
                                 "strings");
  CHECK(exitCode == 1);
}

TEST_CASE("vNext native lowering round-trips arrays through qbe and the system toolchain", "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun main() -> i64 {\n"
                                 "    let xs = [1, 2, 3];\n"
                                 "    let ys = xs << 4;\n"
                                 "    return xs[0] + ys[3] + xs[2];\n"
                                 "}",
                                 "arrays");
  CHECK(exitCode == 8); // 1 + 4 + 3
}

TEST_CASE("vNext native lowering round-trips tuples through qbe and the system toolchain", "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun main() -> i64 {\n"
                                 "    let t = (10, 20, 30);\n"
                                 "    return t.0 + t.2;\n"
                                 "}",
                                 "tuples");
  CHECK(exitCode == 40);
}

TEST_CASE("vNext native lowering round-trips range slices through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun main() -> i64 {\n"
                                 "    let xs = [10, 20, 30, 40];\n"
                                 "    let ys = xs[1..3];\n"
                                 "    return ys[0] + ys[1];\n"
                                 "}",
                                 "ranges");
  CHECK(exitCode == 50); // 20 + 30
}

TEST_CASE("vNext native lowering round-trips string arrays through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun main() -> i64 {\n"
                                 "    let xs = [\"ab\", \"cd\"];\n"
                                 "    if (xs[0] + xs[1] == \"abcd\") { return 1; }\n"
                                 "    return 0;\n"
                                 "}",
                                 "string_arrays");
  CHECK(exitCode == 1);
}
#endif
