

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

static inline ParseResult<ASTRef<ASTNode>> parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

int main(int argc, char **argv) {

    if (argc < 2) {
        std::cout << "must specify a file";
        return -1;
    }

    std::ifstream file(argv[1]);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto astResult = parse(source);
    if (!astResult) {
        std::cout << "Error parsing file: " << astResult.error() << std::endl;
        return -1;
    }

    auto& ast = *astResult;

    IASTVisitor *intp = NG::intp::stupid();

    ast->accept(intp);
}
