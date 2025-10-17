#include <catch2/catch_test_macros.hpp>
#include "orgasm/lexer.hpp"

using namespace ng::orgasm;

TEST_CASE("ORGASM lexer should tokenize directives", "[orgasm][lexer]") {
  std::string source = ".module test\n.endmodule";
  Lexer lexer(source);

  Token tok1 = lexer.next_token();
  REQUIRE(tok1.type == TokenType::DOT);

  Token tok2 = lexer.next_token();
  REQUIRE(tok2.type == TokenType::MODULE);

  Token tok3 = lexer.next_token();
  REQUIRE(tok3.type == TokenType::IDENTIFIER);
  REQUIRE(tok3.value == "test");

  Token tok4 = lexer.next_token();
  REQUIRE(tok4.type == TokenType::DOT);

  Token tok5 = lexer.next_token();
  REQUIRE(tok5.type == TokenType::ENDMODULE);
}

TEST_CASE("ORGASM lexer should tokenize symbols", "[orgasm][lexer]") {
  std::string source = ".symbols [id, n, print]";
  Lexer lexer(source);

  lexer.next_token(); // .
  lexer.next_token(); // symbols

  Token tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::LBRACKET);

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::IDENTIFIER);
  REQUIRE(tok.value == "id");

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::COMMA);
}

TEST_CASE("ORGASM lexer should tokenize constants", "[orgasm][lexer]") {
  std::string source = ".const i32 42\n.const f64 3.14";
  Lexer lexer(source);

  lexer.next_token(); // .
  lexer.next_token(); // const

  Token tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::IDENTIFIER);
  REQUIRE(tok.value == "i32");

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::NUMBER);
  REQUIRE(tok.value == "42");

  lexer.next_token(); // .
  lexer.next_token(); // const

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::IDENTIFIER);
  REQUIRE(tok.value == "f64");

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::FLOAT_NUMBER);
  REQUIRE(tok.value == "3.14");
}

TEST_CASE("ORGASM lexer should tokenize string literals", "[orgasm][lexer]") {
  std::string source = ".str [hello world]";
  Lexer lexer(source);

  lexer.next_token(); // .
  lexer.next_token(); // str

  Token tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::LBRACKET);
  
  // String content as identifier
  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::IDENTIFIER);
  REQUIRE(tok.value == "hello");
  
  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::IDENTIFIER);
  REQUIRE(tok.value == "world");
  
  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::RBRACKET);
}

TEST_CASE("ORGASM lexer should tokenize instructions", "[orgasm][lexer]") {
  std::string source = "00:    load_const.i32 const.0";
  Lexer lexer(source);

  Token tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::NUMBER);
  REQUIRE(tok.value == "00");

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::COLON);

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::IDENTIFIER);
  REQUIRE(tok.value == "load_const.i32");
}

TEST_CASE("ORGASM lexer should skip comments", "[orgasm][lexer]") {
  std::string source = "// This is a comment\n.module test";
  Lexer lexer(source);

  Token tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::DOT);

  tok = lexer.next_token();
  REQUIRE(tok.type == TokenType::MODULE);
}
