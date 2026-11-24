// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-29 step 1 (D-004): ownership annotations `owning` / `borrowed` /
// `shared` on `native fun` and `extern "C"` parameters.
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

  [[nodiscard]] auto emitsSsa(std::string_view source) -> std::string
  {
    std::ostringstream output;
    std::ostringstream errors;
    REQUIRE(NG::runDriver({"--emit=ssa", "--source", source}, output, errors) == 0);
    return output.str();
  }
} // namespace

TEST_CASE("vNext ownership annotations parse on native and extern declarations", "[vNext][CABI][Ownership]")
{
  check("native fun f(a: owning i64, b: borrowed i64, c: shared i64) -> i64;");
  check("extern \"C\" fun g(a: owning i64, b: borrowed i64, c: shared i64) -> i64;");
  // The parsed syntax carries the annotation through to HIR.
  const auto module = resolve("native fun f(a: owning i64) -> i64;");
  REQUIRE(module.functions.front().parameters.front().ownership == syntax::ParameterOwnership::Owning);
}

TEST_CASE("vNext ownership annotations are rejected on ordinary functions", "[vNext][CABI][Ownership]")
{
  for (const auto *annotation : {"owning", "borrowed", "shared"})
  {
    const auto source = std::format("fun g(value: {} i64) -> i64 {{ return value; }}", annotation);
    try
    {
      check(source);
      FAIL("expected an ownership-annotation error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()}.find("is only allowed on `native fun` and `extern \"C\"` declarations") !=
              std::string::npos);
    }
  }
}

TEST_CASE("vNext owning rejects reference parameters", "[vNext][CABI][Ownership]")
{
  try
  {
    check("type H = native;\nnative fun f(handle: owning H ref) -> i64;");
    FAIL("expected an owning-by-value error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "`owning` parameter `handle` of `f` requires a by-value type");
  }
  // borrowed/shared on references are legal (view / descriptor-driven sharing).
  check("type H = native;\nnative fun f(handle: borrowed H ref) -> i64;");
}

TEST_CASE("vNext borrowed native parameter skips the deep clone at the call", "[vNext][CABI][Ownership]")
{
  const auto borrowed = emitsSsa("native fun sum(values: borrowed array<i64>) -> i64;\n"
                                 "fun main() -> i64 { let xs: array<i64> = [1, 2, 3]; return sum(xs); }");
  const auto copied = emitsSsa("native fun sum(values: array<i64>) -> i64;\n"
                               "fun main() -> i64 { let xs: array<i64> = [1, 2, 3]; return sum(xs); }");
  // Count only the clones emitted before the call to the native shim.
  const auto clonesBeforeCall = [](const std::string &il)
  {
    const auto call = il.find("call $ngrt_sum(");
    size_t count = 0;
    for (size_t pos = 0; (pos = il.find("call $ngrt_arr_clone_words(", pos)) != std::string::npos && pos < call;)
    {
      ++count;
      pos += std::string{"call $ngrt_arr_clone_words("}.size();
    }
    return count;
  };
  // The default (copy-first) declaration deep-copies the aggregate at the
  // call site; `borrowed` passes the view directly. The array literal's own
  // bind clone is common to both and appears before either call.
  REQUIRE(clonesBeforeCall(copied) == clonesBeforeCall(borrowed) + 1);
}

TEST_CASE("vNext ownership annotations keep extern ABI safety rules", "[vNext][CABI][Ownership]")
{
  std::string output;
  std::string errors;
  // An annotation does not make a non-ABI-safe type crossing extern "C" legal.
  REQUIRE(run("extern \"C\" fun f(value: owning string) -> i64;", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("not ABI-safe"));
}
