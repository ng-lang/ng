// AI-generated code; reviewed for this repository's vNext rewrite.
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"
#include "typecheck.hpp"

#include <filesystem>
#include <sstream>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  [[nodiscard]] auto resolve(std::string_view source) -> hir::Module
  {
    return hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
  }

  void check(std::string_view source)
  {
    static_cast<void>(typecheck::TypeChecker{}.check(resolve(source)));
  }

  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--source", source}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
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

TEST_CASE("vNext module parser accepts const fun declarations and expression bodies", "[vNext][ConstFun][Syntax]")
{
  const auto unit = syntax::parseSourceUnit("const fun is_large(value: i64) -> bool { return value > 10; } "
                                            "const fun positive(n: i64) -> bool => n > 0; "
                                            "fun double(x: i64) -> i64 => x * 2;");
  REQUIRE(unit.items.size() == 3);
  const auto &first = *static_cast<const syntax::FunctionDeclaration *>(unit.items[0].get());
  REQUIRE(first.constFunction);
  const auto &second = *static_cast<const syntax::FunctionDeclaration *>(unit.items[1].get());
  REQUIRE(second.constFunction);
  REQUIRE(second.body.statements.size() == 1);
  REQUIRE(dynamic_cast<const syntax::ReturnStatement *>(second.body.statements[0].get()) != nullptr);
  const auto &third = *static_cast<const syntax::FunctionDeclaration *>(unit.items[2].get());
  REQUIRE_FALSE(third.constFunction);
  REQUIRE(third.body.statements.size() == 1);
}

TEST_CASE("vNext const fun bodies execute at runtime like ordinary functions", "[vNext][ConstFun][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun fact(value: i64) -> i64 { if (value == 0) { return 1; } return value * fact(value - 1); } "
              "fun main() -> i64 { let input = 5; return fact(input); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 120") != std::string::npos);

  REQUIRE(run("fun double(x: i64) -> i64 => x * 2; fun main() -> i64 { return double(21); }", output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 42") != std::string::npos);
}

TEST_CASE("vNext const fun folds recursive calls inside const if conditions", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun fact(value: i64) -> i64 { if (value == 0) { return 1; } return value * fact(value - 1); } "
              "fun main() -> i64 { const if (fact(4) == 24) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);

  REQUIRE(run("const fun fact(value: i64) -> i64 { if (value == 0) { return 1; } return value * fact(value - 1); } "
              "fun main() -> i64 { const if (fact(4) == 25) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 0") != std::string::npos);
}

TEST_CASE("vNext const fun interpreter executes loops and tail recursion", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun sum_to(n: i64) -> i64 { let mut total = 0; loop (i = 0) { total := total + i; "
              "if (i == n) { return total; } next (i + 1); } } "
              "fun main() -> i64 { const if (sum_to(100) == 5050) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);

  REQUIRE(run("const fun countdown(n: i64) -> i64 { if (n == 0) { return 0; } next (n - 1); } "
              "fun main() -> i64 { const if (countdown(1000) == 0) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);
}

TEST_CASE("vNext const fun bodies may use const predicates and const if", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const is_ref<T>: bool = false; const<T> is_ref<ref<T>>: bool = true; "
              "const fun classify() -> i64 { const if (is_ref<ref<i64>>) { return 1; } return 0; } "
              "fun main() -> i64 { return classify(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);
}

TEST_CASE("vNext const evaluation rejects non-const functions, runtime locals, and generic const fun",
          "[vNext][ConstFun][Errors]")
{
  try
  {
    check("fun plain(n: i64) -> i64 { return n; } fun main() { const if (plain(1) == 1) { return; } }");
    FAIL("expected a non-const call error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "function `plain` is not const-capable");
  }

  try
  {
    check("const fun fact(n: i64) -> i64 { return n; } "
          "fun main() { let input = 3; const if (fact(input) == 3) { return; } }");
    FAIL("expected a runtime local error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "runtime local `input` is not a compile-time constant");
  }

  // Generic const funs evaluate at compile time: inferred arguments reuse
  // the runtime-instantiated body, explicit arguments instantiate per type.
  REQUIRE_NOTHROW(check("const fun identity<T>(value: T) -> T { return value; } "
                        "fun main() { const if (identity(1) == 1) { return; } }"));
}

TEST_CASE("vNext const evaluation enforces the fuel budget", "[vNext][ConstFun][Errors]")
{
  try
  {
    check("const fun spin() -> i64 { loop (i = 0) { next (i + 1); } } "
          "fun main() { const if (spin() == 0) { return; } }");
    FAIL("expected a fuel budget error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const evaluation exceeded the fuel budget");
  }
}

TEST_CASE("vNext const fun example file runs end to end through ngi", "[vNext][ConstFun][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/const_fun.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 32") != std::string::npos);
}

TEST_CASE("vNext const-capable natives fold inside const fun and const if", "[vNext][ConstFun][NativeHosts]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; "
              "const fun isShort(s: string) -> bool => length(s) < 5; "
              "fun main() { const if (isShort(\"abc\")) { print(\"short\"); } else { assert(false); } "
              "const if (regexMatch(\"a1b2\", \"a.b.\")) { print(\"matched\"); } else { assert(false); } }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("short\nmatched\n") != std::string::npos);
}

TEST_CASE("vNext const-capable natives evaluate in where clauses over const parameters",
          "[vNext][ConstFun][NativeHosts]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; "
              "const fun fits(value: i64) -> bool { return length(\"12345\") + value < 10; } "
              "fun make<const N: i64>() -> unit where fits(N) { } "
              "fun main() { make<2>(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; "
              "const fun fits(value: i64) -> bool { return length(\"12345\") + value < 10; } "
              "fun make<const N: i64>() -> unit where fits(N) { } "
              "fun main() { make<8>(); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("does not satisfy its where clause"));
}

TEST_CASE("vNext const-capable natives report compile-time bounds errors", "[vNext][ConstFun][NativeHosts]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; fun main() { const if (charAt(\"ab\", 5) == \"x\") { } }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("const charAt index out of bounds"));
}

TEST_CASE("vNext const string natives fold inside const if", "[vNext][ConstFun][NativeHosts]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; fun main() { const if (contains(\"abc\", \"b\") && startsWith(\"abc\", \"ab\") && "
              "endsWith(\"abc\", \"bc\") && toUpper(\"ab\") == \"AB\" && toLower(\"AB\") == \"ab\" && "
              "trim(\" x \") == \"x\" && replace(\"a-b-c\", \"-\", \"+\") == \"a+b+c\" && "
              "substring(\"abcdef\", 2, 4) == \"cd\" && charAt(\"abc\", 1) == \"b\") { print(\"folded\"); } }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("folded\n") != std::string::npos);
}

TEST_CASE("vNext const string natives report compile-time bounds and pattern errors", "[vNext][ConstFun][NativeHosts]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; fun main() { const if (substring(\"abc\", 0, 9) == \"x\") { } }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("const substring bounds out of range: [0..9) of length 3"));

  REQUIRE(run("import prelude; fun main() { const if (regexMatch(\"x\", \"[\") == true) { } }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("const regexMatch: invalid pattern"));
}

TEST_CASE("vNext impure natives stay rejected in const contexts", "[vNext][ConstFun][NativeHosts]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; const fun bad() -> bool { print(\"x\"); return true; } "
              "fun main() { const if (bad()) { } }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("native `print` is not const-capable"));
}

TEST_CASE("vNext const_native_hosts example runs end to end through ngi", "[vNext][ConstFun][NativeHosts][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/const_native_hosts.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("short folded\nregex folded\ntrim folded\n") != std::string::npos);
}

TEST_CASE("vNext generic const funs evaluate at compile time per type argument", "[vNext][ConstFun][GenericCalls]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; "
              "trait Show { fun show(self: Self ref) -> string; } "
              "impl Show for i64 { fun show(self: Self ref) -> string { return \"int\"; } } "
              "const fun is_showable<T>() -> bool where T: Show { return true; } "
              "fun main() { const if (is_showable<i64>()) { print(\"yes\"); } else { assert(false); } }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("yes\n") != std::string::npos);

  REQUIRE(run("import prelude; "
              "const fun identity<T>(value: T) -> T { return value; } "
              "fun main() { const if (identity(7) == 7) { print(\"folded\"); } else { assert(false); } }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("folded\n") != std::string::npos);

  REQUIRE(run("import prelude; "
              "trait Show { fun show(self: Self ref) -> string; } "
              "const fun is_showable<T>() -> bool where T: Show { return true; } "
              "fun main() { const if (is_showable<string>()) { assert(false); } }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("does not satisfy its where clause"));
}

TEST_CASE("vNext generic const funs defer abstract calls to instances", "[vNext][ConstFun][GenericCalls]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; "
              "trait Show { fun show(self: Self ref) -> string; } "
              "impl Show for i64 { fun show(self: Self ref) -> string { return \"int\"; } } "
              "const fun is_showable<T>() -> bool where T: Show { return true; } "
              "fun classify<U: Show>() -> i64 { const if (is_showable<U>()) { return 1; } return 0; } "
              "fun main() { assert(classify<i64>() == 1); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext generic_const_fun example runs end to end through ngi", "[vNext][ConstFun][GenericCalls][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/generic_const_fun.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("i64 showable\nidentity folded\n") != std::string::npos);
}

TEST_CASE("vNext const fun interpreter guards arithmetic overflow at compile time", "[vNext][ConstFun][ConstEval]")
{
  try
  {
    check("const fun add(a: i64, b: i64) -> i64 { return a + b; } "
          "fun main() { const if (add(9223372036854775807, 1) == 0) { } }");
    FAIL("expected an addition overflow error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const integer addition overflow");
  }

  try
  {
    check("const fun sub(a: i64, b: i64) -> i64 { return a - b; } "
          "fun main() { const if (sub(-9223372036854775808, 1) == 0) { } }");
    FAIL("expected a subtraction overflow error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const integer subtraction overflow");
  }

  try
  {
    check("const fun mul(a: i64, b: i64) -> i64 { return a * b; } "
          "fun main() { const if (mul(9223372036854775807, 2) == 0) { } }");
    FAIL("expected a multiplication overflow error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const integer multiplication overflow");
  }
}

TEST_CASE("vNext const fun interpreter guards division and remainder edge cases", "[vNext][ConstFun][ConstEval]")
{
  const auto expect = [](std::string_view source, std::string_view message)
  {
    try
    {
      check(source);
      FAIL("expected an interpreter error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };
  expect("const fun f(a: i64, b: i64) -> i64 { return a / b; } "
         "fun main() { const if (f(1, 0) == 0) { } }",
         "const integer division by zero");
  expect("const fun f(a: i64, b: i64) -> i64 { return a % b; } "
         "fun main() { const if (f(1, 0) == 0) { } }",
         "const integer modulo by zero");
  expect("const fun f(a: i64, b: i64) -> i64 { return a / b; } "
         "fun main() { const if (f(-9223372036854775807 - 1, -1) == 0) { } }",
         "const integer division overflow");
  expect("const fun f(a: i64, b: i64) -> i64 { return a % b; } "
         "fun main() { const if (f(-9223372036854775807 - 1, -1) == 0) { } }",
         "const integer remainder overflow");
  expect("const fun neg(n: i64) -> i64 { return -n; } "
         "fun main() { const if (neg(-9223372036854775807 - 1) == 0) { } }",
         "const integer negation overflow");
}

TEST_CASE("vNext const fun interpreter supports prefix and logical operators", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; "
              "const fun id(n: i64) -> i64 { return +n; } "
              "const fun negate(b: bool) -> bool { return !b; } "
              "const fun andShort() -> bool { return false && (1 / 0 == 0); } "
              "const fun orShort() -> bool { return true || (1 / 0 == 0); } "
              "fun main() { const if (id(5) == 5 && negate(true) == false && andShort() == false && orShort() == true) "
              "{ print(\"ops-ok\"); } }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("ops-ok\n") != std::string::npos);
}

TEST_CASE("vNext const fun interpreter handles if fallthrough, tail expressions, and comparisons",
          "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; "
              "const fun noElse(n: i64) -> i64 { if (n > 0) { return 1; } return 0; } "
              "const fun fallthrough(n: i64) -> i64 { let mut m = n; if (m > 10) { m := m - 10; } return m; } "
              "const fun tailExpr() -> i64 => 5; "
              "const fun sameText(a: string, b: string) -> bool { return a == b; } "
              "const fun ordered(a: i64, b: i64) -> bool { return a < b && a <= b && b > a && b >= a; } "
              "fun main() { const if (noElse(0) == 0 && fallthrough(5) == 5 && tailExpr() == 5 && "
              "sameText(\"a\", \"a\") && ordered(1, 2)) { print(\"blocks-ok\"); } }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("blocks-ok\n") != std::string::npos);
}

TEST_CASE("vNext const fun interpreter rejects unsupported constructs at compile time", "[vNext][ConstFun][ConstEval]")
{
  const auto expect = [](std::string_view source, std::string_view message)
  {
    try
    {
      check(source);
      FAIL("expected an interpreter error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };
  expect("const fun f() -> i64 { let (a, b) = (1, 2); return a; } "
         "fun main() { const if (f() == 0) { } }",
         "tuple destructuring is not supported during const evaluation");
  expect("const fun f() -> i64 { let xs = [1, 2]; return 1; } "
         "fun main() { const if (f() == 0) { } }",
         "expression is not supported during const evaluation");
  expect("const fun f(n: i64) -> i64 { switch (n) { case 0 { return 0; } otherwise { } } return 1; } "
         "fun main() { const if (f(0) == 0) { } }",
         "switch is not supported during const evaluation");
  expect("struct P { x: i64 } const fun f() -> i64 { let mut p = P { x: 1 }; p.x := 5; return p.x; } "
         "fun main() { const if (f() == 0) { } }",
         "expression is not supported during const evaluation");
  expect("const fun f() -> i64 { let mut t = (1, 2); t.0 := 5; return t.0; } "
         "fun main() { const if (f() == 0) { } }",
         "expression is not supported during const evaluation");
}

TEST_CASE("vNext const fun interpreter enforces the call depth limit", "[vNext][ConstFun][ConstEval]")
{
  try
  {
    check("const fun rec(n: i64) -> i64 { return rec(n); } "
          "fun main() { const if (rec(0) == 0) { } }");
    FAIL("expected a call depth error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const call depth exceeded in `rec`");
  }
}

TEST_CASE("vNext const fun bodies fold const if and i64::min literals at compile time", "[vNext][ConstFun][ConstEval]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const is_ref<T>: bool = false; const<T> is_ref<ref<T>>: bool = true; "
              "const fun classify() -> i64 { const if (is_ref<ref<i64>>) { return 1; } return 0; } "
              "fun main() -> i64 { const if (classify() == 1) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);

  REQUIRE(run("const fun minv() -> i64 { return -9223372036854775808; } "
              "fun main() -> i64 { const if (minv() == -9223372036854775808) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);
}
