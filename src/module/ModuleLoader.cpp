#include <ast.hpp>
#include <module.hpp>
#include <parser.hpp>
#include <token.hpp>

#include <debug.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>

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

  void clear_module_loader_cache() noexcept
  {
    global_module_info_cache.clear();
  }

  auto FileBasedExternalModuleLoader::load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo>
  {
    const Str requestedModuleId = canonical_module_id(module);
    if (isCached(requestedModuleId))
    {
      return getCached(requestedModuleId);
    }

    fs::path modulePath = std::accumulate(module.begin(), module.end(), fs::path{},
                                          [](fs::path acc, const Str &segment) { return acc / segment; });
    Vec<fs::path> relativeProbes;
    auto sourceProbe = modulePath;
    sourceProbe += ".ng";
    relativeProbes.push_back(sourceProbe);
    relativeProbes.push_back(modulePath / "module.ng");

    for (const auto &base : module_search_roots(this->basePaths))
    {
      for (const auto &relative : relativeProbes)
      {
        fs::path candidate{base};
        candidate.append(relative.string());
        if (!fs::exists(candidate))
        {
          continue;
        }
        auto absolute = fs::absolute(candidate).lexically_normal().string();
        if (isCached(absolute))
        {
          return getCached(absolute);
        }
        std::fstream file{candidate};
        std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        auto result = Parser(ParseState(Lexer(LexState{source}).lex())).parse(candidate);
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
          putCached(requestedModuleId, moduleInfo);
          putCached(absolute, moduleInfo);
          return moduleInfo;
        }
      }
    }
    throw RuntimeException("Module not found: " + requestedModuleId);
  }

  FileBasedExternalModuleLoader::~FileBasedExternalModuleLoader() = default;
} // namespace NG::module
