// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-29 step 3a (D-004): `export extern "C" fun ... { body }` — NG functions
// exposed with a C-visible symbol via an ABI wrapper.
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

TEST_CASE("vNext export extern C emits a C-visible wrapper around the NG body",
          "[vNext][CABI][ExportC]")
{
  std::string errors;
  const auto il = emitsSsa("export extern \"C\" fun add(a: i64, b: i64) -> i64 { return a + b; }\n"
                           "fun main() -> i64 { return add(1, 2); }",
                           errors);
  REQUIRE(errors.empty());
  // The NG body keeps its mangled internal symbol; an unmangled exported
  // wrapper forwards to it with C ABI types.
  REQUIRE_THAT(il, ContainsSubstring("function l $add_0("));
  REQUIRE_THAT(il, ContainsSubstring("export function l $add("));
  REQUIRE_THAT(il, ContainsSubstring("call $add_0("));
}

TEST_CASE("vNext export extern C wrapper translates sub-word and f32 boundaries",
          "[vNext][CABI][ExportC]")
{
  std::string errors;
  const auto il = emitsSsa("export extern \"C\" fun scale(a: i32, b: f32) -> f32 { return b; }\n"
                           "fun main() -> i64 { return 0; }",
                           errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(il, ContainsSubstring("export function s $scale(w %p0, s %p1)"));
  REQUIRE_THAT(il, ContainsSubstring("extsw %p0"));
  REQUIRE_THAT(il, ContainsSubstring("exts %p1"));
  REQUIRE_THAT(il, ContainsSubstring("truncd %r"));
}

TEST_CASE("vNext export extern C is callable from real C code end to end",
          "[vNext][CABI][ExportC][Runtime]")
{
  // libngrt's fixture calls the unmangled NG symbol; the strong generated
  // definition overrides the weak fallback in libngrt.
  std::string output;
  std::string errors;
  REQUIRE(run("export extern \"C\" fun ngrt_fixture_exported_add(a: i64, b: i64) -> i64 { return a + b; }\n"
              "extern \"C\" fun ngrt_fixture_call_exported_add(a: i64, b: i64) -> i64;\n"
              "fun main() -> i64 {\n"
              "    if (ngrt_fixture_call_exported_add(20, 22) == 42) { return 1; }\n"
              "    return 0;\n"
              "}",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 1\n") != std::string::npos);
}

TEST_CASE("vNext export extern C rejects deferred aggregate boundaries", "[vNext][CABI][ExportC]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("repr(C) struct P { x: i32, }\n"
              "export extern \"C\" fun p(a: P) -> i64 { return 0; }\n"
              "fun main() -> i64 { return 0; }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("aggregate parameter `a` is deferred"));
  REQUIRE(run("repr(C) struct P { x: i32, }\n"
              "export extern \"C\" fun p() -> P { return P { x: 1 }; }\n"
              "fun main() -> i64 { return 0; }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("aggregate result is deferred"));
  // Sub-word integer results remain deferred for all extern C signatures.
  REQUIRE(run("export extern \"C\" fun p() -> i8 { return 1; }\n"
              "fun main() -> i64 { return 0; }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("sub-word results are deferred"));
}

TEST_CASE("vNext export extern C cannot be generic", "[vNext][CABI][ExportC]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("export extern \"C\" fun f<T>(x: T) -> i64 { return 0; }\n"
              "fun main() -> i64 { return 0; }",
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("extern \"C\" functions cannot be generic"));
}
