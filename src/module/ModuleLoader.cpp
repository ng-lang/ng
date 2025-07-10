#include <module.hpp>
#include <token.hpp>
#include <ast.hpp>
#include <parser.hpp>

#include <iostream>
#include <fstream>
#include <debug.hpp>
#include <filesystem>

namespace NG::module
{

    using namespace NG::parsing;
    namespace fs = std::filesystem;

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
        for (const auto& base : this->basePaths)
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
        // TODO: Add proper error handling
        return nullptr;
    }

    FileBasedExternalModuleLoader::~FileBasedExternalModuleLoader() = default;
}