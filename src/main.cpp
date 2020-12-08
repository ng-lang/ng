

#include "ast.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <fstream>
#include <iostream>
#include <streambuf>
#include "intp/Interpreter.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

static inline ASTRef<ASTNode> parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

int main(int argc, char **argv) {

    if (argc < 2) {
        std::cout << "must specify a file";
        return -1;
    }

    std::ifstream file(argv[1]);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto&& ast { parse(source) };

    IASTVisitor *intp = NG::intp::interpreter();

    ast->accept(intp);
}
