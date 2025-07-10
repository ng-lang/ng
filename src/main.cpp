

#include "ast.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <fstream>
#include <iostream>
#include <streambuf>
#include "intp/intp.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

static inline auto parse(const Str &source, const Str &file) -> ParseResult<ASTRef<ASTNode>>
{
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse(file);
}

auto main(int argc, char **argv) -> int
{

    if (argc < 2)
    {
        std::cout << "must specify a file";
        return -1;
    }

    if (!std::filesystem::exists(argv[1]))
    {
        std::cout << "file " << argv[1] << " not found";
        return -1;
    }

    std::ifstream file(argv[1]);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto astResult = parse(source, argv[1]);
    if (!astResult)
    {
        std::cout << "Error parsing file: " << astResult.error().message << '\n';
        return -1;
    }

    auto &ast = *astResult;

    AstVisitor *stupid = NG::intp::stupid();

    ast->accept(stupid);

    destroyast(ast);
}
