#include <module.hpp>
#include <token.hpp>
#include <ast.hpp>
#include <parser.hpp>

#include <iostream>
#include <fstream>
#include <debug.hpp>
#include <filesystem>
#include <numeric>

#include <sysdep/process.hpp>

namespace NG::module
{

    using namespace NG::parsing;
    namespace fs = std::filesystem;

    using NG::System::Process::current_executable_path;
    static auto moduleId(const Vec<Str> &modulePath) -> Str
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

    auto FileBasedExternalModuleLoader::load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo>
    {
        Str path = std::accumulate(module.begin(), module.end(), fs::path{}, [](const fs::path &path, const Str &segment) -> Str
                                   {
            if (path == "") {
                return segment;
            }
            return path / segment; });

        if (!path.ends_with(".ng"))
        {
            path += ".ng";
        }
        for (const auto &base : this->basePaths)
        {
            fs::path module_path{base};
            module_path.append(path);
            if (!fs::exists(module_path))
            {
                continue;
            }
            if (isCached(module_path))
            {
                return getCached(module_path);
            }
            std::fstream file{module_path};
            std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            auto result = Parser(ParseState(Lexer(LexState{source}).lex())).parse(module_path);
            if (result)
            {
                auto moduleInfo = runtime::makert<ModuleInfo>(ModuleInfo{
                    .moduleId = moduleId(module),
                    .moduleName = *(module.end() - 1),
                    .moduleSource = source,
                    .moduleAst = *result,
                    .moduleAbsolutePath = path,
                    .moduleLoadingLocation = "",
                });
                putCached(module_path, moduleInfo);
                return moduleInfo;
            }
        }
        throw RuntimeException("Module not found: " + path);
    }

    FileBasedExternalModuleLoader::~FileBasedExternalModuleLoader() = default;
}