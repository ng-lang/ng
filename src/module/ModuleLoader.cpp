#include <ast.hpp>
#include <module.hpp>
#include <orgasm/module.hpp>
#include <parser.hpp>
#include <token.hpp>

#include <debug.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <utility>

#include <sysdep/process.hpp>

namespace NG::module
{

  using namespace NG::parsing;
  namespace fs = std::filesystem;

  using NG::System::Process::current_executable_path;
  auto canonical_module_id(const Vec<Str> &modulePath) -> Str
  {
    Str id = {};
    for (auto &&seg : modulePath)
    {
      if (!id.empty())
      {
        id += ".";
      }
      id += seg;
    }
    return id;
  }

  auto module_id_from_name(Str moduleName) -> ModuleId
  {
    ModuleId id;
    id.canonicalName = std::move(moduleName);
    size_t start = 0;
    while (start <= id.canonicalName.size())
    {
      auto dot = id.canonicalName.find('.', start);
      auto end = dot == Str::npos ? id.canonicalName.size() : dot;
      if (end > start)
      {
        id.pathSegments.push_back(id.canonicalName.substr(start, end - start));
      }
      if (dot == Str::npos)
      {
        break;
      }
      start = dot + 1;
    }
    return id;
  }

  static void append_unique(Vec<Str> &items, Str item)
  {
    if (!item.empty() && std::ranges::find(items, item) == items.end())
    {
      items.push_back(std::move(item));
    }
  }

  static void append_env_module_paths(Vec<Str> &roots)
  {
    const char *raw = std::getenv("NG_MODULE_PATH");
    if (!raw)
    {
      return;
    }
#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    Str value{raw};
    size_t start = 0;
    while (start <= value.size())
    {
      auto end = value.find(separator, start);
      auto part = value.substr(start, end == Str::npos ? Str::npos : end - start);
      append_unique(roots, part);
      if (end == Str::npos)
      {
        break;
      }
      start = end + 1;
    }
  }

  static auto read_text_file(const fs::path &path) -> Str
  {
    std::ifstream file{path};
    if (!file)
    {
      throw RuntimeException("Failed to open module file: " + path.string());
    }
    return Str{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  }

  static auto bytecode_artifact_is_stale(const fs::path &base, const fs::path &modulePath,
                                         const NG::orgasm::BytecodeModule &bytecode) -> bool
  {
    if (bytecode.sourceHash.empty())
    {
      return false;
    }
    Vec<fs::path> sourceProbes;
    auto sourceProbe = modulePath;
    sourceProbe += ".ng";
    sourceProbes.push_back(sourceProbe);
    sourceProbes.push_back(modulePath / "module.ng");
    for (const auto &relative : sourceProbes)
    {
      fs::path candidate{base};
      candidate.append(relative.string());
      if (!fs::exists(candidate))
      {
        continue;
      }
      return NG::orgasm::bytecode_source_hash(read_text_file(candidate)) != bytecode.sourceHash;
    }
    return false;
  }

  static auto source_module_exists(const fs::path &base, const fs::path &modulePath) -> bool
  {
    auto sourceProbe = modulePath;
    sourceProbe += ".ng";
    Vec<fs::path> sourceProbes = {sourceProbe, modulePath / "module.ng"};
    return std::ranges::any_of(sourceProbes, [&](const fs::path &relative) {
      fs::path candidate{base};
      candidate.append(relative.string());
      return fs::exists(candidate);
    });
  }

  static auto bytecode_artifact_missing_required_metadata(const NG::orgasm::BytecodeModule &bytecode) -> bool
  {
    for (const auto &[name, _index] : bytecode.exports)
    {
      if (!bytecode.exportTypeReprs.contains(name))
      {
        return true;
      }
    }
    return false;
  }

  static auto restore_function_type(const Str &repr) -> NG::typecheck::CheckingRef<NG::typecheck::FunctionType>
  {
    return std::dynamic_pointer_cast<NG::typecheck::FunctionType>(NG::typecheck::type_from_repr(repr));
  }

  static auto restore_trait_metadata(const NG::orgasm::BytecodeTraitMetadata &metadata)
      -> NG::typecheck::CheckingRef<NG::typecheck::TraitType>
  {
    auto trait = NG::typecheck::makecheck<NG::typecheck::TraitType>(
        metadata.name, metadata.typeParamNames, metadata.moduleId);
    for (const auto &superTraitName : metadata.superTraits)
    {
      trait->superTraits.push_back(NG::typecheck::makecheck<NG::typecheck::TraitType>(superTraitName));
    }
    for (const auto &[name, repr] : metadata.methods)
    {
      if (auto method = restore_function_type(repr))
      {
        trait->methods.insert_or_assign(name, method);
      }
    }
    for (const auto &[name, repr] : metadata.allMethods)
    {
      if (auto method = restore_function_type(repr))
      {
        trait->allMethods.insert_or_assign(name, method);
      }
    }
    if (trait->allMethods.empty())
    {
      trait->allMethods = trait->methods;
    }
    return trait;
  }

  auto module_search_roots(const Vec<Str> &explicitPaths) -> Vec<Str>
  {
    Vec<Str> roots;
    for (const auto &path : explicitPaths)
    {
      append_unique(roots, path);
    }
    append_env_module_paths(roots);
    append_unique(roots, ".");
    try
    {
      append_unique(roots, standard_library_base_path());
    }
    catch (const std::exception &)
    {
      // Some tests intentionally run without a discoverable installed stdlib.
    }
    return roots;
  }

  Str standard_library_base_path()
  {
    fs::path executable_path{current_executable_path()};

    if (!fs::is_directory(executable_path))
    {
      executable_path = executable_path.parent_path();
    }
    if (fs::exists(executable_path / "lib"))
    {
      return (executable_path / "lib");
    }
    auto project_path = executable_path.parent_path();
    auto library_path = project_path / "lib";
    if (fs::exists(library_path))
    {
      return library_path;
    }
    throw RuntimeException("Cannot locate standard library");
  }

  auto ModuleLoader::load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo>
  {
    return {};
  }

  ModuleLoader::~ModuleLoader() noexcept = default;

  static Map<Str, NG::runtime::RuntimeRef<ModuleInfo>> global_module_info_cache{};

  static auto isCached(Str modulePath) -> bool
  {
    return global_module_info_cache.contains(modulePath);
  }

  static auto getCached(Str modulePath) -> RuntimeRef<ModuleInfo>
  {
    return global_module_info_cache[modulePath];
  }

  static void putCached(Str modulePath, RuntimeRef<ModuleInfo> moduleInfo)
  {
    global_module_info_cache[modulePath] = moduleInfo;
  }

  static auto module_cache_key(const fs::path &base, const Str &moduleId) -> Str
  {
    return "module:" + fs::absolute(base).lexically_normal().string() + ":" + moduleId;
  }

  static auto path_cache_key(const Str &absolutePath, const Str &moduleId) -> Str
  {
    return "path:" + absolutePath + ":" + moduleId;
  }

  void clear_module_loader_cache() noexcept
  {
    global_module_info_cache.clear();
  }

  auto FileBasedExternalModuleLoader::load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo>
  {
    const Str requestedModuleId = canonical_module_id(module);
    fs::path modulePath = std::accumulate(module.begin(), module.end(), fs::path{},
                                          [](fs::path acc, const Str &segment) { return acc / segment; });
    Vec<std::pair<fs::path, ModuleFormat>> relativeProbes;
    auto bytecodeProbe = modulePath;
    bytecodeProbe += ".ngo";
    relativeProbes.push_back({bytecodeProbe, ModuleFormat::BytecodeNgo});
    relativeProbes.push_back({modulePath / "module.ngo", ModuleFormat::BytecodeNgo});
    auto sourceProbe = modulePath;
    sourceProbe += ".ng";
    relativeProbes.push_back({sourceProbe, ModuleFormat::SourceNg});
    relativeProbes.push_back({modulePath / "module.ng", ModuleFormat::SourceNg});

    for (const auto &base : module_search_roots(this->basePaths))
    {
      auto moduleKey = module_cache_key(base, requestedModuleId);
      if (isCached(moduleKey))
      {
        return getCached(moduleKey);
      }
      for (const auto &[relative, format] : relativeProbes)
      {
        fs::path candidate{base};
        candidate.append(relative.string());
        if (!fs::exists(candidate))
        {
          continue;
        }
        auto absolute = fs::absolute(candidate).lexically_normal().string();
        auto absoluteKey = path_cache_key(absolute, requestedModuleId);
        if (isCached(absoluteKey))
        {
          auto cached = getCached(absoluteKey);
          if (cached && cached->moduleId == requestedModuleId)
          {
            return cached;
          }
        }
        if (format == ModuleFormat::BytecodeNgo)
        {
          NG::orgasm::BytecodeModule bytecode;
          try
          {
            bytecode = NG::orgasm::read_bytecode_module(absolute, requestedModuleId);
          }
          catch (const std::exception &)
          {
            if (source_module_exists(fs::path{base}, modulePath))
            {
              continue;
            }
            throw;
          }
          if (bytecode_artifact_is_stale(fs::path{base}, modulePath, bytecode))
          {
            continue;
          }
          if (bytecode_artifact_missing_required_metadata(bytecode) &&
              source_module_exists(fs::path{base}, modulePath))
          {
            continue;
          }
          TypeIndex typeIndex;
          ModuleExportIndex exports;
          for (const auto &[name, _index] : bytecode.exports)
          {
            exports.declared.insert(name);
          }
          for (const auto &[name, repr] : bytecode.exportTypeReprs)
          {
            auto type = NG::typecheck::type_from_repr(repr);
            typeIndex.insert_or_assign(name, type);
            exports.types.insert_or_assign(name, type);
          }
          ModuleTraitIndex traits;
          for (const auto &traitMetadata : bytecode.traitMetadata)
          {
            auto trait = restore_trait_metadata(traitMetadata);
            typeIndex.insert_or_assign(traitMetadata.name, trait);
            exports.types.insert_or_assign(traitMetadata.name, trait);
            traits.insert_or_assign(traitMetadata.name, trait);
          }
          ModuleImplIndex impls;
          impls.reserve(bytecode.implMetadata.size());
          for (const auto &impl : bytecode.implMetadata)
          {
            impls.push_back(ModuleImplEvidence{
                .traitName = impl.traitName,
                .targetPattern = impl.targetPattern,
                .moduleId = impl.moduleId,
                .genericParamNames = Set<Str>{impl.genericParamNames.begin(), impl.genericParamNames.end()},
                .whereBounds = impl.whereBounds,
                .methods = impl.methods,
            });
          }
          auto moduleInfo = runtime::makert<ModuleInfo>(ModuleInfo{
            .moduleId = requestedModuleId,
            .moduleName = module.empty() ? Str{} : module.back(),
            .moduleAbsolutePath = absolute,
            .moduleLoadingLocation = base,
            .moduleTypeIndex = typeIndex,
            .bytecodeModule = std::make_shared<NG::orgasm::BytecodeModule>(std::move(bytecode)),
          });
          moduleInfo->artifact = runtime::makert<ModuleArtifact>(ModuleArtifact{
            .id = module_id_from_name(requestedModuleId),
            .format = ModuleFormat::BytecodeNgo,
            .originPath = absolute,
            .typeIndex = typeIndex,
            .bytecodeModule = moduleInfo->bytecodeModule,
            .exports = std::move(exports),
            .traits = std::move(traits),
            .impls = std::move(impls),
          });
          putCached(moduleKey, moduleInfo);
          putCached(absoluteKey, moduleInfo);
          return moduleInfo;
        }
        std::string source = read_text_file(candidate);
        auto result = Parser(ParseState(Lexer(LexState{source}).lex())).parse(candidate);
        if (!result)
        {
          throw RuntimeException("Failed to parse module '" + requestedModuleId + "' from: " + absolute);
        }
        if (result)
        {
          auto compileUnit = dynamic_ast_cast<NG::ast::CompileUnit>(result);
          auto parsedModule = compileUnit ? compileUnit->module : nullptr;
          if (parsedModule)
          {
            if (parsedModule->nameDeclared && parsedModule->name != requestedModuleId)
            {
              throw RuntimeException("Module declaration mismatch: " + parsedModule->name +
                                     ", expected " + requestedModuleId);
            }
            if (!parsedModule->nameDeclared)
            {
              parsedModule->name = requestedModuleId;
            }
          }
          auto moduleInfo = runtime::makert<ModuleInfo>(ModuleInfo{
            .moduleId = requestedModuleId,
            .moduleName = module.empty() ? Str{} : module.back(),
            .moduleSource = source,
            .moduleAst = result,
            .moduleAbsolutePath = absolute,
            .moduleLoadingLocation = base,
          });
          moduleInfo->artifact = runtime::makert<ModuleArtifact>(ModuleArtifact{
            .id = module_id_from_name(requestedModuleId),
            .format = ModuleFormat::SourceNg,
            .originPath = absolute,
            .ast = result,
          });
          putCached(moduleKey, moduleInfo);
          putCached(absoluteKey, moduleInfo);
          return moduleInfo;
        }
      }
    }
    throw RuntimeException("Module not found: " + requestedModuleId);
  }

  FileBasedExternalModuleLoader::~FileBasedExternalModuleLoader() = default;
} // namespace NG::module
