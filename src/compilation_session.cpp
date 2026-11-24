// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-31 (A2.2): compilation sessions — per-run ownership of the loader and
// the build-once module caches.
#include "compilation_session.hpp"

#include "module_loader.hpp"

namespace NG::modules
{
  auto CompilationSession::compileUnit(const syntax::SourceUnit &unit) -> std::vector<ModuleArtifact>
  {
    const auto resolved = hir::Resolver{}.resolve(unit);
    auto artifacts = buildModuleArtifacts(unit, resolved);
    stats_.artifactsBuilt += static_cast<uint32_t>(artifacts.size());
    return artifacts;
  }

  auto CompilationSession::compileFile(const std::filesystem::path &path) -> std::vector<ModuleArtifact>
  {
    // The loader performs the transitive walk; it already parses each module
    // body at most once (a repeated canonical path returns the cached module
    // index), which is the "exactly once per session" source-level contract.
    const auto unit = loader_.loadFile(path);
    ++stats_.sourceParses;
    return compileUnit(unit);
  }

  auto CompilationSession::compileSource(std::string_view source, const std::filesystem::path &baseDirectory)
      -> std::vector<ModuleArtifact>
  {
    const auto unit = loader_.loadSource(source, baseDirectory);
    ++stats_.sourceParses;
    return compileUnit(unit);
  }
} // namespace NG::modules
