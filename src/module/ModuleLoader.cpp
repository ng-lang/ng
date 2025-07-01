#include <module.hpp>
#include <token.hpp>
#include <ast.hpp>
#include <parser.hpp>

#include <iostream>
#include <fstream>
#include <debug.hpp>
#include <filesystem>

namespace NG::module {

    using namespace NG::parsing;
    namespace fs = std::filesystem;

    ASTRef<ASTNode> ModuleLoader::load(const Str &module) {
        return ASTRef<ASTNode>();
    }

    ModuleLoader::~ModuleLoader() noexcept = default;

    ASTRef<ASTNode> FileBasedExternalModuleLoader::load(const Str &module) {
        Str path = module;
        if (!path.ends_with(".ng")) {
            path += ".ng";
        }
        for (auto base: this->basePaths) {
            fs::path module_path {base};
            module_path.append(path);
            if (!fs::exists(module_path)) {
                continue;
            }
            std::fstream file {module_path};
            std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            auto result = Parser(ParseState(Lexer(LexState{source}).lex())).parse(module_path);
            if(result) {
                return std::move(*result);
            }    
        }
        // TODO: Add proper error handling
        return nullptr;
    }

    FileBasedExternalModuleLoader::~FileBasedExternalModuleLoader() = default;
}