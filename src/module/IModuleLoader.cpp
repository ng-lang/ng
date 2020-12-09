#include <module.hpp>
#include <token.hpp>
#include <ast.hpp>
#include <parser.hpp>

#include <iostream>
#include <fstream>

namespace NG::module {

    using namespace NG::parsing;

    ASTRef<ASTNode> IModuleLoader::load(const Str &module) {
        return ASTRef<ASTNode>();
    }

    IModuleLoader::~IModuleLoader() noexcept = default;

    ASTRef<ASTNode> FileBasedExternalModuleLoader::load(const Str &module) {
        Str path = module;
        if (!path.ends_with(".ng")) {
            path += ".ng";
        }
        std::ifstream file{path};
        std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
    }

    FileBasedExternalModuleLoader::~FileBasedExternalModuleLoader() = default;
}