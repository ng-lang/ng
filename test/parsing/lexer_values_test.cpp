
#include "../test.hpp"

using namespace NG;
using namespace NG::parsing;

TEST_CASE("lexer should accept number expression", "[Lexer][Number]")
{
    Lexer lexer{LexState{"fun ones() { return 1; }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 9);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept basic string", "[Lexer][String][EscapeSequence]")
{
    Lexer lexer{LexState{R"( "abc" "def" "ghi" "\x86\xc0\xde" )"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 4);
    for (auto &&token : tokens)
    {
        REQUIRE(token.repr.size() == 3);
    }
}

TEST_CASE("lexer should accept string with blanks", "[Lexer][String]")
{
    Lexer lexer{LexState{R"( " how are you" )"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 1);

    REQUIRE(tokens[0].repr.size() == 12);
}

TEST_CASE("lexer should accept array indexing expr", "[Lexer][Expression][Array]")
{
    Lexer lexer{LexState{R"( [1, 2, 3, "Hello" ] )"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 9);
}

TEST_CASE("lexer should accept numbers", "[Lexer][Number]")
{
    Lexer lexer{LexState{"1 11 123"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].type == TokenType::NUMBER);
    REQUIRE(tokens[2].repr == "123");
}

TEST_CASE("lexer should lex numbers with types and floating points", "[Lexer][Numbers][Literal]")
{
    Lexer lexer{LexState{R"(
123 1234 123_456 123.456_789 
123i8 123u8 123u16 123i16
123i32 123u32 123u64 123i64
123f16 123f32 123f64 123f128
123.0f16 123.0f32 123.0f64 123.0f128
1e1f16 1e1f32 1e1f64 1e1f128
)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 24);
    REQUIRE(tokens[0].type == TokenType::NUMBER);
    REQUIRE(tokens[1].type == TokenType::NUMBER);
    REQUIRE(tokens[2].type == TokenType::NUMBER);
    REQUIRE(tokens[3].type == TokenType::FLOATING_POINT);
    REQUIRE(tokens[4].type == TokenType::NUMBER_I8);
    REQUIRE(tokens[5].type == TokenType::NUMBER_U8);
    REQUIRE(tokens[6].type == TokenType::NUMBER_U16);
    REQUIRE(tokens[7].type == TokenType::NUMBER_I16);
    REQUIRE(tokens[8].type == TokenType::NUMBER_I32);
    REQUIRE(tokens[9].type == TokenType::NUMBER_U32);
    REQUIRE(tokens[10].type == TokenType::NUMBER_U64);
    REQUIRE(tokens[11].type == TokenType::NUMBER_I64);
    REQUIRE(tokens[12].type == TokenType::NUMBER_F16);
    REQUIRE(tokens[13].type == TokenType::NUMBER_F32);
    REQUIRE(tokens[14].type == TokenType::NUMBER_F64);
    REQUIRE(tokens[15].type == TokenType::NUMBER_F128);
    REQUIRE(tokens[16].type == TokenType::NUMBER_F16);
    REQUIRE(tokens[17].type == TokenType::NUMBER_F32);
    REQUIRE(tokens[18].type == TokenType::NUMBER_F64);
    REQUIRE(tokens[19].type == TokenType::NUMBER_F128);
    REQUIRE(tokens[20].type == TokenType::NUMBER_F16);
    REQUIRE(tokens[21].type == TokenType::NUMBER_F32);
    REQUIRE(tokens[22].type == TokenType::NUMBER_F64);
    REQUIRE(tokens[23].type == TokenType::NUMBER_F128);
}
