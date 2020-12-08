
#ifndef __NG_TOKEN_HPP
#define __NG_TOKEN_HPP

#include <string>

namespace NG {

    using Str = std::string;

    enum class TokenType {
        NONE,
        KEYWORD,
        KEYWORD_TYPE,
        KEYWORD_FUN,
        KEYWORD_VAL,
        KEYWORD_SIG,
        KEYWORD_CONS,

        KEYWORD_PROPERTY,

        KEYWORD_MODULE,
        KEYWORD_EXPORT,
        KEYWORD_EXPORTS,
        KEYWORD_IMPORT,
        KEYWORD_USE,
        KEYWORD_NEW,

        KEYWORD_IF,
        KEYWORD_THEN,
        KEYWORD_ELSE,
        KEYWORD_LOOP,
        KEYWORD_COLLECT,
        KWYWORD_SWITCH,
        KEYWORD_CASE,
        KEYWORD_OTHERWISE,
        KEYWORD_RETURN,
        KEYWORD_BREAK,
        KEYWORD_CONTINUE,

        KEYWORD_TRUE,
        KEYWORD_FALSE,
        KEYWORD_UNIT,

        LEFT_PAREN,
        RIGHT_PAREN,
        LEFT_SQUARE,
        RIGHT_SQUARE,
        LEFT_CURLY,
        RIGHT_CURLY,

        DUAL_ARROW,   // =>
        SINGLE_ARROW, // ->
        SEPERATOR,    // ::
        COLON,        // :
        SEMICOLON,    // ;
        COMMA,        // ,
        DOT,          // .

        ID,
        OPERATOR,
        NUMBER,
        STRING,
        RESERVED,
    };

    enum class Operators {
        NONE,
        PLUS,    // +
        MINUS,   // -
        TIMES,   // *
        DIVIDE,  // /
        MODULUS, // %

        ASSIGN,    // =
        EQUAL,     // ==
        NOT_EQUAL, // !=
        GE,        // >=
        GT,        // >
        LE,        // <=
        LT,        // <

        LSHIFT, // <<
        RSHIFT, // >>

        UNKNOWN
    };

    struct TokenPosition {
        size_t line;
        size_t col;
    };

    struct Token {
        TokenType type;
        Str repr;
        TokenPosition position;
        Operators operatorType;

        bool operator==(const Token &t) const {
            return type == t.type &&
                   operatorType == t.operatorType &&
                   repr == t.repr;
        }
    };

} // namespace NG
#endif // __NG_HOKEN_HPP
