// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-29 step 2 (D-004): `T?` nullable foreign handles.
//
// `T?` is only valid on `type T = native;` handles and occupies the same
// 64-bit slot as `T` (zero bit pattern = null). `null` is a literal that
// adopts an expected nullable type; `T` lifts implicitly to `T?`.
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

  [[nodiscard]] auto emitsSsa(std::string_view source, std::string &errors) -> std::string
  {
    std::ostringstream output;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--emit=ssa", "--source", source}, output, errorStream);
    errors = std::move(errorStream).str();
    REQUIRE(status == 0);
    return output.str();
  }
} // namespace

TEST_CASE("vNext nullable handle parses and null compares by the zero sentinel", "[vNext][CABI][Nullable]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("type H = native;\n"
              "fun main() -> i64 { let h: H? = null; if (h == null) { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 1\n") != std::string::npos);
  // `!=` against null is also accepted (typed as the nullable handle).
  std::string ignoredOutput;
  std::string ignoredErrors;
  REQUIRE(run("type H = native;\n"
              "fun main() -> i64 { let h: H? = null; if (h != null) { return 1; } return 0; }",
              ignoredOutput, ignoredErrors) == 0);
}

TEST_CASE("vNext nullable handles reject non-handle element types", "[vNext][CABI][Nullable]")
{
  // Previously `i64?` was silently collapsed to `i64`; now it is rejected.
  try
  {
    check("fun main() -> i64 { let x: i64? = null; return 0; }");
    FAIL("expected a non-handle nullable error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "`i64` is not a foreign handle; `T?` requires `type T = native;`");
  }
  // Abstract types have no representation, so they cannot be nullable handles.
  try
  {
    check("type A;\nfun main() -> i64 { let x: A? = null; return 0; }");
    FAIL("expected an abstract nullable error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("not a foreign handle") != std::string::npos);
  }
}

TEST_CASE("vNext null requires a nullable context and supports equality only", "[vNext][CABI][Nullable]")
{
  try
  {
    check("fun main() -> i64 { let x: i64 = null; return 0; }");
    FAIL("expected a nullable-context error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("cannot initialize a value of type i64") != std::string::npos);
  }
  try
  {
    check("type H = native;\nfun main() -> i64 { let h: H? = null; if (h < null) { return 1; } return 0; }");
    FAIL("expected an ordering-on-null error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "`null` supports only `==` and `!=`");
  }
  try
  {
    check("fun main() -> i64 { if (null == null) { return 1; } return 0; }");
    FAIL("expected an untyped-null error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()}.find("expected nullable handle type") != std::string::npos);
  }
}

TEST_CASE("vNext nullable handles lift a plain handle", "[vNext][CABI][Nullable]")
{
  // The checker accepts the implicit lift from H to H? in a call argument.
  check("type H = native;\n"
        "native fun make() -> H;\n"
        "fun takes(h: H?) -> i64 { if (h == null) { return 1; } return 0; }\n"
        "fun main() -> i64 { let h: H = make(); return takes(h); }");
}

TEST_CASE("vNext nullable handles lower to zero and compare with ceql", "[vNext][CABI][Nullable]")
{
  std::string errors;
  const auto il = emitsSsa("type H = native;\n"
                           "fun main() -> i64 { let h: H? = null; if (h == null) { return 1; } return 0; }",
                           errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(il, ContainsSubstring("copy 0"));
  REQUIRE_THAT(il, ContainsSubstring("ceq"));
}

TEST_CASE("vNext nullable handles cross the C ABI", "[vNext][CABI][Nullable]")
{
  std::string errors;
  const auto il = emitsSsa("type H = native;\n"
                           "extern \"C\" fun f(h: H?) -> i64;\n"
                           "fun main() -> i64 { let h: H? = null; return f(h); }",
                           errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(il, ContainsSubstring("call $f("));
}

TEST_CASE("vNext stray type tokens are rejected", "[vNext][CABI][Nullable]")
{
  // Regression guard for the silent `T?` drop: any unparsed type token now
  // fails loudly instead of being ignored.
  std::string output;
  std::string errors;
  REQUIRE(run("fun main() -> i64 { let x: i64 nonsense = 1; return 0; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("unexpected token"));
}
