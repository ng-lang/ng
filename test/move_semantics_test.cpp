// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
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
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
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
  }

  try
  {
    check("fun main() { let a = 1; let b = move a; return a; }");
    FAIL("expected a moved scalar error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved value `a`");
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
  }

  try
  {
    check("struct P { x: i64 } fun main() { let a = P { x: 1 }; let f = a.x; return a.x; }");
    FAIL("expected a moved field error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "use of moved field `a.x`");
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
