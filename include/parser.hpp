
#ifndef __NG_PARSER_HPP
#define __NG_PARSER_HPP

#include <fwd.hpp>
#include <ast.hpp>
#include <token.hpp>
#include <utility>

namespace NG::parsing {

    struct LexState {
        const Str source;
        const size_t size;
        size_t index;

        size_t line;
        size_t col;

        explicit LexState(const Str &_source);

        [[nodiscard]] char current() const;

        [[nodiscard]] bool eof() const;

        void next(int n = 1);

        void revert(size_t n = 1);

        void nextLine();

        [[nodiscard]] char lookAhead() const;
    };

    class Lexer {
        LexState state;

    public:
        explicit Lexer(LexState state) : state(std::move(state)) {}

        LexState &operator->() {
            return state;
        }

        Lexer(const Lexer &) = delete;

        Vec <Token> lex();
    };

    using NG::ast::ASTNode;

    struct ParseState {
        const Vec <Token> tokens;
        const size_t size;
        size_t index;

        explicit ParseState(const Vec <Token> &source);

        const Token &current();

        const Token *operator->() {
            return &current();
        }

        bool eof();

        void next(int n = 1);

        void revert(size_t n = 1);
    };

    struct Parser {
        ParseState state;
        Str module_filename;

    public:
        explicit Parser(ParseState state) : state(std::move(state)) {}

        Parser(const Parser &) = delete;

        ParseResult<ast::ASTRef <ASTNode>> parse(const Str &filename = "untitled.ng");
    };

} // namespace NG

#endif // __NG_PARSER_HPP
