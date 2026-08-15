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
      {"--emit=ssa", "--source",
       "struct Counter { value: i64 } trait Show { fun show(self: Self ref) -> i64; } impl Show for Counter { fun show(self: Self ref) -> i64 { return (*self).value; } } fun main() -> i64 { let c = Counter { value: 5 }; let view: ref<Show> = c; 7 }"},
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

TEST_CASE("vNext native lowering round-trips structs through qbe and the system toolchain", "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("struct Point { x: i64, y: i64 }\n"
                                 "fun main() -> i64 {\n"
                                 "    let p = Point { x: 3, y: 4 };\n"
                                 "    return p.x + p.y;\n"
                                 "}",
                                 "structs");
  CHECK(exitCode == 7);
}

TEST_CASE("vNext native lowering round-trips struct field mutation through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("struct Point { x: i64, y: i64 }\n"
                                 "fun main() -> i64 {\n"
                                 "    let mut p = Point { x: 3, y: 4 };\n"
                                 "    p.x := 10;\n"
                                 "    return p.x + p.y;\n"
                                 "}",
                                 "struct_mutation");
  CHECK(exitCode == 14);
}

TEST_CASE("vNext native lowering round-trips mutable references through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("fun swap<T>(a: T ref mut, b: T ref mut) {\n"
                                 "    let tmp = *a;\n"
                                 "    *a := *b;\n"
                                 "    *b := tmp;\n"
                                 "}\n"
                                 "fun main() -> i64 {\n"
                                 "    let mut x = 1;\n"
                                 "    let mut y = 2;\n"
                                 "    swap(ref mut x, ref mut y);\n"
                                 "    return x * 10 + y;\n"
                                 "}",
                                 "ref_swap");
  CHECK(exitCode == 21);
}

TEST_CASE("vNext native lowering round-trips field references through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("struct Box {\n"
                                 "    value: i64,\n"
                                 "}\n"
                                 "fun set_to(target: i64 ref mut, value: i64) {\n"
                                 "    *target := value;\n"
                                 "}\n"
                                 "fun main() -> i64 {\n"
                                 "    let mut n = 10;\n"
                                 "    let mut nr = ref mut n;\n"
                                 "    set_to(nr, 11);\n"
                                 "    if (n != 11) { return 0; }\n"
                                 "    let mut box = Box { value: 20 };\n"
                                 "    let field = ref mut box.value;\n"
                                 "    set_to(field, 21);\n"
                                 "    if (box.value != 21) { return 0; }\n"
                                 "    return n + box.value;\n"
                                 "}",
                                 "field_refs");
  CHECK(exitCode == 32);
}

TEST_CASE("vNext native lowering round-trips enums and switches through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("enum Shape { Circle(radius: i64), Square(side: i64), Empty }\n"
                                 "fun main() -> i64 {\n"
                                 "    let shape: Shape = Shape.Circle(7);\n"
                                 "    let mut total = 0;\n"
                                 "    switch (shape) {\n"
                                 "        case Circle(radius) { total := radius; }\n"
                                 "        case Square(side) { total := side; }\n"
                                 "        case Empty { total := 5; }\n"
                                 "    }\n"
                                 "    return total;\n"
                                 "}",
                                 "enums");
  CHECK(exitCode == 7);
}

TEST_CASE("vNext native lowering round-trips multi-field enum variants through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("enum Pair { Both(a: i64, b: i64), None }\n"
                                 "fun main() -> i64 {\n"
                                 "    let p: Pair = Pair.Both(3, 4);\n"
                                 "    switch (p) {\n"
                                 "        case Both(a, b) { return a + b; }\n"
                                 "        case None { return 0; }\n"
                                 "    }\n"
                                 "}",
                                 "enum_multi_field");
  CHECK(exitCode == 7);
}

TEST_CASE("vNext native lowering round-trips recursive enum switches through qbe and the system toolchain",
          "[vNext][Native][Qbe]")
{
  const int exitCode = runNative("enum List<T> { Nil, Cons(head: T, tail: ref<List<T>>) }\n"
                                 "fun length<T>(list: ref<List<T>>) -> i64 {\n"
                                 "    switch (*list) {\n"
                                 "        case Nil { return 0; }\n"
                                 "        case Cons(value, rest) { return 1 + length(rest); }\n"
                                 "    }\n"
                                 "}\n"
                                 "fun main() -> i64 {\n"
                                 "    let tail: List<i64> = List.Nil;\n"
                                 "    let mid: List<i64> = List.Cons(2, ref tail);\n"
                                 "    let head: List<i64> = List.Cons(1, ref mid);\n"
                                 "    return length(ref head);\n"
                                 "}",
                                 "recursive_enum_switch");
  CHECK(exitCode == 2);
}
#endif
