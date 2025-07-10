#pragma once

#include <fwd.hpp>
#include <ast.hpp>
#include <token.hpp>
#include <utility>
#include <list>

namespace NG::parsing
{

    struct ParseError
    {
        Token token;
        Str message;
        std::list<TokenType> expected;
    };

    template <class T>
    using ParseResult = std::expected<T, ParseError>;

    struct LexState
    {
        Str source;
        size_t size;
        size_t index;

        size_t line;
        size_t col;

        explicit LexState(const Str &_source);

        [[nodiscard]] auto current() const -> char;

        [[nodiscard]] auto eof() const -> bool;

        void next(int n = 1);

        void revert(size_t n = 1);

        void nextLine();

        [[nodiscard]] auto lookAhead() const -> char;
    };

    class Lexer : NonCopyable
    {
        LexState state;

    public:
        explicit Lexer(LexState state) : state(std::move(state)) {}

        auto operator->() -> LexState &
        {
            return state;
        }

        auto lex() -> Vec<Token>;
    };

    using NG::ast::ASTNode;

    struct ParseState
    {
        Vec<Token> tokens;
        size_t size;
        size_t index;

        explicit ParseState(const Vec<Token> &tokens);

        auto current() -> const Token &;

        auto operator->() -> const Token *
        {
            return &current();
        }

        [[nodiscard]] auto eof() const -> bool;

        void next(int n = 1);

        void revert(size_t n = 1);

        auto error(Str message, std::list<TokenType> expected = {}) -> ParseError
        {
            return ParseError{
                .token = current(),
                .message = std::move(message),
                .expected = std::move(expected),
            };
        }
    };

    struct Parser : NonCopyable
    {
        ParseState state;
        Str module_filename;

    public:
        explicit Parser(ParseState state) : state(std::move(state)) {}

        auto parse(const Str &filename = "untitled.ng") -> ParseResult<ast::ASTRef<ASTNode>>;
    };

} // namespace NG
