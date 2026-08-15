// AI-generated code; reviewed for this repository's vNext rewrite.
#include "module_loader.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace NG::modules
{
  namespace
  {
    [[nodiscard]] auto readFile(const std::filesystem::path &path) -> std::string
    {
      std::ifstream input{path};
      if (!input) throw LoadError(std::format("cannot read module file `{}`", path.string()));
      std::ostringstream buffer;
      buffer << input.rdbuf();
      return buffer.str();
    }

    [[nodiscard]] auto typeDeclarationName(const syntax::ModuleItem &item) -> std::string
    {
      if (const auto *structure = dynamic_cast<const syntax::StructDeclaration *>(&item)) return structure->name;
      if (const auto *enumeration = dynamic_cast<const syntax::EnumDeclaration *>(&item)) return enumeration->name;
      if (const auto *trait = dynamic_cast<const syntax::TraitDeclaration *>(&item)) return trait->name;
      if (const auto *declaration = dynamic_cast<const syntax::ConstDeclaration *>(&item)) return declaration->name;
      if (const auto *opaque = dynamic_cast<const syntax::OpaqueTypeDeclaration *>(&item)) return opaque->name;
      return {};
    }

    [[nodiscard]] auto isTypeDeclaration(const syntax::ModuleItem &item) -> bool
    {
      return !typeDeclarationName(item).empty();
    }

    struct CollectedModule
    {
      std::filesystem::path path;
      std::vector<syntax::ModuleItemPtr> items;
      std::unordered_set<std::string> functionNames;
      std::unordered_set<std::string> exportedFunctions;
      std::unordered_set<std::string> typeNames;
      /// Imported module index plus selective names (empty = wildcard).
      std::vector<std::pair<uint32_t, std::vector<std::string>>> imports;
    };

    /// Resolves an import name against the importer's directory first, then
    /// walks `lib/std` search paths from the working directory upward so both
    /// repo-root and build-directory invocations find the stdlib.
    [[nodiscard]] auto resolveImportPath(const std::string &name, const std::filesystem::path &baseDirectory)
        -> std::filesystem::path
    {
      const auto direct = baseDirectory / (name + ".ng");
      if (std::filesystem::exists(direct)) return direct;
      for (auto directory = std::filesystem::current_path();; directory = directory.parent_path())
      {
        const auto candidate = directory / "lib" / "std" / (name + ".ng");
        if (std::filesystem::exists(candidate)) return candidate;
        if (!directory.has_parent_path() || directory.parent_path() == directory) break;
      }
      return std::filesystem::current_path() / "lib" / "std" / (name + ".ng");
    }

    /// Loads a module file into `modules`, recursing through imports. Returns
    /// the assigned module index.
    auto collectModuleFile(const std::filesystem::path &path, std::vector<CollectedModule> &modules,
                           std::unordered_map<std::string, uint32_t> &indexByPath,
                           const auto &self) -> uint32_t
    {
      const auto canonical = std::filesystem::weakly_canonical(path);
      if (const auto found = indexByPath.find(canonical.string()); found != indexByPath.end()) return found->second;
      if (!std::filesystem::exists(canonical))
        throw LoadError(std::format("module `{}` not found (looked for {})", path.stem().string(), canonical.string()));
      const uint32_t index = static_cast<uint32_t>(modules.size());
      indexByPath.emplace(canonical.string(), index);
      modules.push_back(CollectedModule{.path = canonical});
      auto unit = syntax::parseSourceUnit(readFile(canonical));
      for (auto &item : unit.items)
      {
        if (const auto *import = dynamic_cast<const syntax::ImportDeclaration *>(item.get()); import != nullptr)
        {
          const auto resolved = resolveImportPath(import->name, canonical.parent_path());
          const uint32_t imported = self(self, resolved);
          modules[index].imports.push_back({imported, import->names});
          continue;
        }
        if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()); function != nullptr)
        {
          modules[index].functionNames.insert(function->name);
          if (function->exported) modules[index].exportedFunctions.insert(function->name);
        }
        else if (isTypeDeclaration(*item))
        {
          modules[index].typeNames.insert(typeDeclarationName(*item));
        }
        modules[index].items.push_back(std::move(item));
      }
      return index;
    }

    /// Collects a parsed entry unit (already loaded items) as module 0.
    void collectEntryItems(std::vector<syntax::ModuleItemPtr> &items, std::vector<CollectedModule> &modules,
                           std::unordered_map<std::string, uint32_t> &indexByPath, const std::filesystem::path &baseDirectory,
                           const auto &self)
    {
      if (modules.empty()) modules.push_back(CollectedModule{.path = baseDirectory});
      for (auto &item : items)
      {
        if (const auto *import = dynamic_cast<const syntax::ImportDeclaration *>(item.get()); import != nullptr)
        {
          const auto resolved = resolveImportPath(import->name, baseDirectory);
          const uint32_t imported = self(self, resolved);
          modules.front().imports.push_back({imported, import->names});
          continue;
        }
        if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()); function != nullptr)
        {
          modules.front().functionNames.insert(function->name);
          if (function->exported) modules.front().exportedFunctions.insert(function->name);
        }
        else if (isTypeDeclaration(*item))
        {
          modules.front().typeNames.insert(typeDeclarationName(*item));
        }
        modules.front().items.push_back(std::move(item));
      }
      items.clear();
    }

    /// Computes per-module visible name sets: a module sees its own names,
    /// the selective import lists it names, and (for wildcard imports) the
    /// exported functions plus type declarations of the imported module.
    [[nodiscard]] auto computeVisibility(const std::vector<CollectedModule> &modules)
        -> std::unordered_map<uint32_t, std::unordered_set<std::string>>
    {
      // Re-exported surfaces: a module's exported functions plus the exported
      // surfaces of its wildcard imports (transitive re-export). Imports get
      // higher indexes than their importers, so a reverse pass is acyclic
      // for the deterministic import graph.
      std::vector<std::unordered_set<std::string>> surface(modules.size());
      for (size_t i = modules.size(); i-- > 0;)
      {
        surface[i] = modules[i].exportedFunctions;
        for (const auto &[target, selective] : modules[i].imports)
          if (selective.empty()) surface[i].insert(surface[target].begin(), surface[target].end());
      }
      std::unordered_map<uint32_t, std::unordered_set<std::string>> visible;
      for (uint32_t index = 0; index < modules.size(); ++index)
      {
        auto &names = visible[index];
        names = modules[index].functionNames;
        names.insert(modules[index].typeNames.begin(), modules[index].typeNames.end());
        for (const auto &[target, selective] : modules[index].imports)
        {
          if (selective.empty())
          {
            names.insert(surface[target].begin(), surface[target].end());
            names.insert(modules[target].typeNames.begin(), modules[target].typeNames.end());
          }
          else
          {
            names.insert(selective.begin(), selective.end());
          }
        }
      }
      return visible;
    }

    [[nodiscard]] auto mergeUnits(std::vector<CollectedModule> &modules,
                                  const std::unordered_map<uint32_t, std::unordered_set<std::string>> &visible)
        -> syntax::SourceUnit
    {
      std::vector<syntax::ModuleItemPtr> merged;
      for (uint32_t index = 0; index < modules.size(); ++index)
        for (auto &item : modules[index].items)
        {
          item->originModule = index;
          merged.push_back(std::move(item));
        }
      const size_t begin = merged.empty() ? 0 : merged.front()->span.begin;
      const size_t end = merged.empty() ? 0 : merged.back()->span.end;
      return syntax::SourceUnit{syntax::SourceSpan{begin, end}, std::move(merged), visible};
    }
  } // namespace

  auto ModuleLoader::loadFile(const std::filesystem::path &path) -> syntax::SourceUnit
  {
    loaded_.clear();
    std::vector<CollectedModule> modules;
    std::unordered_map<std::string, uint32_t> indexByPath;
    const auto collect = [&](const auto &self, const std::filesystem::path &modulePath) -> uint32_t {
      return collectModuleFile(modulePath, modules, indexByPath, self);
    };
    static_cast<void>(collect(collect, path));
    const auto visible = computeVisibility(modules);
    return mergeUnits(modules, visible);
  }

  auto ModuleLoader::loadSource(std::string_view source, const std::filesystem::path &baseDirectory) -> syntax::SourceUnit
  {
    loaded_.clear();
    std::vector<CollectedModule> modules;
    std::unordered_map<std::string, uint32_t> indexByPath;
    auto unit = syntax::parseSourceUnit(source);
    const auto collect = [&](const auto &self, const std::filesystem::path &modulePath) -> uint32_t {
      return collectModuleFile(modulePath, modules, indexByPath, self);
    };
    collectEntryItems(unit.items, modules, indexByPath, baseDirectory, collect);
    const auto visible = computeVisibility(modules);
    return mergeUnits(modules, visible);
  }
} // namespace NG::modules
