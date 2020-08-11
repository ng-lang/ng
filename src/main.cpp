

#include "ast.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <streambuf>
#include "interpreter.hpp"

using namespace NG;
using namespace NG::AST;
using namespace NG::Parsing;

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

    std::unique_ptr<ASTNode> ast{parse(source)};

    IASTVisitor *intp = NG::interpreter::interpreter();

    ast->accept(intp);
}
