

#include "ast.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <fstream>
#include <iostream>
#include <streambuf>
#include <filesystem>
#include "intp/intp.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

static inline auto parse(const Str &source, const Str &file = "[noname]") -> ParseResult<ASTRef<ASTNode>>
{
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse(file);
}

// NOLINTNEXTLINE(bugprone-exception-escape)

auto repl() -> int
{

    std::cout << ">> ";
    std::string line{};
    std::getline(std::cin, line);
    std::string source{line};
    LexState state{source};
    Lexer lexer{LexState{line}};
    NG::intp::Interpreter *stupid = NG::intp::stupid();

    Token current;
    Vec<Token> tokens;
    while (true)
    {
        while (true)
        {
            if (lexer->eof())
            {
                std::cout << ">> ";
                if (std::getline(std::cin, line))
                {

                    lexer->extend(line);
                }
                else
                {
                    exit(0);
                }
            }
            current = lexer.next();
            tokens.push_back(current);
            if (current.type == TokenType::SEMICOLON)
            {
                break;
            }
        }

        debug_log("Tokens", tokens);

        ParseState parse_state{tokens};
        auto ast = Parser(parse_state).parse("[interperter]");

        if (!ast)
        {
            debug_log("Syntax error:", ast.error().token, ast.error().message);
            continue;
        }
        tokens.clear();

        (*ast)->accept(stupid);

        destroyast(*ast);

        stupid->summary();
    }
}
auto main(int argc, char *argv[]) -> int
{

    if (argc < 2)
    {
        return repl();
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (!std::filesystem::exists(argv[1]))
    {
        std::cout << "file " << argv[1] << " not found";
        return -1;
    }

    std::string filename{argv[1]};
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    std::ifstream file{filename};
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto astResult = parse(source, filename);
    if (!astResult)
    {
        std::cout << "Error parsing file: " << astResult.error().message << '\n';
        return -1;
    }

    auto &ast = *astResult;

    NG::intp::Interpreter *stupid = NG::intp::stupid();

    ast->accept(stupid);

    destroyast(ast);
}
