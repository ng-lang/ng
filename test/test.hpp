
#ifndef __NG_TEST_HPP

#define __NG_TEST_HPP

#include <catch2/catch_test_macros.hpp>

#include <ast.hpp>
#include <parser.hpp>
#include <intp/intp.hpp>
#include <intp/runtime.hpp>
#include <token.hpp>
#include <debug.hpp>
#include <typecheck/typecheck.hpp>

#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <format>
#include <filesystem>
#include <regex>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;
using namespace NG::intp;
using namespace NG::runtime;

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

#endif // __NG_TEST_HPP
