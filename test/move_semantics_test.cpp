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

  [[nodiscard]] auto expectValue(std::string_view source, std::string_view value) -> void
  {
    std::string output;
    std::string errors;
    REQUIRE(run(source, output, errors) == 0);
    INFO("errors: " << errors);
    REQUIRE(errors.empty());
    REQUIRE(output.find(std::string{"native main exited with code "} + std::string{value}) != std::string::npos);
  }
} // namespace

TEST_CASE("vNext move transfers and assignment revives the source", "[vNext][Move][Runtime]")
{
  expectValue("struct P { x: i64 } "
              "fun main() -> i64 { let mut source = P { x: 1 }; let moved = move source; "
              "source := P { x: 3 }; return moved.x + source.x; }",
              "4");
}

TEST_CASE("vNext nominal bindings copy without aliasing", "[vNext][Move][Runtime]")
{
  expectValue("struct P { x: i64 } "
              "fun main() -> i64 { let source = P { x: 1 }; let copied = source; return copied.x; }",
              "1");
}

TEST_CASE("vNext partial moves revive fields by assignment", "[vNext][Move][Runtime]")
{
  expectValue("struct P { x: i64, y: i64 } "
              "fun main() -> i64 { let mut source = P { x: 1, y: 2 }; let f = source.x; source.x := 9; "
              "return source.x + source.y + f; }",
              "12");

  expectValue("struct P { x: i64, y: i64 } "
              "fun main() -> i64 { let source = P { x: 1, y: 2 }; let f = source.x; return f + source.y; }",
              "3");
}

TEST_CASE("vNext clone copies affine values without moving", "[vNext][Move][Runtime]")
{
  expectValue("struct P { x: i64 } "
              "fun main() -> i64 { let a = P { x: 1 }; let b = clone a; return a.x + b.x; }",
              "2");
}

TEST_CASE("vNext move checking rejects uses of moved values", "[vNext][Move][Errors]")
{
  try
  {
    check("struct P { x: i64 } fun main() { let a = P { x: 1 }; let b = a; return a.x; }");
    FAIL("expected a use-of-moved error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved value `a`");
    REQUIRE(error.code == "E-move-used");
  }

  try
  {
    check("fun main() { let a = 1; let b = move a; return a; }");
    FAIL("expected a moved scalar error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved value `a`");
    REQUIRE(error.code == "E-move-used");
  }
}

TEST_CASE("vNext move checking rejects partially moved whole values and moved fields", "[vNext][Move][Errors]")
{
  try
  {
    check("struct P { x: i64 } fun main() { let a = P { x: 1 }; let f = a.x; let g = a; return f + g.x; }");
    FAIL("expected a partially moved error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of partially moved value `a`");
    REQUIRE(error.code == "E-move-partial");
  }

  try
  {
    check("struct P { x: i64 } fun main() { let a = P { x: 1 }; let f = a.x; return a.x; }");
    FAIL("expected a moved field error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved field `a.x`");
    REQUIRE(error.code == "E-move-field");
  }
}

TEST_CASE("vNext move checking consumes by-value call arguments", "[vNext][Move][Errors]")
{
  try
  {
    check("struct P { x: i64 } "
          "fun consume(value: P) -> i64 { return value.x; } "
          "fun main() { let a = P { x: 1 }; let r = consume(a); return r + a.x; }");
    FAIL("expected a consumed argument error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved value `a`");
    REQUIRE(error.code == "E-move-used");
  }
}

TEST_CASE("vNext nested by-value calls consume an affine argument once", "[vNext][Move][Runtime]")
{
  // Regression: call arguments are inferred more than once (overload
  // scoring, spread analysis, expected-type checking), so the inner call's
  // move of `r` used to be re-reported as E-move-used.
  const std::string prelude = "struct R { v: i64 } fun take(r: R) -> R { return r; } "
                              "fun id(r: R) -> R { return r; } fun pick<T>(x: T) -> T { return x; } ";
  expectValue(prelude + "fun main() -> i64 { let r = R { v: 7 }; let s = id(take(r)); return s.v; }", "7");
  expectValue(prelude + "fun main() -> i64 { let r = R { v: 7 }; let s: R = id(pick(r)); return s.v; }", "7");
  expectValue(prelude + "fun main() -> i64 { let r = R { v: 7 }; return id(id(id(r))).v; }", "7");
}

TEST_CASE("vNext nested by-value calls still reject later and repeated uses", "[vNext][Move][Errors]")
{
  const std::string prelude = "struct R { v: i64 } fun take(r: R) -> R { return r; } "
                              "fun id(r: R) -> R { return r; } fun two(a: R, b: R) -> i64 { return a.v + b.v; } ";
  for (const std::string body :
       {"let r = R { v: 7 }; let s = id(take(r)); return r.v;", "let r = R { v: 7 }; return two(take(r), take(r));",
        "let r = R { v: 7 }; return two(r, id(r));"})
  {
    INFO(body);
    try
    {
      check(prelude + "fun main() -> i64 { " + body + " }");
      FAIL("expected a use-after-move error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == "use of moved value `r`");
      REQUIRE(error.code == "E-move-used");
    }
  }
}

TEST_CASE("vNext move checking merges branch states conservatively", "[vNext][Move][Errors]")
{
  try
  {
    check("struct P { x: i64 } "
          "fun main() -> i64 { let a = P { x: 1 }; let mut moved = false; "
          "if (moved) { let b = a; } return a.x; }");
    FAIL("expected a branch move error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved value `a`");
    REQUIRE(error.code == "E-move-used");
  }

  expectValue("struct P { x: i64 } "
              "fun main() -> i64 { let mut a = P { x: 1 }; let mut moved = false; "
              "if (moved) { let b = a; a := P { x: 5 }; } return a.x; }",
              "1");
}

TEST_CASE("vNext move semantics example file runs end to end through ngi", "[vNext][Move][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/move_semantics.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 42") != std::string::npos);
}

TEST_CASE("vNext bare move statements consume the moved local", "[vNext][Move][Errors]")
{
  // Regression: an expression statement `move s;` did not consume `s`.
  for (const std::string body :
       {"let s = P { x: 1 }; move s; let t = s; return t.x;", "let s = P { x: 1 }; s; let t = s; return t.x;",
        "let s = P { x: 1 }; move s; move s; return 0;"})
  {
    INFO(body);
    try
    {
      check("struct P { x: i64 } fun main() -> i64 { " + body + " }");
      FAIL("expected a use-after-move error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == "use of moved value `s`");
      REQUIRE(error.code == "E-move-used");
    }
  }
  expectValue("struct P { x: i64 } fun main() -> i64 { let mut s = P { x: 1 }; move s; s := P { x: 4 }; return s.x; }",
              "4");
}

TEST_CASE("vNext copy-capturing an affine value requires Clone", "[vNext][Move][Closure][Errors]")
{
  // B5: a copy capture is threaded into every call, so an affine captured value
  // must be deep-copied per call. Capturing one without Clone used to compile
  // and then abort with `invalid heap handle` on the first call.
  const char *prefix = "struct P { v: i64 } ";
  for (const std::string body : {"let p = P { v: 1 }; let f = [p] |x: i64| x + p.v; return f(1);",
                                 "let p = P { v: 1 }; let f = [p] |x: i64| x; return f(1);"})
  {
    INFO(body);
    try
    {
      check(std::string{prefix} + "fun main() -> i64 { " + body + " }");
      FAIL("expected a copy-capture Clone error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()}.find("does not implement Clone") != std::string::npos);
      REQUIRE(std::string{error.what()}.find("`P`") != std::string::npos);
    }
  }
}

TEST_CASE("vNext copy capture of a Clone affine value clones per call", "[vNext][Move][Closure][Runtime]")
{
  // The decided semantics: each call gets its own clone, the original stays
  // live, and it remains usable after the closure has been called.
  const char *cloneImpl = "struct P { v: i64 } "
                          "impl Clone for P { fun clone(self: Self ref) -> Self { return P { v: (*self).v }; } } ";

  std::string output;
  std::string errors;
  REQUIRE(run(std::string{"import prelude; "} + cloneImpl +
                  "fun main() -> i64 { let p = P { v: 10 }; let f = [p] |x: i64| x + p.v; return f(1) + f(2); }",
              output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 23") != std::string::npos); // 11 + 12

  // The captured original is still owned by the enclosing scope.
  expectValue(std::string{"import prelude; "} + cloneImpl +
                  "fun main() -> i64 { let p = P { v: 10 }; let f = [p] |x: i64| x + p.v; "
                  "let a = f(1); let b = f(2); return a + b + p.v; }",
              "33");
}

TEST_CASE("vNext move capture still consumes the enclosing local", "[vNext][Move][Closure]")
{
  // `[move p]` keeps working: the closure owns the value and later uses of the
  // enclosing local are rejected.
  try
  {
    check("struct P { v: i64 } fun main() -> i64 { let p = P { v: 1 }; let f = [move p] |x: i64| x; let q = p; "
          "return f(1) + q.v; }");
    FAIL("expected a use-after-move error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved value `p`");
    REQUIRE(error.code == "E-move-used");
  }

  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; struct P { v: i64 } "
              "fun main() -> i64 { let p = P { v: 5 }; let f = [move p] |x: i64| x + p.v; return f(1); }",
              output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("code 6") != std::string::npos);
}
