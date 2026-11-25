// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-30 (A2.1): builds and verifies per-module interface artifacts over the
// merged syntax unit + resolved HIR module.
#include "module_artifact.hpp"

#include <algorithm>
#include <format>
#include <set>

namespace NG::modules
{
  namespace
  {
    /// Type declarations visible to importers (D-009): structs, enums, traits,
    /// consts, and opaque types, mirroring the loader's visibility rules.
    [[nodiscard]] auto typeDeclarationName(const syntax::ModuleItem &item) -> std::string
    {
      if (const auto *structure = dynamic_cast<const syntax::StructDeclaration *>(&item)) return structure->name;
      if (const auto *enumeration = dynamic_cast<const syntax::EnumDeclaration *>(&item)) return enumeration->name;
      if (const auto *trait = dynamic_cast<const syntax::TraitDeclaration *>(&item)) return trait->name;
      if (const auto *declaration = dynamic_cast<const syntax::ConstDeclaration *>(&item)) return declaration->name;
      if (const auto *opaque = dynamic_cast<const syntax::OpaqueTypeDeclaration *>(&item)) return opaque->name;
      return {};
    }
  } // namespace

  auto buildModuleArtifacts(const syntax::SourceUnit &unit, const hir::Module &module) -> std::vector<ModuleArtifact>
  {
    std::vector<ModuleArtifact> artifacts;
    const auto ensure = [&](uint32_t index) -> ModuleArtifact &
    {
      if (artifacts.size() <= index)
        artifacts.resize(index + 1);
      artifacts[index].moduleIndex = index;
      return artifacts[index];
    };
    // Functions carry their origin module and resolved DefId (== position in
    // module.functions for declaration-order functions).
    for (const auto &function : module.functions)
    {
      auto &artifact = ensure(function.originModule);
      if (function.name.starts_with("impl$") || function.name.starts_with("default$"))
        continue;
      artifact.definitions.push_back(PublicDefinition{.name = function.name,
                                                      .span = function.span,
                                                      .function = function.id,
                                                      .isFunction = true,
                                                      .exported = function.exported});
      if (function.exported)
        artifact.exportedFunctions.push_back(function.name);
    }
    // Type declarations come from the merged items (the HIR module stores them
    // without an origin index on every kind, so use the syntax origin).
    for (const auto &item : unit.items)
    {
      const auto typeName = typeDeclarationName(*item);
      if (typeName.empty())
        continue;
      auto &artifact = ensure(item->originModule);
      artifact.typeNames.push_back(typeName);
      artifact.definitions.push_back(
          PublicDefinition{.name = typeName, .span = item->span, .function = {}, .isFunction = false, .exported = true});
    }
    for (auto &artifact : artifacts)
    {
      std::ranges::sort(artifact.exportedFunctions);
      artifact.exportedFunctions.erase(std::unique(artifact.exportedFunctions.begin(), artifact.exportedFunctions.end()),
                                       artifact.exportedFunctions.end());
      std::ranges::sort(artifact.typeNames);
      artifact.typeNames.erase(std::unique(artifact.typeNames.begin(), artifact.typeNames.end()), artifact.typeNames.end());
    }
    return artifacts;
  }

  auto verifyArtifactBoundary(const syntax::SourceUnit &unit, const std::vector<ModuleArtifact> &artifacts)
      -> std::vector<ArtifactBoundaryError>
  {
    std::vector<ArtifactBoundaryError> errors;
    // The loader already computed per-module visible name sets from the same
    // export/type rules the artifacts encode; a selective import that names a
    // symbol absent from the target module's interface is a broken boundary.
    // We re-derive it here from the artifacts so the check is independent of
    // the loader's internal bookkeeping: for each module, every name in its
    // visible set that is not its own must resolve on some imported artifact.
    for (const auto &[moduleIndex, visible] : unit.visibleNames)
    {
      std::set<std::string> localNames;
      if (moduleIndex < artifacts.size())
      {
        const auto &self = artifacts[moduleIndex];
        localNames.insert(self.exportedFunctions.begin(), self.exportedFunctions.end());
        localNames.insert(self.typeNames.begin(), self.typeNames.end());
      }
      // Own private (non-exported) functions are legitimate too.
      for (const auto &definition : artifacts.size() > moduleIndex ? artifacts[moduleIndex].definitions
                                                                   : std::vector<PublicDefinition>{})
        localNames.insert(definition.name);
      // Union of every importable name across all other artifacts.
      std::set<std::string> importable;
      for (size_t index = 0; index < artifacts.size(); ++index)
      {
        if (index == moduleIndex)
          continue;
        importable.insert(artifacts[index].exportedFunctions.begin(), artifacts[index].exportedFunctions.end());
        importable.insert(artifacts[index].typeNames.begin(), artifacts[index].typeNames.end());
      }
      for (const auto &name : visible)
        if (!localNames.contains(name) && !importable.contains(name))
          errors.push_back(ArtifactBoundaryError{.importingModule = moduleIndex,
                                                 .importName = {},
                                                 .missingSymbol = name,
                                                 .message = std::format("module {}: imported name `{}` is not on any "
                                                                        "imported module's interface",
                                                                        moduleIndex, name)});
    }
    return errors;
  }

  auto renderArtifactSummary(const std::vector<ModuleArtifact> &artifacts) -> std::string
  {
    std::string out;
    for (const auto &artifact : artifacts)
    {
      out += std::format("module {}\n", artifact.moduleIndex);
      for (const auto &definition : artifact.definitions)
        out += std::format("  {} {}{}\n", definition.isFunction ? "fun" : "type", definition.name,
                           definition.exported ? "" : " (private)");
    }
    return out;
  }
} // namespace NG::modules
