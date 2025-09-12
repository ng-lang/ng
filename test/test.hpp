#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include <ast.hpp>
#include <parser.hpp>
#include <debug.hpp>

#include <format>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;

inline ParseResult<ASTRef<ASTNode>> parse(const Str &source, const Str &moduleName = "[noname]", const Str &errMsg = "")
{
    auto &&tokens = Lexer(LexState{source}).lex();
    // debug_log(source, tokens);
    auto astResult = Parser(ParseState(tokens)).parse(moduleName);

    if (!astResult)
    {
        ParseError error = astResult.error();
        auto &&position = error.token.position;
        Str location = std::format("Location: {} / {}", position.line, position.col);

        if (!errMsg.empty())
        {
            REQUIRE_THAT(error.message, ContainsSubstring(errMsg));
        }
        else
        {
            debug_log("Error parse result:",
                      error.message,
                      location);
        }
    }
    // debug_log("Parsed ast", (*astResult)->repr());
    return astResult;
}

inline ParseResult<ASTRef<ASTNode>> parseInvalid(const Str &source, const Str &errMsg)
{
    return parse(source, "[noname]", errMsg);
}