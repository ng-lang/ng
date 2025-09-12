
#include "../test.hpp"

using namespace NG;
using namespace NG::parsing;

TEST_CASE("lexer should accept tokens", "[Lexer][Token]")
{
    LexState state{"hello world"};

    REQUIRE(state.size == 11);
    REQUIRE(state.source == "hello world");

    state.index = 11;

    REQUIRE(state.eof());
    REQUIRE(!state.current());
    REQUIRE(state.lookAhead() == '\0');
}

TEST_CASE("lexer should reject unexpected tokens", "[Lexer][Token][Failure]")
{
    Lexer lexer{LexState{"@"}};

    REQUIRE_THROWS_MATCHES(lexer.lex(), LexException,
                           MessageMatches(ContainsSubstring("Unknown token")));
}

TEST_CASE("lexer should produce correct positions", "[Lexer][Position][Line][Column]")
{
    Lexer lexer{LexState{">> \n   123\n\n\r\n Bc def"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 4);
    REQUIRE(tokens[0].position.line == 1);
    REQUIRE(tokens[0].position.col == 1);
    REQUIRE(tokens[1].position.line == 2);
    REQUIRE(tokens[1].position.col == 4);
    REQUIRE(tokens[2].position.line == 5);
    REQUIRE(tokens[2].position.col == 2);
    REQUIRE(tokens[3].position.line == 5);
    REQUIRE(tokens[3].position.col == 5);
}

TEST_CASE("lexer should accept simple function", "[Lexer][Function][Definition]")
{
    Lexer lexer{LexState{"fun id(n) { return n; }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 10);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept argument list", "[Lexer][Function][Argument]")
{

    Lexer lexer{LexState{"fun swap(x, y) { return (y, x); }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 16);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept arithmetic operation", "[Lexer][Operator][Arithmetic][Function]")
{
    Lexer lexer{LexState{"fun add(a, b) { return a + b * a - b * (a - b/a); }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 26);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept assignment", "[Lexer][Assignment]")
{
    Lexer lexer{LexState{"fun id(n) { val x = n; return x; }"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 15);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should lex comment", "[Lexer][Comment][Identifier]")
{
    Lexer lexer{LexState{R"(
#!shebang like comment
// comment
hello /* multiline comment*/
/**
 *  multiline comment
 */
// comment
)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].type == TokenType::ID);
}

TEST_CASE("lexer should lex single line comment without newline", "[Lexer][Comment][Identifier]")
{
    REQUIRE(Lexer{LexState{"// comment"}}.lex().size() == 0);

    REQUIRE(Lexer{LexState{"# comment"}}.lex().size() == 0);
}

TEST_CASE("lexer should fail with invalid multiline comment", "[Lexer][Comment][Identifier][Failure]")
{
    REQUIRE_THROWS_MATCHES(Lexer{LexState{"/*/"}}.lex(), LexException,
                           MessageMatches(ContainsSubstring("Unterminated block comment")));

    REQUIRE_THROWS_MATCHES(Lexer{LexState{"/*"}}.lex(), LexException,
                           MessageMatches(ContainsSubstring("Unterminated block comment")));
}
