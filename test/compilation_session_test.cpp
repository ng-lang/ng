// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-31 (A2.2): compilation sessions — per-run build-once state, and
// isolation between sessions.
//
// The language has no module globals/init statements yet (D-009), so the
// runtime "initialize exactly once" contract has no runtime carrier. The
// observable contract at this stage is the compile-time one: within one
// session each module is built exactly once (diamond/cyclic imports included),
// and two sessions never share mutable state.
#include "compilation_session.hpp"
#include "module_artifact.hpp"
#include "module_loader.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>

namespace modules = NG::modules;

namespace
{
  [[nodiscard]] auto repoPath(std::string_view relative) -> std::filesystem::path
  {
    std::string path{relative};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example"))
      path = std::string{"../"} + path;
    return std::filesystem::path{path};
  }

  /// A scratch module directory, removed on destruction.
  struct ScratchDirectory
  {
    std::filesystem::path path;
    explicit ScratchDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / std::filesystem::path{name})
    {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
      std::filesystem::create_directories(path);
    }
    ~ScratchDirectory()
    {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
    void write(std::string_view file, std::string_view contents) const
    {
      std::ofstream(path / std::string{file}) << contents;
    }
  };
} // namespace

TEST_CASE("vNext compilation session builds one artifact per module", "[vNext][Modules][Session]")
{
  modules::CompilationSession session;
  const auto artifacts = session.compileFile(repoPath("example/modules/imports_main.ng"));
  // imports_main.ng (module 0) + hello.ng (module 1).
  REQUIRE(artifacts.size() == 2);
  REQUIRE(session.stats().sourceParses == 1);
  REQUIRE(session.stats().artifactsBuilt == 2);
}

TEST_CASE("vNext compilation session initializes a diamond import exactly once", "[vNext][Modules][Session]")
{
  ScratchDirectory scratch{"ng_session_diamond"};
  // base <- left, base <- right, left+right <- top. base must be built once.
  scratch.write("base.ng", "export fun b() -> i64 { return 1; }\n");
  scratch.write("left.ng", "import base;\nexport fun l() -> i64 { return b(); }\n");
  scratch.write("right.ng", "import base;\nexport fun r() -> i64 { return b(); }\n");
  scratch.write("top.ng", "import left;\nimport right;\n"
                          "fun main() -> i64 { return l() + r(); }\n");
  modules::CompilationSession session;
  const auto artifacts = session.compileFile(scratch.path / "top.ng");
  // Four distinct modules, each built once.
  REQUIRE(artifacts.size() == 4);
  REQUIRE(session.stats().artifactsBuilt == 4);
}

TEST_CASE("vNext compilation session handles a cyclic import once", "[vNext][Modules][Session]")
{
  ScratchDirectory scratch{"ng_session_cycle"};
  // a imports b, b imports a. The loader caches by canonical path, so each is
  // parsed and built once even though the graph is cyclic.
  scratch.write("a.ng", "import b;\nexport fun fa() -> i64 { return 1; }\n");
  scratch.write("b.ng", "import a;\nexport fun fb() -> i64 { return 2; }\n");
  scratch.write("main.ng", "import a;\nimport b;\nfun main() -> i64 { return fa() + fb(); }\n");
  modules::CompilationSession session;
  const auto artifacts = session.compileFile(scratch.path / "main.ng");
  REQUIRE(artifacts.size() == 3);
  REQUIRE(session.stats().artifactsBuilt == 3);
}

TEST_CASE("vNext separate compilation sessions are isolated but equivalent", "[vNext][Modules][Session]")
{
  const auto path = repoPath("example/modules/imports_main.ng");
  modules::CompilationSession first;
  modules::CompilationSession second;
  const auto a = first.compileFile(path);
  const auto b = second.compileFile(path);
  // Each session built its own copy — no shared mutable state.
  REQUIRE(first.stats().artifactsBuilt == 2);
  REQUIRE(second.stats().artifactsBuilt == 2);
  // Equivalent surfaces, independent objects.
  REQUIRE(a.size() == b.size());
  REQUIRE(modules::renderArtifactSummary(a) == modules::renderArtifactSummary(b));
}

TEST_CASE("vNext compilation session surfaces the same boundary error", "[vNext][Modules][Session]")
{
  ScratchDirectory scratch{"ng_session_boundary"};
  scratch.write("target.ng", "fun hidden() -> i64 { return 1; }\nexport fun shown() -> i64 { return 2; }\n");
  scratch.write("user.ng", "import target (hidden);\nfun main() -> i64 { return 0; }\n");
  modules::CompilationSession session;
  const auto artifacts = session.compileFile(scratch.path / "user.ng");
  // The artifacts themselves are built; the boundary check is a separate step
  // (mirrors the driver's `--emit=artifact` path).
  const auto unit = modules::ModuleLoader{}.loadFile(scratch.path / "user.ng");
  const auto failures = modules::verifyArtifactBoundary(unit, artifacts);
  REQUIRE(failures.size() == 1);
  REQUIRE(failures.front().missingSymbol == "hidden");
}
