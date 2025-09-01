#include <module.hpp>
#include <token.hpp>
#include <ast.hpp>
#include <parser.hpp>

#include <iostream>
#include <fstream>
#include <debug.hpp>
#include <filesystem>

#include <sysdep/process.hpp>

namespace NG::module
{

    using namespace NG::parsing;
    namespace fs = std::filesystem;

    using NG::System::Process::current_executable_path;

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

    auto ModuleLoader::load(const Str &module) -> ASTRef<ASTNode>
    {
        return {};
    }

    ModuleLoader::~ModuleLoader() noexcept = default;

    auto FileBasedExternalModuleLoader::load(const Str &module) -> ASTRef<ASTNode>
    {
        Str path = module;
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
            std::fstream file{module_path};
            std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            auto result = Parser(ParseState(Lexer(LexState{source}).lex())).parse(module_path);
            if (result)
            {
                return std::move(*result);
            }
        }
        throw RuntimeException("Module not found: " + module);
    }

    FileBasedExternalModuleLoader::~FileBasedExternalModuleLoader() = default;
}