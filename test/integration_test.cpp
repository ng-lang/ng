
#include "test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

static inline ASTRef<ASTNode> parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

static inline void
runIntegrationTest(const std::string &filename) {
    std::ifstream file(filename);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    auto ast = parse(source);

    IInterperter *intp = NG::intp::interpreter();

    ast->accept(intp);

    intp->summary();
    destroyast(ast);

    delete intp;
}

TEST_CASE("should run with id function definition", "[Integration]") {
    runIntegrationTest("example/01.id.ng");
}

TEST_CASE("should run with many definitions", "[Integration]") {
    runIntegrationTest("example/02.many_defs.ng");
}

TEST_CASE("should run with function calls", "[Integration]") {
    runIntegrationTest("example/03.funcall_and_idexpr.ng");
}


TEST_CASE("should run with array", "[Integration]") {
    runIntegrationTest("example/06.array.ng");
}

TEST_CASE("should run with string concat", "[Integration]") {
    runIntegrationTest("example/04.str.ng");
}


TEST_CASE("should run with valdefs -- function and members", "[Integration]") {
    runIntegrationTest("example/05.valdef.ng");
}

TEST_CASE("should run with type definition and object creation", "[Integration]") {
    runIntegrationTest("example/07.object.ng");
}

