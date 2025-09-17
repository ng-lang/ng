
#include "../test.hpp"

using namespace NG;
using namespace NG::parsing;

TEST_CASE("lexer should accept brackets", "[Lexer][Brackets]")
{
    Lexer lexer{LexState{"{[()]}"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 6);
    REQUIRE(tokens[0].repr == "{");
    REQUIRE(tokens[4].type == TokenType::RIGHT_SQUARE);
}

TEST_CASE("lexer should accept operators", "[Lexer][Operator]")
{
    Lexer lexer{LexState{">= <= 1 2 3 + * / *"}};
    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 9);
    REQUIRE(tokens[0].type == TokenType::GE);
    REQUIRE(tokens[2].type == TokenType::NUMBER);
    REQUIRE(tokens[3].repr == "2");
}

TEST_CASE("lexer should fail while met unrecorgnized operator", "[Lexer][Operator]")
{
    Lexer lexer{LexState{"<*>"}};
    REQUIRE_THROWS_MATCHES(lexer.lex(), LexException, MessageMatches(ContainsSubstring("Unknown operator: <*>")));
}

TEST_CASE("lexer should accept special symbols", "[Lexer][Symbol][Arrow][Sepeerator][Colon][Semicolon][Comma]")
{
    Lexer lexer{LexState{"=>  -> :: : ; ,"}};
    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 6);
    REQUIRE(tokens[0].type == TokenType::DUAL_ARROW);
    REQUIRE(tokens[1].type == TokenType::SINGLE_ARROW);
    REQUIRE(tokens[2].type == TokenType::SEPERATOR);
    REQUIRE(tokens[3].type == TokenType::COLON);
    REQUIRE(tokens[4].type == TokenType::SEMICOLON);
    REQUIRE(tokens[5].type == TokenType::COMMA);
}

TEST_CASE("lexer should lex builtin types", "[Lexer][Builtin][Type][Number]")
{
    Lexer lexer{LexState{R"(
int bool string float byte ubyte short ushort uint long ulong
u8 i8 u16 i16 u32 i32 u64 i64 uptr iptr 
half double quadruple
f16 f32 f64 f128
)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 28);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_INT);
    REQUIRE(tokens[1].type == TokenType::KEYWORD_BOOL);
    REQUIRE(tokens[2].type == TokenType::KEYWORD_STRING);
    REQUIRE(tokens[3].type == TokenType::KEYWORD_FLOAT);
    REQUIRE(tokens[4].type == TokenType::KEYWORD_BYTE);
    REQUIRE(tokens[5].type == TokenType::KEYWORD_UBYTE);
    REQUIRE(tokens[6].type == TokenType::KEYWORD_SHORT);
    REQUIRE(tokens[7].type == TokenType::KEYWORD_USHORT);
    REQUIRE(tokens[8].type == TokenType::KEYWORD_UINT);
    REQUIRE(tokens[9].type == TokenType::KEYWORD_LONG);
    REQUIRE(tokens[10].type == TokenType::KEYWORD_ULONG);
    REQUIRE(tokens[11].type == TokenType::KEYWORD_U8);
    REQUIRE(tokens[12].type == TokenType::KEYWORD_I8);
    REQUIRE(tokens[13].type == TokenType::KEYWORD_U16);
    REQUIRE(tokens[14].type == TokenType::KEYWORD_I16);
    REQUIRE(tokens[15].type == TokenType::KEYWORD_U32);
    REQUIRE(tokens[16].type == TokenType::KEYWORD_I32);
    REQUIRE(tokens[17].type == TokenType::KEYWORD_U64);
    REQUIRE(tokens[18].type == TokenType::KEYWORD_I64);
    REQUIRE(tokens[19].type == TokenType::KEYWORD_UPTR);
    REQUIRE(tokens[20].type == TokenType::KEYWORD_IPTR);
    REQUIRE(tokens[21].type == TokenType::KEYWORD_HALF);
    REQUIRE(tokens[22].type == TokenType::KEYWORD_DOUBLE);
    REQUIRE(tokens[23].type == TokenType::KEYWORD_QUADRUPLE);
    REQUIRE(tokens[24].type == TokenType::KEYWORD_F16);
    REQUIRE(tokens[25].type == TokenType::KEYWORD_F32);
    REQUIRE(tokens[26].type == TokenType::KEYWORD_F64);
    REQUIRE(tokens[27].type == TokenType::KEYWORD_F128);
}

TEST_CASE("lexer should lex identifier with underscore", "[Lexer][Identifier]")
{
    Lexer lexer{LexState{R"(some_identifier)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].type == TokenType::ID);
}