// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: file-size and copy-paste guards to prevent future monoliths.
//
// This test scans every .cpp and .hpp file under src/ and include/ and
// enforces a hard line-count ceiling.  Files that grow past it are split;
// the ceiling itself does not move.

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
  /// Maximum allowed lines per source file.  Intentionally hard-coded:
  /// these are hard ceilings.  A file that outgrows them must be split along
  /// complete definitions (see flowir_*.cpp, typecheck_*.cpp, hir_*.cpp,
  /// syntax/ast_expr.hpp for the pattern) — never fixed by raising a limit.
  constexpr size_t kMaxCppLines = 1200;
  constexpr size_t kMaxHeaderLines = 800;

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
    message += "\nTo fix: split the file along complete definitions (the limits are hard ceilings).";
    FAIL(message);
  }
}

namespace
{
  /// Machine-specific home directories must not leak into tracked files:
  /// absolute paths embed a local user name and break portability. Sources,
  /// docs, and prompts refer to the repository root or use relative paths.
  constexpr std::string_view kHomePrefixes[] = {"/Users/", "/home/"};
} // namespace

TEST_CASE("Tracked sources and docs contain no absolute home paths", "[Arch][Guard]")
{
#ifdef NG_SOURCE_DIR
  const std::filesystem::path root(NG_SOURCE_DIR);
#else
  const std::filesystem::path root = std::filesystem::is_directory("src") ? "." : "..";
#endif

  std::vector<std::string> violations;
  const auto scanFile = [&](const std::filesystem::path &path)
  {
    // This test names the forbidden prefixes, so it is not scanned itself.
    if (path.filename() == "guard_test.cpp")
      return;
    std::ifstream stream(path);
    std::string line;
    size_t number = 0;
    while (std::getline(stream, line))
    {
      ++number;
      for (const auto prefix : kHomePrefixes)
        if (line.find(prefix) != std::string::npos)
          violations.push_back(std::format("{}:{}: contains `{}`", path.string(), number, prefix));
    }
  };
  const auto scanTree = [&](const std::filesystem::path &directory)
  {
    if (!std::filesystem::exists(directory))
      return;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
      if (entry.is_regular_file())
        scanFile(entry.path());
  };

  for (const auto *directory : {"src", "include", "lib", "example", "test", "docs/guide", "docs/ref", "docs/research",
                                ".agents/plans"})
    scanTree(root / directory);
  for (const auto *file : {"README.md", "AGENTS.md", "HANDOFF.md", "CONTRIBUTING.md", "CHANGELOG.md"})
    if (std::filesystem::exists(root / file))
      scanFile(root / file);

  if (!violations.empty())
  {
    std::string message = "Absolute home-directory paths found (use relative paths instead):\n";
    for (const auto &v : violations)
      message += "  " + v + "\n";
    FAIL(message);
  }
}
