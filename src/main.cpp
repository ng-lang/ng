

#include "ast.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <fstream>
#include <iostream>
#include <streambuf>
#include <filesystem>
#include <cstdlib>
#include <functional>
#include <unistd.h>
#include "intp/intp.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define STDIN_FILENO 0
#else
#include <unistd.h>
#endif

static inline auto parse(const Str &source, const Str &file = "[noname]") -> ParseResult<ASTRef<ASTNode>>
{
    return Parser(ParseState(Lexer(LexState{source}).lex())).parse(file);
}

// NOLINTNEXTLINE(bugprone-exception-escape)

auto showHint() -> bool
{
    return isatty(STDIN_FILENO);
}

auto repl() -> int
{

    if (showHint())
    {
        std::cout << ">> ";
    }
    std::string line{};
    std::getline(std::cin, line);
    std::string source{line};
    LexState state{source};
    Lexer lexer{LexState{line}};
    NG::intp::Interpreter *stupid = NG::intp::stupid();

    Token current;
    Vec<Token> tokens;
    Vec<ASTRef<ASTNode>> histories;

    bool shouldRestart = true;
    while (true)
    {
        shouldRestart = false;
        int braceCount = 0;
        tokens.clear();
        while (true)
        {
            if (lexer->eof())
            {
                if (showHint())
                {
                    std::cout << ">> ";
                }
                if (std::getline(std::cin, line))
                {
                    if (std::find_if(begin(line), end(line), std::not_fn([](char &c) -> bool
                                                                         { return std::isblank(c); })) != line.end())
                    {
                        lexer->extend(line);
                    }
                }
                else
                {
                    exit(0);
                }
            }
            current = lexer.next();
            if (current.type == TokenType::NONE)
            {
                continue;
            }
            tokens.push_back(current);
            if (current.type == TokenType::LEFT_CURLY)
            {
                braceCount++;
            }
            if (current.type == TokenType::RIGHT_CURLY)
            {
                braceCount--;
                if (braceCount < 0)
                {
                    debug_log("Invalid input, unbalanced curly brackets");
                    shouldRestart = true;
                    break;
                }
            }
            if (current.type == TokenType::SEMICOLON && braceCount == 0)
            {
                break;
            }
        }
        if (shouldRestart)
        {
            continue;
        }

        ParseState parse_state{tokens};
        auto ast = Parser(parse_state).parse("[interperter]");

        if (!ast)
        {
            debug_log("Syntax error:", ast.error().token, ast.error().message);
            continue;
        }
        tokens.clear();

        try
        {
            (*ast)->accept(stupid);
            histories.push_back(*ast);
        }
        catch (NG::RuntimeException ex)
        {
            debug_log("Runtime error", ex.what());
        }
    }

    for (auto ast : histories)
    {
        destroyast(ast);
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
