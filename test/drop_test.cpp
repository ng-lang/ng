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
} // namespace

TEST_CASE("vNext Drop impls run when a value's scope ends at return", "[vNext][Drop][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun print(value: string) -> unit; "
              "struct Resource { id: i64 } "
              "impl Drop for Resource { fun drop(self: Self ref) -> unit { print(\"dropped\"); } } "
              "fun make() -> i64 { let resource = Resource { id: 1 }; return 42; } "
              "fun main() { let r = make(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("dropped\n") != std::string::npos);
}

TEST_CASE("vNext Drop runs once per live value at function fall-through", "[vNext][Drop][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun print(value: string) -> unit; "
              "struct Resource { id: i64 } "
              "impl Drop for Resource { fun drop(self: Self ref) -> unit { print(\"dropped\"); } } "
              "fun main() { let one = Resource { id: 1 }; let two = Resource { id: 2 }; return; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("dropped\ndropped\n") != std::string::npos);
}

TEST_CASE("vNext moved-to bindings are dropped and moved-from bindings are not", "[vNext][Drop][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun print(value: string) -> unit; "
              "struct Resource { id: i64 } "
              "impl Drop for Resource { fun drop(self: Self ref) -> unit { print(\"dropped\"); } } "
              "fun keep() -> i64 { let resource = Resource { id: 1 }; let moved = move resource; return moved.id; } "
              "fun main() { let r = keep(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("dropped\n") != std::string::npos);
  REQUIRE(output.find("dropped\ndropped\n") == std::string::npos);
}

TEST_CASE("vNext Drop impls validate their shape", "[vNext][Drop][Errors]")
{
  try
  {
    check("struct Resource { id: i64 } "
          "impl Drop for Resource { fun cleanup(self: Self ref) -> unit { return; } }");
    FAIL("expected a malformed Drop impl error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "Drop impl must define exactly one `drop` method");
  }
}

TEST_CASE("vNext Drop runs when a nested block scope exits", "[vNext][Drop][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun print(value: string) -> unit; "
              "struct Resource { id: i64 } "
              "impl Drop for Resource { fun drop(self: Self ref) -> unit { print(\"dropped\"); } } "
              "fun main() { if (1 < 2) { let inner = Resource { id: 1 }; } return; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("dropped\n") != std::string::npos);
}

TEST_CASE("vNext Drop runs per loop iteration on next edges", "[vNext][Drop][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun print(value: string) -> unit; "
              "struct Resource { id: i64 } "
              "impl Drop for Resource { fun drop(self: Self ref) -> unit { print(\"dropped\"); } } "
              "fun main() { loop (i = 0) { let each = Resource { id: i }; if (i == 1) { return; } next (i + 1); } }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("dropped\ndropped\n") != std::string::npos);
}

TEST_CASE("vNext block-scoped drops skip wholly moved locals", "[vNext][Drop][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("native fun print(value: string) -> unit; "
              "struct Resource { id: i64 } "
              "impl Drop for Resource { fun drop(self: Self ref) -> unit { print(\"dropped\"); } } "
              "fun main() { if (1 < 2) { let resource = Resource { id: 1 }; let moved = move resource; } return; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("dropped\n") != std::string::npos);
  REQUIRE(output.find("dropped\ndropped\n") == std::string::npos);
}

TEST_CASE("vNext Drop edges validate field moves against the Drop impl contract", "[vNext][Drop][Errors]")
{
  try
  {
    check("struct Inner { value: i64 } impl Drop for Inner { fun drop(self: Self ref) -> unit { return; } } "
          "struct Resource { owned: Inner, id: i64 } "
          "impl Drop for Resource { fun drop(self: Self ref) -> unit { let taken = move (*self).owned; } } "
          "fun main() -> i64 { let resource = Resource { owned: Inner { value: 1 }, id: 7 }; "
          "let moved = move resource.owned; return moved.value; }");
    FAIL("expected a field-aware drop conflict error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot drop a value with field `owned` moved out");
  }

  // Moving a different field keeps the destructor's own field moves valid.
  REQUIRE_NOTHROW(check("struct Inner { value: i64 } impl Drop for Inner { fun drop(self: Self ref) -> unit { return; } } "
                        "struct Resource { owned: Inner, id: i64 } "
                        "impl Drop for Resource { fun drop(self: Self ref) -> unit { let taken = move (*self).owned; } } "
                        "fun main() -> i64 { let resource = Resource { owned: Inner { value: 1 }, id: 7 }; "
                        "let moved = move resource.id; return moved; }"));
}

TEST_CASE("vNext drop example file runs end to end through ngi", "[vNext][Drop][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/drop_raii.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("dropped\ndropped\ndropped\ndropped\n") != std::string::npos);
}
