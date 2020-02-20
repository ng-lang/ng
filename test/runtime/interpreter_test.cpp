#include "../../../3rdparty/catch.hpp"
#include <test.hpp>

using namespace NG;
using namespace NG::Parsing;
using namespace NG::interpreter;
using namespace NG::AST;

static ASTRef<ASTNode> parse(const Str &source) {
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse();
}

TEST_CASE("interpreter should accept simple definitions", "[InterpreterTest]") {
    IASTVisitor *intp = NG::interpreter::interpreter();

    auto ast = parse(R"(
        val x = 1;
        val y = 2;
        val name = "ng";
        fun hello() {
          return "fuck";
        }
        val z = x + y;

        val g = hello();

        fun times(a, b) {
            return a * b;
        }

        val h = times(x, y);
    )");

    ast->accept(intp);

    auto isum = dynamic_cast<NG::interpreter::ISummarizable *>(intp);

    isum->summary();

    destroyast(ast);
}
