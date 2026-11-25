// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-29 step 3c: C function-pointer callbacks, limited to named
// non-capturing functions. The callback value is a generated C ABI thunk;
// closure captures (userdata/trampolines) are explicitly out of scope.
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

TEST_CASE("vNext named function passes as a C callback end to end", "[vNext][CABI][Callback][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("extern \"C\" fun ngrt_fixture_apply_callback(cb: |i64| -> i64, x: i64) -> i64;\n"
              "fun addOne(x: i64) -> i64 { return x + 1; }\n"
              "fun main() -> i64 {\n"
              "    if (ngrt_fixture_apply_callback(addOne, 41) == 42) { return 1; }\n"
              "    return 0;\n"
              "}",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 1\n") != std::string::npos);
}

TEST_CASE("vNext callback thunk translates sub-word C boundaries", "[vNext][CABI][Callback][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("extern \"C\" fun ngrt_fixture_apply_callback(cb: |i32| -> i32, x: i64) -> i64;\n"
              "fun addOne(x: i32) -> i32 { return x + 1; }\n"
              "fun main() -> i64 {\n"
              "    if (ngrt_fixture_apply_callback(addOne, 41) == 42) { return 1; }\n"
              "    return 0;\n"
              "}",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 1\n") != std::string::npos);
}

TEST_CASE("vNext callback lowers to a generated C ABI thunk pointer", "[vNext][CABI][Callback]")
{
  std::string errors;
  const auto il = emitsSsa("extern \"C\" fun ngrt_fixture_apply_callback(cb: |i32| -> i32, x: i64) -> i64;\n"
                           "fun addOne(x: i32) -> i32 { return x + 1; }\n"
                           "fun main() -> i64 { return ngrt_fixture_apply_callback(addOne, 41); }",
                           errors);
  REQUIRE(errors.empty());
  // Internal thunk: C ABI `w` signature, widens to the NG i64 slot.
  REQUIRE_THAT(il, ContainsSubstring("function w $cb1(w %p0)"));
  REQUIRE_THAT(il, ContainsSubstring("extsw %p0"));
  // The call site passes the thunk address, not the NG body symbol.
  REQUIRE_THAT(il, ContainsSubstring("copy $cb1"));
}

TEST_CASE("vNext callback requires a named function", "[vNext][CABI][Callback]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("extern \"C\" fun ngrt_fixture_apply_callback(cb: |i64| -> i64, x: i64) -> i64;\n"
              "fun main() -> i64 {\n"
              "    let c = |x: i64| x + 1;\n"
              "    if (ngrt_fixture_apply_callback(c, 41) == 42) { return 1; }\n"
              "    return 0;\n"
              "}",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("a C callback must be a named function"));
}

TEST_CASE("vNext callback function signature must match exactly", "[vNext][CABI][Callback]")
{
  std::string output;
  std::string errors;
  // Wrong arity.
  REQUIRE(run("extern \"C\" fun ngrt_fixture_apply_callback(cb: |i64| -> i64, x: i64) -> i64;\n"
              "fun two(a: i64, b: i64) -> i64 { return a + b; }\n"
              "fun main() -> i64 { return ngrt_fixture_apply_callback(two, 41); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("no function matches callback signature"));
  // Wrong result type.
  REQUIRE(run("extern \"C\" fun ngrt_fixture_apply_callback(cb: |i64| -> i64, x: i64) -> i64;\n"
              "fun f(a: i64) -> bool { return true; }\n"
              "fun main() -> i64 { return ngrt_fixture_apply_callback(f, 41); }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("no function matches callback signature"));
}

TEST_CASE("vNext callback types require ABI-safe scalar signatures", "[vNext][CABI][Callback]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("extern \"C\" fun f(cb: |array<i64>| -> i64) -> i64;\n"
              "fun main() -> i64 { return 0; }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("not ABI-safe"));
}
