#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <ast.hpp>
#include <debug.hpp>
#include <parser.hpp>
#include <token.hpp>

#include <format>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;

inline ASTRef<ASTNode> parse(const Str &source, const Str &moduleName = "[noname]", const Str &errMsg = "")
{
  auto &&tokens = Lexer(LexState{source}).lex();
  // debug_log(source, tokens);

  try
  {
    auto ast = Parser(ParseState(tokens)).parse(moduleName);
    // debug_log(ast->repr());
    return ast;
  }
  catch (const ParseException &ex)
  {
    if (!errMsg.empty())
    {
      REQUIRE_THAT(ex.what(), ContainsSubstring(errMsg));
    }
    else
    {
      debug_log("Error parse result:", ex.what());
    }
    return {};
  }
  return nullptr;
}

inline ASTRef<ASTNode> parseInvalid(const Str &source, const Str &errMsg)
{
  return parse(source, "[noname]", errMsg);
}