
#include <catch.hpp>
#include <test.hpp>

using namespace NG;
using namespace NG::AST;
using namespace NG::Parsing;

static inline ASTNode *parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

static std::unique_ptr<IASTVisitor> dumper_holder{get_ast_dumper()};

static inline void
runIntegrationTest(const std::string& filename) {
    std::ifstream file(filename);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    auto ast = parse(source);

    IASTVisitor *intp = NG::interpreter::interpreter();

    ast->accept(dumper_holder.get());
    ast->accept(intp);
    auto &&bytes = serialize_ast(ast);
    auto ast2 = deserialize_ast(bytes);
    REQUIRE(*ast == *ast2);
    destroyast(ast);
    destroyast(ast2);

    destroyast(intp);
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

// TEST_CASE(TestStrIntg) {
//     runIntegrationTest("example/04.str.ng");
// }


// TODO: fixmes
// TEST_CASE(TestValDefIntg) {
//     runIntegrationTest("example/05.valdef.ng");
// }
