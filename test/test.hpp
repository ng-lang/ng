#pragma once

#include <catch2/catch_test_macros.hpp>

#include <ast.hpp>
#include <parser.hpp>
#include <debug.hpp>

#include <format>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

inline ParseResult<ASTRef<ASTNode>> parse(const Str &source, const Str &moduleName = "[noname]")
{
    // debug_log(source);
    auto astResult = Parser(ParseState(Lexer(LexState{source}).lex())).parse(moduleName);

    if (!astResult)
    {
        ParseError error = astResult.error();
        auto &&position = error.token.position;
        Str location = std::format("Location: {} / {}", position.line, position.col);

        debug_log("Error parse result:",
                  error.message,
                  location);
    }
    return astResult;
}
