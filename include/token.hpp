#pragma once

#include <string>

namespace NG
{

    using Str = std::string;

    // NOLINTNEXTLINE(performance-enum-size)
    enum class TokenType : uint32_t
    {
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
        KEYWORD_NATIVE,

        KEYWORD_IF,
        KEYWORD_THEN,
        KEYWORD_ELSE,
        KEYWORD_LOOP,
        KEYWORD_COLLECT,
        KEYWORD_NEXT,
        KWYWORD_SWITCH,
        KEYWORD_CASE,
        KEYWORD_OTHERWISE,
        KEYWORD_RETURN,
        KEYWORD_BREAK,
        KEYWORD_CONTINUE,
        KEYWORD_IN,
        KEYWORD_IS,

        KEYWORD_TRUE,
        KEYWORD_FALSE,
        KEYWORD_UNIT,

        // types
        KEYWORD_INT,
        KEYWORD_BOOL,
        KEYWORD_STRING,
        KEYWORD_FLOAT,

        // integer variants
        KEYWORD_BYTE,
        KEYWORD_UBYTE,
        KEYWORD_SHORT,
        KEYWORD_USHORT,
        KEYWORD_UINT,
        KEYWORD_LONG,
        KEYWORD_ULONG,
        KEYWORD_U8,
        KEYWORD_I8,
        KEYWORD_U16,
        KEYWORD_I16,
        KEYWORD_U32,
        KEYWORD_I32,
        KEYWORD_U64,
        KEYWORD_I64,
        KEYWORD_UPTR,
        KEYWORD_IPTR,

        // floating point variants
        KEYWORD_HALF,
        KEYWORD_DOUBLE,
        KEYWORD_QUADRUPLE,
        KEYWORD_F16,
        KEYWORD_F32,
        KEYWORD_F64,
        KEYWORD_F128,

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
        INTEGRAL,
        FLOATING_POINT,
        NUMBER_U8,
        NUMBER_I8,
        NUMBER_U16,
        NUMBER_I16,
        NUMBER_U32,
        NUMBER_I32,
        NUMBER_U64,
        NUMBER_I64,
        NUMBER_F16,
        NUMBER_F32,
        NUMBER_F64,
        NUMBER_F128,
        STRING,
        RESERVED,
    };

    enum class Operators : uint8_t
    {
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

    struct TokenPosition
    {
        size_t line;
        size_t col;
    };

    struct Token
    {
        TokenType type;
        Str repr;
        TokenPosition position;
        Operators operatorType;

        auto operator==(const Token &token) const -> bool
        {
            return type == token.type &&
                   operatorType == token.operatorType &&
                   repr == token.repr;
        }
    };

} // namespace NG
