#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ng::orgasm {

enum class TokenType {
  // Directives
  MODULE,
  ENDMODULE,
  SYMBOLS,
  IMPORT,
  EXPORT,
  CONST,
  STR,
  ARRAY,
  VAL,
  FUN,
  ENDFUN,
  PARAM,
  START,
  END,
  LABEL,

  // Instructions (will be parsed as identifiers first, then categorized)
  IDENTIFIER,

  // Operands
  NUMBER,
  FLOAT_NUMBER,
  STRING_LITERAL,
  TYPE,

  // Punctuation
  DOT,
  COMMA,
  COLON,
  SEMICOLON,
  LBRACKET,
  RBRACKET,
  LPAREN,
  RPAREN,
  PLUS,
  MINUS,

  // Special
  COMMENT,
  NEWLINE,
  EOF_TOKEN,
  ERROR,
};

struct Token {
  TokenType type;
  std::string value;
  int line;
  int column;

  Token(TokenType t, std::string v, int l, int c)
      : type(t), value(std::move(v)), line(l), column(c) {}
};

class Lexer {
public:
  explicit Lexer(std::string_view source);

  Token next_token();
  Token peek_token();
  bool has_more_tokens() const;

private:
  std::string_view source_;
  size_t pos_;
  int line_;
  int column_;
  std::optional<Token> peeked_token_;

  char current_char() const;
  char peek_char(size_t offset = 1) const;
  void advance();
  void skip_whitespace();
  void skip_comment();

  Token read_number();
  Token read_string();
  Token read_identifier_or_keyword();
  TokenType classify_keyword(const std::string &word);
  Token make_token(TokenType type, std::string value);
};

} // namespace ng::orgasm
