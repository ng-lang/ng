// AI-generated code; reviewed for this repository's vNext rewrite.
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

  void expectTypeError(std::string_view source, std::string_view message)
  {
    try
    {
      check(source);
      FAIL("expected a type error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  }
} // namespace

TEST_CASE("vNext type interner rejects unknown type names", "[vNext][TypeInterner][Errors]")
{
  expectTypeError("fun main() { let x: NoSuch = 1; }", "unknown type `NoSuch`");
}

TEST_CASE("vNext type interner validates builtin type constructor arities", "[vNext][TypeInterner][Errors]")
{
  expectTypeError("fun main() { let x: array<i64, i64, i64> = [1, 2]; }", "array type expects 1 or 2 arguments, got 3");
  expectTypeError("fun main() { let r: range<i64, i64> = 1..2; }", "range type expects 1 element argument, got 2");
}

TEST_CASE("vNext type interner validates struct enum and opaque type arities", "[vNext][TypeInterner][Errors]")
{
  expectTypeError("struct P<T> { x: T } fun main() { let p: P<i64, i64> = P { x: 1 }; }",
                  "struct type `P` expects 1 arguments, got 2");
  expectTypeError("enum E<T> { A(T) } fun main() { let e: E<i64, i64> = E.A(1); }",
                  "enum type `E` expects 1 arguments, got 2");
  expectTypeError("type Pair<T> = native; fun f(p: Pair<i64, i64>) { } fun main() { }",
                  "opaque type `Pair` expects 1 arguments, got 2");
}

TEST_CASE("vNext tuple introspection reports non-tuple type arguments", "[vNext][TypeInterner][Errors]")
{
  expectTypeError("fun main() { let x: tuple_element<i64, 0> = 1; }",
                  "tuple_element<T, I> expects a tuple type as T, got i64");
}

TEST_CASE("vNext generic instantiations intern range fields and struct arguments", "[vNext][TypeInterner][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("struct R<T> { r: range<T> } "
              "fun main() -> i64 { let x: R<i64> = R { r: 1..2 }; return 1; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);

  REQUIRE(run("struct P<T> { x: T } fun get<T>(p: P<T>) -> T { return p.x; } "
              "fun main() -> i64 { let p: P<i64> = P { x: 7 }; return get<i64>(p); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 7") != std::string::npos);
}

TEST_CASE("vNext generic instantiations intern nested struct and fixed array fields", "[vNext][TypeInterner][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("struct P<T> { x: T } struct W<T> { inner: P<T> } "
              "fun main() -> i64 { let p: P<i64> = P { x: 7 }; let w: W<i64> = W { inner: p }; return w.inner.x; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 7") != std::string::npos);

  REQUIRE(run("struct A<T> { xs: array<T, 2> } "
              "fun main() -> i64 { let a: A<i64> = A { xs: [1, 2] }; return a.xs[1]; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 2") != std::string::npos);
}

TEST_CASE("vNext variadic opaque templates accept pack arguments", "[vNext][TypeInterner][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("type Pack<T...> = native; fun f(p: Pack<i64, i64>) { } fun main() { }", output, errors) == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext tuple introspection validates arity", "[vNext][TypeInterner][Errors]")
{
  expectTypeError("fun main() { let x: tuple_element<i64> = 1; }",
                  "tuple_element<T, I> expects a tuple type and a const index");
  expectTypeError("fun main() { let x: tuple_concat<i64> = (1); }", "tuple_concat<A, B> expects two tuple types");
}
