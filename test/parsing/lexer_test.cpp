
#include "../catch.hpp"
#include "../test.hpp"

using namespace NG;
using namespace NG::parsing;

TEST_CASE("lexer should accept tokens", "[LexerTest]") {
    LexState state{"hello world"};

    REQUIRE(state.size == 11);
    REQUIRE(state.source == "hello world");

    state.index = 11;

    REQUIRE(state.eof());
    REQUIRE(!state.current());
}

TEST_CASE("lexer should accept symbols", "[LexerTest]") {
    Lexer lexer{LexState{"type hello world"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_TYPE);
    REQUIRE(tokens[1].type == TokenType::ID);
}

TEST_CASE("lexer should accept all keywords", "[LexerTest]") {
    Lexer lexer{LexState{
            R"(
    type val sig fun cons
    module export import
    if else loop collect case
    return break continue
    unit true false
    exports
    property new
)"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 22);
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
    REQUIRE(tokens[12].type == TokenType::KEYWORD_CASE);
    REQUIRE(tokens[13].type == TokenType::KEYWORD_RETURN);
    REQUIRE(tokens[14].type == TokenType::KEYWORD_BREAK);
    REQUIRE(tokens[15].type == TokenType::KEYWORD_CONTINUE);
    REQUIRE(tokens[16].type == TokenType::KEYWORD_UNIT);
    REQUIRE(tokens[17].type == TokenType::KEYWORD_TRUE);
    REQUIRE(tokens[18].type == TokenType::KEYWORD_FALSE);
    REQUIRE(tokens[19].type == TokenType::KEYWORD_EXPORTS);
    REQUIRE(tokens[20].type == TokenType::KEYWORD_PROPERTY);
    REQUIRE(tokens[21].type == TokenType::KEYWORD_NEW);
}

TEST_CASE("lexer should accept numbers", "[LexerTest]") {
    Lexer lexer{LexState{"1 11 123"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].type == TokenType::NUMBER);
    REQUIRE(tokens[2].repr == "123");
}

TEST_CASE("lexer should accept brackets", "[LexerTest]") {
    Lexer lexer{LexState{"{[()]}"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 6);
    REQUIRE(tokens[0].repr == "{");
    REQUIRE(tokens[4].type == TokenType::RIGHT_SQUARE);
}

TEST_CASE("lexer should accept operators", "[LexerTest]") {
    Lexer lexer{LexState{">= <*> <= 1 2 3 + * / *"}};
    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 10);
    REQUIRE(tokens[0].type == TokenType::OPERATOR);
    REQUIRE(tokens[0].operatorType == Operators::GE);
    REQUIRE(tokens[1].repr == "<*>");
    REQUIRE(tokens[1].operatorType == Operators::UNKNOWN);
    REQUIRE(tokens[3].type == TokenType::NUMBER);
    REQUIRE(tokens[4].repr == "2");
}

TEST_CASE("lexer should produce correct positions", "[LexerTest]") {
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

TEST_CASE("lexer should accept special symbols", "[LexerTest]") {
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

TEST_CASE("lexer should accept simple function", "[LexerTest]") {
    Lexer lexer{LexState{"fun id(n) { return n; }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 10);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept argument list", "[LexerTest]") {

    Lexer lexer{LexState{"fun swap(x, y) { return (y, x); }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 16);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept arithmetic operation", "[LexerTest]") {
    Lexer lexer{LexState{"fun add(a, b) { return a + b * a - b * (a - b/a); }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 26);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept number expression", "[LexerTest]") {
    Lexer lexer{LexState{"fun ones() { return 1; }"}};
    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 9);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept assignment", "[LexerTest]") {
    Lexer lexer{LexState{"fun id(n) { val x = n; return x; }"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 15);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_FUN);
}

TEST_CASE("lexer should accept basic string", "[LexerTest]") {
    Lexer lexer{LexState{R"( "abc" "def" "ghi" "\x86\xc0\xde" )"}};

    auto &&tokens = lexer.lex();
    REQUIRE(tokens.size() == 4);
    for (auto &&token: tokens) {
        REQUIRE(token.repr.size() == 3);
    }
}

TEST_CASE("lexer should accept string with blanks", "[LexerTest]") {
    Lexer lexer{LexState{R"( " how are you" )"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 1);

    REQUIRE(tokens[0].repr.size() == 12);
}

TEST_CASE("lexer should accept array indexing expr", "[LexerTest]") {
    Lexer lexer{LexState{R"( [1, 2, 3, "Hello" ] )"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 9);
}

TEST_CASE("lexer should accept property keyword", "[LexerTest]") {
    Lexer lexer{LexState{R"(property)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_PROPERTY);
}


TEST_CASE("lexer should accept new keyword", "[LexerTest]") {
    Lexer lexer{LexState{R"(new)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].type == TokenType::KEYWORD_NEW);
}

TEST_CASE("lexer should lex comment", "[LexerTest]") {
    Lexer lexer{LexState{R"(
// comment
hello
// comment
)"}};

    auto &&tokens = lexer.lex();

    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].type == TokenType::ID);
}