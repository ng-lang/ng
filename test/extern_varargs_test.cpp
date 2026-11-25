// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-29 step 3b (D-004): C varargs on `extern "C"` declarations.
#include "driver.hpp"
#include "test.hpp"

#include <sstream>

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
} // namespace

TEST_CASE("vNext extern C varargs emit a QBE variadic call", "[vNext][CABI][Varargs]")
{
  std::string errors;
  const auto il = emitsSsa("extern \"C\" fun f(fixed: i64, ...) -> i64;\n"
                           "fun main() -> i64 { return f(0, 1, 2); }",
                           errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(il, ContainsSubstring("call $f(l %t0, ..., l %t1, l %t2)"));
}

TEST_CASE("vNext extern C varargs pass typed values to a real C fixture", "[vNext][CABI][Varargs][Runtime]")
{
  // libngrt's fixture sums the trailing i64 arguments.
  std::string output;
  std::string errors;
  REQUIRE(run("extern \"C\" fun ngrt_fixture_variadic(count: i64, ...) -> i64;\n"
              "fun main() -> i64 {\n"
              "    if (ngrt_fixture_variadic(3, 10, 20, 30) == 60) { return 1; }\n"
              "    return 0;\n"
              "}",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 1\n") != std::string::npos);
}

TEST_CASE("vNext varargs are restricted to extern C declarations", "[vNext][CABI][Varargs]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("fun f(x: i64, ...) -> i64 { return 0; }\nfun main() -> i64 { return 0; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("`...` is only allowed on `extern \"C\"` signatures"));
  REQUIRE(run("native fun f(x: i64, ...) -> i64;\nfun main() -> i64 { return 0; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("`...` is only allowed on `extern \"C\"` signatures"));
}

TEST_CASE("vNext variadic extern needs a fixed parameter", "[vNext][CABI][Varargs]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("extern \"C\" fun f(...) -> i64;\nfun main() -> i64 { return 0; }", output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("a variadic signature needs at least one fixed parameter"));
}

TEST_CASE("vNext exported extern C varargs are deferred", "[vNext][CABI][Varargs]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("export extern \"C\" fun f(x: i64, ...) -> i64 { return x; }\n"
              "fun main() -> i64 { return 0; }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("varargs are deferred for exported definitions"));
}

TEST_CASE("vNext variadic trailing arguments must be ABI-safe", "[vNext][CABI][Varargs]")
{
  std::string output;
  std::string errors;
  // An NG array is not an ABI-safe scalar and cannot be a C vararg.
  REQUIRE(run("extern \"C\" fun f(fixed: i64, ...) -> i64;\n"
              "fun main() -> i64 { let xs: array<i64> = [1, 2]; return f(0, xs); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("not ABI-safe as a C vararg"));
}
