
#include "../test.hpp"

using namespace NG;
using namespace NG::parsing;

TEST_CASE("lexer should accept symbols", "[Lexer][Symbol][Keyword][Identifier]")
{
  Lexer lexer{LexState{"type hello world"}};

  auto &&tokens = lexer.lex();
  REQUIRE(tokens.size() == 3);
  REQUIRE(tokens[0].type == TokenType::KEYWORD_TYPE);
  REQUIRE(tokens[1].type == TokenType::ID);
}

TEST_CASE("lexer should accept all keywords", "[Lexer][Keyword]")
{
  Lexer lexer{LexState{
    R"(
    type val sig fun cons
    module export import
    if else loop collect next
    return break continue
    unit true false
    exports
    property new in is
)"}};

  auto &&tokens = lexer.lex();
  REQUIRE(tokens.size() == 24);
  REQUIRE(tokens[0].type == TokenType::KEYWORD_TYPE);
  REQUIRE(tokens[1].type == TokenType::KEYWORD_VAL);
  REQUIRE(tokens[2].type == TokenType::KEYWORD_SIG);
  REQUIRE(tokens[3].type == TokenType::KEYWORD_FUN);
  REQUIRE(tokens[4].type == TokenType::KEYWORD_CONS);
  REQUIRE(tokens[5].type == TokenType::KEYWORD_MODULE);
  REQUIRE(tokens[6].type == TokenType::KEYWORD_EXPORT);
  REQUIRE(tokens[7].type == TokenType::KEYWORD_IMPORT);
  REQUIRE(tokens[8].type == TokenType::KEYWORD_IF);
  REQUIRE(tokens[9].type == TokenType::KEYWORD_ELSE);
  REQUIRE(tokens[10].type == TokenType::KEYWORD_LOOP);
  REQUIRE(tokens[11].type == TokenType::KEYWORD_COLLECT);
  REQUIRE(tokens[12].type == TokenType::KEYWORD_NEXT);
  REQUIRE(tokens[13].type == TokenType::KEYWORD_RETURN);
  REQUIRE(tokens[14].type == TokenType::KEYWORD_BREAK);
  REQUIRE(tokens[15].type == TokenType::KEYWORD_CONTINUE);
  REQUIRE(tokens[16].type == TokenType::KEYWORD_UNIT);
  REQUIRE(tokens[17].type == TokenType::KEYWORD_TRUE);
  REQUIRE(tokens[18].type == TokenType::KEYWORD_FALSE);
  REQUIRE(tokens[19].type == TokenType::KEYWORD_EXPORTS);
  REQUIRE(tokens[20].type == TokenType::KEYWORD_PROPERTY);
  REQUIRE(tokens[21].type == TokenType::KEYWORD_NEW);
  REQUIRE(tokens[22].type == TokenType::KEYWORD_IN);
  REQUIRE(tokens[23].type == TokenType::KEYWORD_IS);
}

TEST_CASE("lexer should accept property keyword", "[Lexer][Keyword][Property]")
{
  Lexer lexer{LexState{R"(property)"}};

  auto &&tokens = lexer.lex();

  REQUIRE(tokens.size() == 1);
  REQUIRE(tokens[0].type == TokenType::KEYWORD_PROPERTY);
}

TEST_CASE("lexer should accept new keyword", "[Lexer][Keyword][New]")
{
  Lexer lexer{LexState{R"(new)"}};

  auto &&tokens = lexer.lex();

  REQUIRE(tokens.size() == 1);
  REQUIRE(tokens[0].type == TokenType::KEYWORD_NEW);
}
