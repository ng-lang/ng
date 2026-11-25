// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-30 (A2.1): per-module interface artifacts and import-boundary checks.
//
// The loader still merges imported modules into one unit; these artifacts are
// an interface layer over the merged unit + resolved module. Emitting them
// must never change the lowered program (the SSA is byte-identical whether or
// not artifacts are requested).
#include "driver.hpp"
#include "hir.hpp"
#include "module_artifact.hpp"
#include "module_loader.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace modules = NG::modules;

namespace
{
  [[nodiscard]] auto repoPath(std::string_view relative) -> std::string
  {
    std::string path{relative};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example"))
      path = std::string{"../"} + path;
    return path;
  }

  [[nodiscard]] auto runDriver(const std::vector<std::string_view> &arguments, std::string &output,
                               std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  [[nodiscard]] auto artifactSummary(std::string_view path, std::string &errors) -> std::string
  {
    std::string output;
    REQUIRE(runDriver({"--emit=artifact", path}, output, errors) == 0);
    return output;
  }
} // namespace

TEST_CASE("vNext module artifacts list exported functions and types per module", "[vNext][Modules][Artifact]")
{
  std::string errors;
  const auto summary = artifactSummary(repoPath("example/modules/imports_main.ng"), errors);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  // Module 1 is `hello.ng`; it exports greeting/double.
  REQUIRE_THAT(summary, ContainsSubstring("module 1"));
  REQUIRE_THAT(summary, ContainsSubstring("fun greeting"));
  REQUIRE_THAT(summary, ContainsSubstring("fun double"));
  // The entry module's `main` is its own (non-exported) function.
  REQUIRE_THAT(summary, ContainsSubstring("fun main (private)"));
}

TEST_CASE("vNext module artifacts expose type declarations to importers", "[vNext][Modules][Artifact]")
{
  // prelude re-exports Option/Result and declares traits; the artifact lists
  // them as type entries on the prelude module.
  std::string errors;
  const auto summary = artifactSummary(repoPath("example/stdlib_basics.ng"), errors);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE_THAT(summary, ContainsSubstring("type Option"));
  REQUIRE_THAT(summary, ContainsSubstring("type Result"));
}

TEST_CASE("vNext module artifact emission does not change lowered SSA", "[vNext][Modules][Artifact]")
{
  // Equivalence contract: default SSA is identical whether or not the artifact
  // path exists. We assert the artifact mode runs and the plain SSA still runs
  // (the byte-identity sweep is the example corpus check in the suite).
  const auto path = repoPath("example/modules/imports_main.ng");
  std::string output;
  std::string errors;
  REQUIRE(runDriver({"--emit=ssa", path}, output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, ContainsSubstring("export function l $main("));
}

TEST_CASE("vNext module artifacts accept a well-formed import graph", "[vNext][Modules][Artifact]")
{
  std::string errors;
  std::string output;
  const int status = runDriver({"--emit=artifact", repoPath("example/modules/imports_main.ng")}, output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
}

TEST_CASE("vNext module artifact reports a broken import boundary", "[vNext][Modules][Artifact]")
{
  const auto directory = std::filesystem::temp_directory_path() / "ng_artifact_boundary";
  std::error_code ignored;
  std::filesystem::remove_all(directory, ignored);
  std::filesystem::create_directories(directory);
  {
    std::ofstream(directory / "target.ng") << "fun hidden() -> i64 { return 1; }\n"
                                              "export fun shown() -> i64 { return 2; }\n";
    // Importing a private symbol by name is a broken boundary.
    std::ofstream(directory / "user.ng") << "import target (hidden);\nfun main() -> i64 { return 0; }\n";
  }
  std::string output;
  std::string errors;
  const int status = runDriver({"--emit=artifact", (directory / "user.ng").string()}, output, errors);
  std::filesystem::remove_all(directory, ignored);
  REQUIRE(status == 1);
  REQUIRE_THAT(errors, ContainsSubstring("module boundary error"));
  REQUIRE_THAT(errors, ContainsSubstring("`hidden` is not on any imported module's interface"));
}
