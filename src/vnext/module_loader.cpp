// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/module_loader.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>
#include <string>

namespace NG::vnext::modules
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
  } // namespace

  void ModuleLoader::loadModule(const std::filesystem::path &path, std::vector<syntax::ModuleItemPtr> &merged)
  {
    const auto canonical = std::filesystem::weakly_canonical(path);
    if (std::find(loaded_.begin(), loaded_.end(), canonical) != loaded_.end()) return;
    if (!std::filesystem::exists(canonical))
      throw LoadError(std::format("module `{}` not found (looked for {})", path.stem().string(), canonical.string()));
    loaded_.push_back(canonical);
    auto unit = syntax::parseSourceUnit(readFile(canonical));
    for (auto &item : unit.items)
    {
      if (dynamic_cast<const syntax::ImportDeclaration *>(item.get()) != nullptr)
      {
        const auto *import = static_cast<const syntax::ImportDeclaration *>(item.get());
        loadModule(canonical.parent_path() / (import->name + ".ng"), merged);
        continue;
      }
      merged.push_back(std::move(item));
    }
  }

  auto ModuleLoader::loadFile(const std::filesystem::path &path) -> syntax::SourceUnit
  {
    loaded_.clear();
    std::vector<syntax::ModuleItemPtr> merged;
    loadModule(path, merged);
    const size_t begin = merged.empty() ? 0 : merged.front()->span.begin;
    return syntax::SourceUnit{syntax::SourceSpan{begin, merged.empty() ? 0 : merged.back()->span.end}, std::move(merged)};
  }

  auto ModuleLoader::loadSource(std::string_view source, const std::filesystem::path &baseDirectory) -> syntax::SourceUnit
  {
    loaded_.clear();
    auto unit = syntax::parseSourceUnit(source);
    std::vector<syntax::ModuleItemPtr> merged;
    for (auto &item : unit.items)
    {
      if (dynamic_cast<const syntax::ImportDeclaration *>(item.get()) != nullptr)
      {
        const auto *import = static_cast<const syntax::ImportDeclaration *>(item.get());
        loadModule(baseDirectory / (import->name + ".ng"), merged);
        continue;
      }
      merged.push_back(std::move(item));
    }
    const size_t begin = merged.empty() ? 0 : merged.front()->span.begin;
    return syntax::SourceUnit{syntax::SourceSpan{begin, merged.empty() ? 0 : merged.back()->span.end}, std::move(merged)};
  }
} // namespace NG::vnext::modules
