#include "orgasm/lexer.hpp"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace ng::orgasm {

Lexer::Lexer(std::string_view source)
    : source_(source), pos_(0), line_(1), column_(1), peeked_token_() {}

char Lexer::current_char() const {
  if (pos_ >= source_.size()) {
    return '\0';
  }
  return source_[pos_];
}

char Lexer::peek_char(size_t offset) const {
  if (pos_ + offset >= source_.size()) {
    return '\0';
  }
  return source_[pos_ + offset];
}

void Lexer::advance() {
  if (pos_ < source_.size()) {
    if (source_[pos_] == '\n') {
      line_++;
      column_ = 1;
    } else {
      column_++;
    }
    pos_++;
  }
}

void Lexer::skip_whitespace() {
  while (pos_ < source_.size() &&
         (current_char() == ' ' || current_char() == '\t' ||
          current_char() == '\r' || current_char() == '\n')) {
    advance();
  }
}

void Lexer::skip_comment() {
  if (current_char() == '/' && peek_char() == '/') {
    // Single-line comment
    while (current_char() != '\n' && current_char() != '\0') {
      advance();
    }
  }
}

Token Lexer::make_token(TokenType type, std::string value) {
  return Token(type, std::move(value), line_, column_);
}

Token Lexer::read_number() {
  int start_column = column_;
  std::string num_str;
  bool is_float = false;

  // Handle negative numbers
  if (current_char() == '-') {
    num_str += current_char();
    advance();
  }

  while (std::isdigit(current_char()) || current_char() == '.' ||
         current_char() == 'u' || std::tolower(current_char()) == 'f' ||
         std::tolower(current_char()) == 'i') {
    if (current_char() == '.') {
      is_float = true;
    }
    num_str += current_char();
    advance();
  }

  TokenType type = is_float ? TokenType::FLOAT_NUMBER : TokenType::NUMBER;
  return Token(type, num_str, line_, start_column);
}

Token Lexer::read_string() {
  int start_column = column_;
  std::string str;

  advance(); // Skip opening '['

  while (current_char() != ']' && current_char() != '\0') {
    str += current_char();
    advance();
  }

  if (current_char() == ']') {
    advance(); // Skip closing ']'
  }

  return Token(TokenType::STRING_LITERAL, str, line_, start_column);
}

TokenType Lexer::classify_keyword(const std::string &word) {
  static const std::unordered_map<std::string, TokenType> keywords = {
      // Directives
      {"module", TokenType::MODULE},
      {"endmodule", TokenType::ENDMODULE},
      {"symbols", TokenType::SYMBOLS},
      {"import", TokenType::IMPORT},
      {"export", TokenType::EXPORT},
      {"const", TokenType::CONST},
      {"str", TokenType::STR},
      {"array", TokenType::ARRAY},
      {"val", TokenType::VAL},
      {"fun", TokenType::FUN},
      {"endfun", TokenType::ENDFUN},
      {"param", TokenType::PARAM},
      {"start", TokenType::START},
      {"end", TokenType::END},
      {"label", TokenType::LABEL},
  };

  auto it = keywords.find(word);
  return it != keywords.end() ? it->second : TokenType::IDENTIFIER;
}

Token Lexer::read_identifier_or_keyword() {
  int start_column = column_;
  std::string id;

  // Read identifier (can contain letters, digits, underscores, and dots for
  // type suffixes)
  while (std::isalnum(current_char()) || current_char() == '_' ||
         current_char() == '.') {
    id += current_char();
    advance();
  }

  TokenType type = classify_keyword(id);
  return Token(type, id, line_, start_column);
}

Token Lexer::next_token() {
  if (peeked_token_) {
    Token t = *peeked_token_;
    peeked_token_ = std::nullopt;
    return t;
  }

  skip_whitespace();

  if (pos_ >= source_.size()) {
    return make_token(TokenType::EOF_TOKEN, "");
  }

  // Skip comments
  if (current_char() == '/' && peek_char() == '/') {
    skip_comment();
    skip_whitespace();
  }

  if (pos_ >= source_.size()) {
    return make_token(TokenType::EOF_TOKEN, "");
  }

  char ch = current_char();
  int start_column = column_;

  // Single-character tokens
  switch (ch) {
  case '.': {
    advance();
    return Token(TokenType::DOT, ".", line_, start_column);
  }
  case ',':
    advance();
    return Token(TokenType::COMMA, ",", line_, start_column);
  case ':':
    advance();
    return Token(TokenType::COLON, ":", line_, start_column);
  case ';':
    advance();
    return Token(TokenType::SEMICOLON, ";", line_, start_column);
  case '[':
    advance();
    return Token(TokenType::LBRACKET, "[", line_, start_column);
  case ']':
    advance();
    return Token(TokenType::RBRACKET, "]", line_, start_column);
  case '(':
    advance();
    return Token(TokenType::LPAREN, "(", line_, start_column);
  case ')':
    advance();
    return Token(TokenType::RPAREN, ")", line_, start_column);
  case '+':
    advance();
    return Token(TokenType::PLUS, "+", line_, start_column);
  case '-':
    // Could be a negative number or a minus sign
    if (std::isdigit(peek_char())) {
      return read_number();
    }
    advance();
    return Token(TokenType::MINUS, "-", line_, start_column);
  }

  // Numbers
  if (std::isdigit(ch)) {
    return read_number();
  }

  // Identifiers and keywords
  if (std::isalpha(ch) || ch == '_') {
    return read_identifier_or_keyword();
  }

  // Unknown character
  advance();
  return Token(TokenType::ERROR, std::string(1, ch), line_, start_column);
}

Token Lexer::peek_token() {
  if (!peeked_token_) {
    peeked_token_ = next_token();
  }
  return *peeked_token_;
}

bool Lexer::has_more_tokens() const {
  return pos_ < source_.size() || peeked_token_.has_value();
}

} // namespace ng::orgasm
