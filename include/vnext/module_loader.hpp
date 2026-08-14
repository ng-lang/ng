// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/syntax/module_parser.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace NG::vnext::modules
{
  struct LoadError : std::runtime_error
  {
    explicit LoadError(std::string message) : std::runtime_error(std::move(message)) {}
  };

  /// Loads a source unit and its transitive `import` graph, merging every
  /// reachable module into one syntax unit for the existing resolution and
  /// typechecking pipeline. Imports resolve as `<name>.ng` in the importing
  /// file's directory. Cycles and missing files are deterministic errors.
  /// Merged modules contribute all of their items; `export` is parsed and
  /// recorded, while name-privacy enforcement remains a follow-up.
  class ModuleLoader final
  {
  public:
    [[nodiscard]] auto loadFile(const std::filesystem::path &path) -> syntax::SourceUnit;
    [[nodiscard]] auto loadSource(std::string_view source, const std::filesystem::path &baseDirectory) -> syntax::SourceUnit;

  private:
    void loadModule(const std::filesystem::path &path, std::vector<syntax::ModuleItemPtr> &merged);

    std::vector<std::filesystem::path> loaded_;
  };
} // namespace NG::vnext::modules
