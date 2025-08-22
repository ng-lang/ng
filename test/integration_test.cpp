
#include "test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::intp;
using namespace NG::parsing;

static inline ParseResult<ASTRef<ASTNode>> parse(const Str &source, const Str &module_filename)
{
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse(module_filename);
}

static inline void
runIntegrationTest(const std::string &filename)
{
    std::ifstream file(filename);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    auto astResult = parse(source, filename);

    if (!astResult)
    {
        ParseError error = astResult.error();
        auto &&position = error.token.position;
        Str location = std::format("Location: {} / {}", position.line, position.col);

        debug_log("Error parse result:",
                  error.message,
                  location);
    }

    REQUIRE(astResult.has_value());

    auto &ast = *astResult;

    Interpreter *intp = NG::intp::stupid();

    ast->accept(intp);

    intp->summary();

    delete intp;
    destroyast(ast);
}

TEST_CASE("should run with id function definition", "[Integration]")
{
    runIntegrationTest("example/01.id.ng");
}

TEST_CASE("should run with many definitions", "[Integration]")
{
    runIntegrationTest("example/02.many_defs.ng");
}

TEST_CASE("should run with function calls", "[Integration]")
{
    runIntegrationTest("example/03.funcall_and_idexpr.ng");
}

TEST_CASE("should run with string concat", "[Integration]")
{
    runIntegrationTest("example/04.str.ng");
}

TEST_CASE("should run with valdefs -- function and members", "[Integration]")
{
    runIntegrationTest("example/05.valdef.ng");
}

TEST_CASE("should run with type definition and object creation", "[Integration]")
{
    runIntegrationTest("example/07.object.ng");
}

TEST_CASE("should run with array", "[Integration]")
{
    runIntegrationTest("example/06.array.ng");
}

TEST_CASE("should run with multiple modules import and export", "[Integration]")
{
    runIntegrationTest("example/08.imports.ng");
}

TEST_CASE("should run with multi level scopes", "[Integration]")
{
    runIntegrationTest("example/09.scope.ng");
}
