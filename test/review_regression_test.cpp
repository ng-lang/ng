// AI-generated code; reviewed for this repository's vNext rewrite.
// PR #47 review regressions: findings verified against the current tree and
// fixed. Each case pins the previously broken behavior.
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"
#include "typecheck.hpp"

#include <sstream>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--source", source}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  [[nodiscard]] auto emitsSsa(std::string_view source, std::string &errors) -> std::string
  {
    std::ostringstream output;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--emit=ssa", "--source", source}, output, errorStream);
    errors = std::move(errorStream).str();
    REQUIRE(status == 0);
    return output.str();
  }

  [[nodiscard]] auto exitCode(std::string_view source, std::string &errors) -> int
  {
    std::string output;
    std::string errorStream;
    const int status = run(source, output, errorStream);
    errors = std::move(errorStream);
    REQUIRE(status == 0);
    const auto marker = output.find("native main exited with code ");
    REQUIRE(marker != std::string::npos);
    return std::stoi(output.substr(marker + std::string{"native main exited with code "}.size()));
  }
} // namespace

TEST_CASE("PR review: spread before an affine argument is tracked without OOB", "[vNext][Review][Spread]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory;\n"
              "fun tail(a: Box<i64>, b: Box<i64>, c: Box<i64>) -> i64 { return 0; }\n"
              "fun main() -> i64 {\n"
              "    let a = new(1); let b = new(2); let c = new(3);\n"
              "    let t = (b, c);\n"
              "    tail(t..., a);\n"
              "    let ar = ref a;\n"
              "    return read(ar);\n"
              "}",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("use of moved value `a`"));
}

TEST_CASE("PR review: derived Ord <= and >= are lexicographic, nested Ord recurses", "[vNext][Review][Ord]")
{
  std::string errors;
  REQUIRE(exitCode("import prelude;\n"
                   "struct P: derive(Ord) { x: i64, y: i64 }\n"
                   "fun main() -> i64 {\n"
                   "    let p = P { x: 1, y: 9 }; let q = P { x: 1, y: 5 };\n"
                   "    if (p <= q) { return 0; }\n"
                   "    if (p >= q) { return 1; }\n"
                   "    return 0;\n"
                   "}",
                   errors) == 1);
  REQUIRE(errors.empty());
  REQUIRE(exitCode("import prelude;\n"
                   "struct P: derive(Ord) { x: i64, y: i64 }\n"
                   "fun main() -> i64 {\n"
                   "    let p = P { x: 1, y: 9 }; let q = P { x: 1, y: 5 };\n"
                   "    if (p <= q) { return 1; }\n"
                   "    return 0;\n"
                   "}",
                   errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(exitCode("import prelude;\n"
                   "struct Inner: derive(Ord) { x: i64 }\n"
                   "struct Outer: derive(Ord) { i: Inner }\n"
                   "fun main() -> i64 {\n"
                   "    let a = Outer { i: Inner { x: 1 } }; let b = Outer { i: Inner { x: 2 } };\n"
                   "    if (a < b) { return 1; }\n"
                   "    return 0;\n"
                   "}",
                   errors) == 1);
  REQUIRE(errors.empty());
}

TEST_CASE("PR review: callback typing is scoped to the extern call slot", "[vNext][Review][Callback]")
{
  std::string output;
  std::string errors;
  // A normal NG higher-order call must still accept a closure even when a
  // callback-typed extern signature exists in the same module.
  REQUIRE(run("import prelude;\n"
              "struct S: derive(Show) { x: i64 }\n"
              "const fun hasShow<T>() -> bool where T: Show { return true; }\n"
              "extern \"C\" fun unused(cb: |i64| -> i64) -> i64;\n"
              "fun apply<T>(f: |i64| -> i64, v: i64) -> i64 { const if (hasShow<S>()) { return f(v); } return 0; }\n"
              "fun main() -> i64 { let c = |x: i64| x + 1; return apply(c, 41); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  // Declaring a native or extern function as a callback is rejected.
  REQUIRE(run("extern \"C\" fun ngrt_fixture_apply_callback(cb: |i64| -> i64, x: i64) -> i64;\n"
              "native fun nvm(a: i64) -> i64;\n"
              "fun main() -> i64 { return ngrt_fixture_apply_callback(nvm, 1); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("no function matches callback signature"));
}

TEST_CASE("PR review: C varargs emit the marker and default argument promotions", "[vNext][Review][Varargs]")
{
  std::string errors;
  const auto zero = emitsSsa("extern \"C\" fun f(fixed: i64, ...) -> i64;\n"
                             "fun main() -> i64 { return f(0); }",
                             errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(zero, ContainsSubstring("call $f(l %t0, ...)"));
  const auto promoted = emitsSsa("extern \"C\" fun f(fixed: i64, ...) -> i64;\n"
                                 "fun main() -> i64 { let x: f32 = 1.5f32; return f(0, x); }",
                                 errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(promoted, ContainsSubstring("..., d "));
}

TEST_CASE("PR review: C ABI thunks forward floats and emit void returns", "[vNext][Review][ExportC]")
{
  std::string errors;
  const auto il = emitsSsa("export extern \"C\" fun scale(a: i32, b: f32) -> f32 { return b; }\n"
                           "fun main() -> i64 { return 0; }",
                           errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(il, ContainsSubstring("(l %c0, d %c1)"));
  const auto nop = emitsSsa("export extern \"C\" fun nop(a: i64) -> unit { let x = a; return; }\n"
                            "fun main() -> i64 { return 0; }",
                            errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(nop, ContainsSubstring("export function $nop(l %p0)"));
  REQUIRE_THAT(nop, ContainsSubstring("ret\n"));
}

TEST_CASE("PR review: zero-step ref-mut assignment uses the field width", "[vNext][Review][AssignPlace]")
{
  std::string errors;
  REQUIRE(exitCode("struct Narrow { a: i8, b: i16, c: u8 }\n"
                   "fun check(s: Narrow) -> i64 {\n"
                   "    if (s.a == 100 && s.b == 2 && s.c == 3) { return 1; }\n"
                   "    return 0;\n"
                   "}\n"
                   "fun main() -> i64 {\n"
                   "    let mut s = Narrow { a: 1, b: 2, c: 3 };\n"
                   "    let mut r = ref mut s.a;\n"
                   "    *r := 100;\n"
                   "    return check(s);\n"
                   "}",
                   errors) == 1);
  REQUIRE(errors.empty());
}

TEST_CASE("PR review: nested ? unwraps in evaluation order", "[vNext][Review][TryOperator]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude;\n"
              "fun f(x: i64) -> Result<i64, string> { return Result.Ok(x + 1); }\n"
              "fun g() -> Result<i64, string> { return Result.Ok(1); }\n"
              "fun h() -> Result<i64, string> { let v = f(g()?)?; return Result.Ok(v); }\n"
              "fun main() -> unit {\n"
              "    switch (h()) { case Ok(n) { print(n); } case Err(e) { print(e); } }\n"
              "}",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("2\n") != std::string::npos);
}

TEST_CASE("PR review: ? in a short-circuit right operand is rejected", "[vNext][Review][TryOperator]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude;\n"
              "fun boom() -> Result<i64, string> { print(\"BOOM\"); return Result.Err(\"bad\"); }\n"
              "fun test() -> Result<i64, string> {\n"
              "    let c = false && boom()? == 1;\n"
              "    return Result.Ok(0);\n"
              "}\n"
              "fun main() -> unit { let r = test(); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("cannot appear mid-expression"));
  REQUIRE(output.find("BOOM") == std::string::npos);
}

TEST_CASE("PR review: trait view reassignment invalidates the Self origin", "[vNext][Review][TraitObjects]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("struct Point { x: i64 }\n"
              "struct Circle { r: i64 }\n"
              "trait Dup { fun dup(self: Self ref) -> Self; }\n"
              "impl Dup for Point { fun dup(self: Self ref) -> Self { return Point { x: (*self).x }; } }\n"
              "impl Dup for Circle { fun dup(self: Self ref) -> Self { return Circle { r: (*self).r }; } }\n"
              "fun main() -> i64 {\n"
              "    let mut p = Point { x: 1 };\n"
              "    let mut view: ref<Dup> = ref p;\n"
              "    let mut c = Circle { r: 9 };\n"
              "    view := ref c;\n"
              "    let d = view.dup();\n"
              "    if (d.x == 1) { return 1; }\n"
              "    return 0;\n"
              "}",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("without a known concrete type"));
}

TEST_CASE("PR review: tuple switch consumes an affine scrutinee", "[vNext][Review][TupleSwitch]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import memory;\n"
              "fun main() -> i64 {\n"
              "    let p = (new(1), new(2));\n"
              "    switch (p) { case (a, b) { return 0; } }\n"
              "    let q = p;\n"
              "    return 0;\n"
              "}",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("use of moved value `p`"));
}

TEST_CASE("PR review: empty struct emits a non-empty QBE aggregate", "[vNext][Review][EmptyStruct]")
{
  std::string errors;
  const auto il = emitsSsa("struct Empty {}\n"
                           "fun takes(e: Empty) -> i64 { return 1; }\n"
                           "fun main() -> i64 { let e = Empty {}; return takes(e); }",
                           errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(il, ContainsSubstring("{ b 1 }"));
}
