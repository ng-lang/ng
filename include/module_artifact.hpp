// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

// INC-30 (A2.1): module interface artifacts.
//
// The loader merges every transitively imported module into one syntax unit
// today; this header models the *interface* each source module exports so the
// pipeline can (a) verify the boundary — every import that names a symbol must
// see a definition the source module actually exports, and (b) surface the
// artifact (an immutable, per-module function/type descriptor table) for
// diagnostics and the future compiled-artifact path.
//
// The artifact holds no execution state (no globals table, no storage cells):
// it is a pure description of a module's public surface, keyed by the module's
// index in the loader. Emitting it never changes the lowered program; it is an
// interface layer over the same resolved `hir::Module`.

#include "hir.hpp"
#include "syntax/module_parser.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace NG::modules
{
  /// One public definition in a module's interface. Function entries keep the
  /// declaration's source span and DefId; type entries (struct/enum/trait/
  /// opaque/const) carry only their name in this first slice.
  struct PublicDefinition
  {
    std::string name;
    syntax::SourceSpan span;
    /// DefId for functions; default (value 0 is a real id) is ignored for
    /// non-function entries, which are tracked by `isFunction` instead.
    hir::DefId function{};
    bool isFunction{};
    bool exported{};
  };

  /// The immutable interface of one source module (INC-30). Built from the
  /// merged syntax unit + the resolved module; contains no runtime state.
  struct ModuleArtifact
  {
    uint32_t moduleIndex{};
    /// Names of the module's own exported functions, and every type
    /// declaration it introduces (types are visible to importers per D-009).
    std::vector<std::string> exportedFunctions;
    std::vector<std::string> typeNames;
    /// Every public definition the module contributes, in declaration order.
    std::vector<PublicDefinition> definitions;
  };

  struct ArtifactBoundaryError
  {
    uint32_t importingModule{};
    std::string importName;
    std::string missingSymbol;
    std::string message;
  };

  /// Builds one artifact per merged module. `unit.items` carry their
  /// `originModule`; `functions` is the resolved module (DefIds match
  /// declaration order). The result is indexed by module index.
  [[nodiscard]] auto buildModuleArtifacts(const syntax::SourceUnit &unit, const hir::Module &module)
      -> std::vector<ModuleArtifact>;

  /// Verifies the merged import boundary against the artifacts: for every
  /// selective import `import m (a, b);`, both `a` and `b` must be visible on
  /// module `m`'s interface (an exported function or a type declaration).
  /// Returns an empty list when the boundary is consistent. Import directives
  /// themselves are consumed by the loader and are not present in a merged
  /// unit, so this checks the visibility sets the loader computed.
  [[nodiscard]] auto verifyArtifactBoundary(const syntax::SourceUnit &unit,
                                            const std::vector<ModuleArtifact> &artifacts)
      -> std::vector<ArtifactBoundaryError>;

  /// Renders the artifacts as a stable textual interface summary. Emitted by
  /// `--emit=artifact` for inspection and used by the equivalence tests.
  [[nodiscard]] auto renderArtifactSummary(const std::vector<ModuleArtifact> &artifacts) -> std::string;
} // namespace NG::modules
