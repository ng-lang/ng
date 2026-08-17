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

TEST_CASE("vNext module parser builds where clauses on function declarations", "[vNext][Where][Syntax]")
{
  const auto unit = syntax::parseSourceUnit("const is_box<T>: bool = false; "
                                            "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; } "
                                            "fun exact<T>(value: T) -> i64 where T is i64 => 1;");
  REQUIRE(unit.items.size() == 3);
  const auto &describe = *static_cast<const syntax::FunctionDeclaration *>(unit.items[1].get());
  REQUIRE(describe.whereClause != nullptr);
  REQUIRE(describe.whereClause->kind == syntax::ExpressionKind::GenericApplication);
  const auto &exact = *static_cast<const syntax::FunctionDeclaration *>(unit.items[2].get());
  REQUIRE(exact.whereClause != nullptr);
  REQUIRE(exact.whereClause->kind == syntax::ExpressionKind::TypeTest);
}

TEST_CASE("vNext resolver lowers where clause constraints into the HIR function", "[vNext][Where][Hir]")
{
  const auto module = resolve("const is_box<T>: bool = false; "
                              "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; }");
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions.front().whereClause != nullptr);
  REQUIRE(module.functions.front().whereClause->kind == hir::ExpressionKind::GenericApplication);
  REQUIRE(module.functions.front().whereClause->text == "is_box");
}

TEST_CASE("vNext where clauses accept and reject calls by predicate satisfaction", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const is_box<T>: bool = false; struct Box { value: i64 } const<T> is_box<Box>: bool = true; "
              "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; } "
              "fun main() -> i64 { let box = Box { value: 1 }; return describe(box); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 1") != std::string::npos);

  try
  {
    check("const is_box<T>: bool = false; struct Box { value: i64 } const<T> is_box<Box>: bool = true; "
          "fun describe<T>(value: T) -> i64 where is_box<T> { return 1; } "
          "fun main() -> i64 { let plain = 7; return describe(plain); }");
    FAIL("expected an unsatisfied where clause error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call to `describe` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses support negation and direct type patterns", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const is_box<T>: bool = false; "
              "fun describe_other<T>(value: T) -> i64 where !is_box<T> { return 2; } "
              "fun main() -> i64 { let plain = 7; return describe_other(plain); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 2") != std::string::npos);

  REQUIRE(run("fun exact<T>(value: T) -> i64 where T is i64 { return 16; } "
              "fun main() -> i64 { let plain = 7; return exact(plain); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 16") != std::string::npos);

  try
  {
    check("fun exact<T>(value: T) -> i64 where T is i64 { return 1; } "
          "fun main() -> i64 { let name = \"hi\"; return exact(name); }");
    FAIL("expected an unsatisfied type pattern error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call to `exact` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses call const fun over const parameters", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("const fun is_large(value: i64) -> bool => value > 10; "
              "fun require_large<const N: i64>() -> i64 where is_large(N) { return 8; } "
              "fun main() -> i64 { return require_large<42>(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 8") != std::string::npos);

  try
  {
    check("const fun is_large(value: i64) -> bool => value > 10; "
          "fun require_large<const N: i64>() -> i64 where is_large(N) { return 8; } "
          "fun main() -> i64 { return require_large<3>(); }");
    FAIL("expected an unsatisfied const fun where clause");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call to `require_large` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses are checked per instance and at module level", "[vNext][Where][Typecheck]")
{
  // Abstract calls defer the where-clause check to the monomorphized
  // instance, where the bindings are concrete.
  {
    std::string output;
    std::string errors;
    REQUIRE(run("fun exact<T>(value: T) -> i64 where T is i64 { return 1; } "
                "fun wrap<T>(x: T) -> i64 { return exact(x); } fun main() -> i64 { return wrap(1); }",
                output, errors) == 0);
    REQUIRE(errors.empty());
    REQUIRE(output.find("native main exited with code 1") != std::string::npos);
  }

  {
    std::string output;
    std::string errors;
    REQUIRE(run("fun exact<T>(value: T) -> i64 where T is i64 { return 1; } "
                "fun wrap<T>(x: T) -> i64 { return exact(x); } fun main() -> i64 { return wrap(\"no\"); }",
                output, errors) == 1);
    REQUIRE_THAT(errors, ContainsSubstring("does not satisfy its where clause"));
  }

  try
  {
    check("const is_box<T>: bool = false; "
          "fun unused(x: i64) -> i64 where is_box<i64> { return 1; } "
          "fun main() -> i64 { return 0; }");
    FAIL("expected a module-level where clause error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "function `unused` does not satisfy its where clause");
  }
}

TEST_CASE("vNext where clauses example file runs end to end through ngi", "[vNext][Where][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/where_clauses.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 31") != std::string::npos);
}

TEST_CASE("vNext where clauses combine predicates with logical operators", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; "
              "const fun positive(n: i64) -> bool { return n > 0; } "
              "const fun small(n: i64) -> bool { return n < 10; } "
              "fun make<const N: i64>() -> unit where positive(N) && small(N) { } "
              "fun main() { make<3>(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; "
              "const fun positive(n: i64) -> bool { return n > 0; } "
              "const fun small(n: i64) -> bool { return n < 10; } "
              "fun make<const N: i64>() -> unit where positive(N) || small(N) { } "
              "fun main() { make<-5>(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; "
              "const fun positive(n: i64) -> bool { return n > 0; } "
              "fun make<const N: i64>() -> unit where !positive(N) { } "
              "fun main() { make<-5>(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("import prelude; "
              "const fun positive(n: i64) -> bool { return n > 0; } "
              "const fun small(n: i64) -> bool { return n < 10; } "
              "fun make<const N: i64>() -> unit where positive(N) && small(N) { } "
              "fun main() { make<11>(); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("does not satisfy its where clause"));
}

TEST_CASE("vNext where clauses reject non-bool and unknown predicates", "[vNext][Where][Typecheck]")
{
  try
  {
    check("const k<T>: i64 = 5; fun make<T>() -> unit where k<i64> { } fun main() { make<i64>(); }");
    FAIL("expected a non-bool predicate error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const declaration `k` must evaluate to bool in predicate position");
  }

  try
  {
    check("fun f<T>() -> unit where Q is i64 { } fun main() { f<i64>(); }");
    FAIL("expected an unknown parameter error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "where clause tests unknown type parameter `Q`");
  }
}

TEST_CASE("vNext traits are rejected as value types", "[vNext][Where][Typecheck]")
{
  const auto expect = [](std::string_view source, std::string_view message)
  {
    try
    {
      check(source);
      FAIL("expected a trait value error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };
  expect("trait Show { fun show(self: Self ref) -> string; } fun f(x: Show) { } fun main() { }",
         "trait `Show` is not a value type; use `ref<Show>`");
  expect("trait Show { fun show(self: Self ref) -> string; } fun f() -> Show { } fun main() { }",
         "trait `Show` is not a value type; use `ref<Show>`");
}

TEST_CASE("vNext where clauses short-circuit logical operators", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  // `&&` short-circuits on the false first operand, so the unsatisfied
  // second predicate never evaluates.
  REQUIRE(run("import prelude; "
              "const fun positive(n: i64) -> bool { return n > 0; } "
              "const fun small(n: i64) -> bool { return n < 10; } "
              "fun make<const N: i64>() -> unit where positive(N) && small(N) { } "
              "fun main() { make<-5>(); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("does not satisfy its where clause"));

  // `||` short-circuits on the true first operand.
  REQUIRE(run("import prelude; "
              "const fun positive(n: i64) -> bool { return n > 0; } "
              "const fun small(n: i64) -> bool { return n < 10; } "
              "fun make<const N: i64>() -> unit where positive(N) || small(N) { } "
              "fun main() { make<3>(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext where clauses reject unsupported operators and non-bool predicates", "[vNext][Where][Typecheck]")
{
  const auto expect = [](std::string_view source, std::string_view message)
  {
    try
    {
      check(source);
      FAIL("expected a where clause error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };
  expect("fun make<const N: i64>() -> unit where N + 1 { } fun main() { make<1>(); }",
         "unsupported where clause operator `+`");
  expect("fun f() -> unit where tuple_size<tuple<i64, i64>> { } fun main() { f(); }",
         "const predicate `tuple_size` must evaluate to bool in predicate position");
}

TEST_CASE("vNext non-generic functions evaluate their where clauses", "[vNext][Where][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("trait Show { fun show(self: Self ref) -> string; } "
              "fun f() -> unit where is_trait<Show> { } "
              "fun main() { f(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());

  REQUIRE(run("const yes<T>: bool = true; fun f() -> unit where yes<i64> { } fun main() { f(); }", output, errors) ==
          0);
  REQUIRE(errors.empty());

  REQUIRE(run("const no<T>: bool = false; fun f() -> unit where no<i64> { } fun main() { f(); }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("function `f` does not satisfy its where clause"));
}

TEST_CASE("vNext where clauses validate traits and trait methods", "[vNext][Where][Typecheck]")
{
  const auto expect = [](std::string_view source, std::string_view message)
  {
    try
    {
      check(source);
      FAIL("expected a where clause error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };
  expect("trait Show { fun show(self: Self ref) -> string; } "
         "fun f<T>() -> unit where T: NoSuch { } fun main() { f<i64>(); }",
         "unknown trait `NoSuch` in where clause");
  expect("trait T { fun m(x: i64); } fun main() { }", "trait method `m` must take `self: Self ref`");
  expect("trait T { fun m(self: Self ref); } impl T for i64 { fun m(x: i64) { } } fun main() { }",
         "trait method `impl$T$m$i64` must take `self: Self ref`");
}
