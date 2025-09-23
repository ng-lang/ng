
#include "../test.hpp"

using namespace NG;
using namespace NG::parsing;

TEST_CASE("lexer should accept tuple and range ralated tokens", "[Lexer][Token][Tuple][Range]")
{
    Lexer lexer{LexState{"a..b 1..=10 ...tup a.. ..b ^3..^1 (int, bool, float)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 24);
    REQUIRE(tokens[0].type == TokenType::ID);
    REQUIRE(tokens[1].type == TokenType::RANGE);
    REQUIRE(tokens[2].type == TokenType::ID);
    REQUIRE(tokens[3].type == TokenType::NUMBER);
    REQUIRE(tokens[4].type == TokenType::RANGE_INCLUSIVE);
    REQUIRE(tokens[5].type == TokenType::NUMBER);
    REQUIRE(tokens[6].type == TokenType::SPREAD);
    REQUIRE(tokens[7].type == TokenType::ID);
    REQUIRE(tokens[8].type == TokenType::ID);
    REQUIRE(tokens[9].type == TokenType::RANGE);
    REQUIRE(tokens[10].type == TokenType::RANGE);
    REQUIRE(tokens[11].type == TokenType::ID);
    REQUIRE(tokens[12].type == TokenType::CARET);
    REQUIRE(tokens[13].type == TokenType::NUMBER);
    REQUIRE(tokens[14].type == TokenType::RANGE);
    REQUIRE(tokens[15].type == TokenType::CARET);
    REQUIRE(tokens[16].type == TokenType::NUMBER);
    REQUIRE(tokens[17].type == TokenType::LEFT_PAREN);
    REQUIRE(tokens[18].type == TokenType::KEYWORD_INT);
    REQUIRE(tokens[19].type == TokenType::COMMA);
    REQUIRE(tokens[20].type == TokenType::KEYWORD_BOOL);
    REQUIRE(tokens[21].type == TokenType::COMMA);
    REQUIRE(tokens[22].type == TokenType::KEYWORD_FLOAT);
    REQUIRE(tokens[23].type == TokenType::RIGHT_PAREN);   
}
