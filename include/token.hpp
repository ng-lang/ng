#pragma once

#include "common.hpp"
#include <string>

namespace NG
{

    using Str = std::string;

    /**
     * @brief The type of a token.
     */
    // NOLINTNEXTLINE(performance-enum-size)
    enum class TokenType : uint32_t
    {
        NONE,
        KEYWORD = 0x0100,
        KEYWORD_TYPE,
        KEYWORD_FUN,
        KEYWORD_VAL,
        KEYWORD_SIG,
        KEYWORD_CONS,

        KEYWORD_PROPERTY,

        KEYWORD_MODULE = 0x0200,
        KEYWORD_EXPORT,
        KEYWORD_EXPORTS,
        KEYWORD_IMPORT,
        KEYWORD_USE,
        KEYWORD_NEW,
        KEYWORD_NATIVE,

        KEYWORD_IF = 0x0300,
        KEYWORD_THEN,
        KEYWORD_ELSE,
        KEYWORD_LOOP,
        KEYWORD_COLLECT,
        KEYWORD_NEXT,
        KEYWORD_SWITCH,
        KEYWORD_CASE,
        KEYWORD_OTHERWISE,
        KEYWORD_RETURN,
        KEYWORD_BREAK,
        KEYWORD_CONTINUE,
        KEYWORD_IN,
        KEYWORD_IS,

        KEYWORD_TRUE = 0x0400,
        KEYWORD_FALSE,
        KEYWORD_UNIT,

        // types
        KEYWORD_INT = 0x0500,
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

        LEFT_PAREN = 0x0A00,
        RIGHT_PAREN,
        LEFT_SQUARE,
        RIGHT_SQUARE,
        LEFT_CURLY,
        RIGHT_CURLY,

        DUAL_ARROW,   // =>
        SINGLE_ARROW, // ->
        ASSIGN_EQUAL, // :=
        SEPARATOR,    // ::
        COLON,        // :
        SEMICOLON,    // ;
        COMMA,        // ,
        DOT,          // .

        PLUS,    // +
        MINUS,   // -
        TIMES,   // *
        DIVIDE,  // /
        MODULUS, // %

        BIND,      // =
        EQUAL,     // ==
        NOT_EQUAL, // !=
        GE,        // >=
        GT,        // >
        LE,        // <=
        LT,        // <

        LSHIFT, // <<
        RSHIFT, // >>

        AMPERSAND, // &
        PIPE,      // |
        CARET,     // ^
        TILDE,     // ~

        AT,       // @
        DOLLAR,   // $
        BACKTICK, // `
        HASHTAG,  // #

        AND, // &&
        OR,  // ||

        NOT,       // !
        QUERY,     // ?
        UNDEFINED, // ???

        ID,
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

    /**
     * @brief The position of a token in the source code.
     */
    struct TokenPosition
    {
        size_t line; ///< The line number.
        size_t col;  ///< The column number.
    };

    /**
     * @brief A token.
     */
    struct Token
    {
        TokenType type;         ///< The type of the token.
        Str repr;               ///< The representation of the token.
        TokenPosition position; ///< The position of the token.

        auto operator==(const Token &token) const -> bool
        {
            return type == token.type &&
                   repr == token.repr;
        }
    };

    /**
     * @brief Writes a token to a stream.
     *
     * @param stream The stream to write to.
     * @param token The token to write.
     * @return The stream.
     */
    auto operator<<(std::ostream &stream, const Token &token) -> std::ostream &;

} // namespace NG
