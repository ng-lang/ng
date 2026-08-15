// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "syntax/module_parser.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace NG::modules
{
  struct LoadError : std::runtime_error
  {
    explicit LoadError(std::string message) : std::runtime_error(std::move(message)) {}
  };

  /// Loads a source unit and its transitive `import` graph, merging every
  /// reachable module into one syntax unit for the existing resolution and
  /// typechecking pipeline. Imports resolve as `<name>.ng` in the importing
  /// file's directory. Cycles and missing files are deterministic errors.
  ///
  /// Name privacy (D-009): each merged item carries its origin module, and
  /// the unit records per-module visible top-level names — a module sees its
  /// own names, its selective import lists, and (for wildcard imports) the
  /// imported module's exported functions plus its type declarations. The
  /// resolver enforces these sets for function references.
  class ModuleLoader final
  {
  public:
    [[nodiscard]] auto loadFile(const std::filesystem::path &path) -> syntax::SourceUnit;
    [[nodiscard]] auto loadSource(std::string_view source, const std::filesystem::path &baseDirectory) -> syntax::SourceUnit;

  private:
    std::vector<std::filesystem::path> loaded_;
  };
} // namespace NG::modules
