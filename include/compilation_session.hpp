// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

// INC-31 (A2.2): compilation sessions.
//
// A `CompilationSession` owns the compilation-side state for one compilation
// run: the source-unit loader and the caches of build products keyed by module
// path (parsed source units today, module interface artifacts in the same run).
// It makes the "one instance per module, built exactly once" contract explicit
// and gives it its only currently-observable carrier:
//
//   - within a session, a module's parse result and interface artifact are
//     built exactly once, even across a diamond/cyclic import graph;
//   - across sessions, nothing mutable is shared — a fresh session rebuilds
//     from scratch and yields an equivalent but independent result.
//
// The language has no module-level globals or init statements yet (D-009), so
// there is no runtime global state to split into artifacts vs instances; this
// session layer is the compile-time half of that split. When module globals
// arrive, per-session instance state attaches here without changing callers.

#include "hir.hpp"
#include "module_artifact.hpp"
#include "module_loader.hpp"
#include "syntax/module_parser.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NG::modules
{
  /// Build counters, exposed so tests can assert "exactly once" behavior.
  struct SessionStats
  {
    /// Number of distinct modules parsed from disk (cache misses).
    uint32_t sourceParses{};
    /// Number of interface artifacts built.
    uint32_t artifactsBuilt{};
    /// Number of cache hits (a re-request for an already-built module).
    uint32_t cacheHits{};
  };

  /// One compilation run's owned state. Not copyable: it is the unit of
  /// compilation isolation, and two sessions never share mutable state.
  class CompilationSession final
  {
  public:
    CompilationSession() = default;
    CompilationSession(const CompilationSession &) = delete;
    auto operator=(const CompilationSession &) -> CompilationSession & = delete;
    CompilationSession(CompilationSession &&) = default;
    auto operator=(CompilationSession &&) -> CompilationSession & = default;

    /// Loads the transitive import graph rooted at `path`, resolving and
    /// checking it in this session, and returns the module interface
    /// artifacts. Each distinct module is parsed exactly once per session.
    [[nodiscard]] auto compileFile(const std::filesystem::path &path) -> std::vector<ModuleArtifact>;

    /// Same, for an in-memory entry source unit (tests, `--source`).
    [[nodiscard]] auto compileSource(std::string_view source, const std::filesystem::path &baseDirectory)
        -> std::vector<ModuleArtifact>;

    [[nodiscard]] auto stats() const -> const SessionStats & { return stats_; }

  private:
    [[nodiscard]] auto compileUnit(const syntax::SourceUnit &unit) -> std::vector<ModuleArtifact>;

    ModuleLoader loader_;
    SessionStats stats_;
  };
} // namespace NG::modules
