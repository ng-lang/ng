// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: file-size and copy-paste guards to prevent future monoliths.
//
// This test scans every .cpp and .hpp file under src/ and include/ and
// enforces a hard line-count ceiling.  The limit is intentionally
// conservative — if a file genuinely needs to grow past it, the limit
// must be raised in *this file* with a written justification, so the
// decision is visible in code review.

#include "test.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
  /// Maximum allowed lines per source file.  Intentionally hard-coded so
  /// that raising it requires an explicit, reviewable edit.
  ///
  /// Current ceiling (2026-08-26): set to pass every existing file.
  /// Target ceiling (stretch goal): .cpp ≤ 1200, .hpp ≤ 400 — approaching
  /// this as future ARCH work splits flowir.cpp, typecheck_infer.cpp,
  /// ast.hpp, and function_lowerer.hpp.
  /// 2058: INC-10 while/continue/break loop lowering landed in flowir.cpp
  /// (lowerWhile/lowerContinue/lowerBreak); ARCH split tracked in the
  /// post-cutover plan.
  /// 2149: INC-11 derive(Show) string synthesis (buildShowString) landed in
  /// flowir.cpp — the generate/show slice is ~90 lines and is planned to
  /// move out with the ARCH flowir split in the post-cutover plan.
  /// 2177: INC-14 S1 closure lowering (extended directCall for closure calls,
  /// inert zero-word closure literals) landed in flowir.cpp; ARCH split
  /// tracked in the post-cutover plan.
  /// 2185: INC-14 S2 wired function-body tail values into the fallthrough
  /// return (latent bugfix: bare value tails previously emitted `ret` without
  /// an operand), plus the closure instance return target.
  /// 2202: INC-14 S3 closure capture threading in flowir.cpp (prepend captured
  /// enclosing-local values as leading closure-call operands); ARCH split
  /// tracked in the post-cutover plan.
  /// 2203: INC-14 S3 higher-order capture threading widened the directCall
  /// guard to fire on callCaptures (overload re-inference safe);
  /// ARCH split tracked in the post-cutover plan.
  constexpr size_t kMaxCppLines = 2203;
  /// 917: INC-10 added While/Continue/Break statement nodes to ast.hpp;
  /// ARCH split tracked in the post-cutover plan.
  /// 938: INC-14 S1 added ClosureExpression to ast.hpp (plus FunctionParameter
  /// and Block forward declarations); ARCH split tracked in the post-cutover
  /// plan.
  /// 955: INC-14 S2 added ClosureTypeSyntax (`|T| -> R` function-type literal)
  /// to ast.hpp for expected-type-driven inference.
  /// 966: INC-14 S3 added `Capture`-aware CaptureInfo field and
  /// closureCaptureParams to the Closure expression; ARCH split tracked in the
  /// post-cutover plan.
  /// 977: INC-13 added TryExpression (`expr?`, D-007) to ast.hpp; ARCH split
  /// tracked in the post-cutover plan.
  /// 980: INC-15 added the `origins` list to ScopedReferenceTypeSyntax
  /// (`T ref(a | b)`, D-002 A1.1); ARCH split tracked in the post-cutover plan.
  constexpr size_t kMaxHeaderLines = 980;

  /// Recursively collect every file matching `extension` under `root`.
  auto collectFiles(const std::string &root, std::string_view extension) -> std::vector<std::filesystem::path>
  {
    std::vector<std::filesystem::path> files;
    const std::filesystem::path rootPath(root);
    if (!std::filesystem::exists(rootPath))
      return files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(rootPath))
    {
      if (!entry.is_regular_file())
        continue;
      if (entry.path().extension() != extension)
        continue;
      // Skip generated / vendored paths.
      const auto path = entry.path().string();
      if (path.find("/build") != std::string::npos || path.find("/_deps/") != std::string::npos ||
          path.find("/vendored/") != std::string::npos)
        continue;
      files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
  }

  /// Count newlines in a file (cheap byte scan).
  auto countLines(const std::filesystem::path &path) -> size_t
  {
    std::ifstream stream(path);
    size_t lines = 0;
    std::string buffer;
    while (std::getline(stream, buffer))
      ++lines;
    return lines;
  }
} // namespace

TEST_CASE("Source files respect the hard line-count ceiling", "[Arch][Guard]")
{
  std::vector<std::string> violations;

#ifdef NG_SOURCE_DIR
  const std::string srcDir = std::string(NG_SOURCE_DIR) + "/src";
  const std::string includeDir = std::string(NG_SOURCE_DIR) + "/include";
#else
  const std::string srcDir = "src";
  const std::string includeDir = "include";
#endif

  for (const auto &path : collectFiles(srcDir, ".cpp"))
  {
    const auto lines = countLines(path);
    if (lines > kMaxCppLines)
      violations.push_back(std::format("{}: {} lines (limit {})", path.string(), lines, kMaxCppLines));
  }
  for (const auto &path : collectFiles(includeDir, ".hpp"))
  {
    const auto lines = countLines(path);
    if (lines > kMaxHeaderLines)
      violations.push_back(std::format("{}: {} lines (limit {})", path.string(), lines, kMaxHeaderLines));
  }
  // Also check src/ headers (internal headers).
  for (const auto &path : collectFiles(srcDir, ".hpp"))
  {
    const auto lines = countLines(path);
    if (lines > kMaxHeaderLines)
      violations.push_back(std::format("{}: {} lines (limit {})", path.string(), lines, kMaxHeaderLines));
  }

  if (!violations.empty())
  {
    std::string message = "Files exceed the hard line-count ceiling:\n";
    for (const auto &v : violations)
      message += "  " + v + "\n";
    message += "\nTo fix: split the file or raise the limit in test/guard_test.cpp with a justification.";
    FAIL(message);
  }
}
