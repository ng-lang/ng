#pragma once

#include <fwd.hpp>
#include <ast.hpp>
#include <token.hpp>
#include <utility>

namespace NG::parsing
{

    /**
     * @brief Represents a parsing error.
     */
    struct ParseError
    {
        Token token;                   ///< The token that caused the error.
        Str message;                   ///< The error message.
        std::list<TokenType> expected; ///< The expected token types.
    };

    /**
     * @brief Represents the state of the lexer.
     */
    struct LexState
    {
        Str source;   ///< The source code.
        size_t size;  ///< The size of the source code.
        size_t index; ///< The current index in the source code.

        size_t line; ///< The current line number.
        size_t col;  ///< The current column number.

        explicit LexState(const Str &_source);

        /**
         * @brief Returns the current character.
         *
         * @return The current character.
         */
        [[nodiscard]] auto current() const -> char;

        /**
         * @brief Returns whether the end of the source code has been reached.
         *
         * @return `true` if the end of the source code has been reached, `false` otherwise.
         */
        [[nodiscard]] auto eof() const -> bool;

        /**
         * @brief Advances the lexer by `n` characters.
         *
         * @param n The number of characters to advance.
         */
        void next(int n = 1);

        /**
         * @brief Reverts the lexer by `n` characters.
         *
         * @param n The number of characters to revert.
         */
        void revert(size_t n = 1);

        /**
         * @brief Advances the lexer to the next line.
         */
        void nextLine();

        /**
         * @brief Extends the source code with more source code.
         *
         * @param source The source code to append.
         */
        void extend(const Str &source);

        /**
         * @brief Looks ahead one character without advancing the lexer.
         *
         * @return The next character.
         */
        [[nodiscard]] auto lookAhead() const -> char;
    };

    /**
     * @brief The lexer.
     */
    class Lexer : NonCopyable
    {
        LexState state;      ///< The state of the lexer.
        Vec<Token> tokens{}; ///< The tokens that have been lexed.

    public:
        explicit Lexer(LexState state) : state(std::move(state)) {}

        auto operator->() -> LexState *
        {
            return &state;
        }

        /**
         * @brief Lexes the source code.
         *
         * @return The lexed tokens.
         */
        auto lex() -> Vec<Token>;

        /**
         * @brief Returns the next token.
         *
         * @return The next token.
         */
        auto next() -> Token;
    };

    /// Returns true if `type` is an operator token usable in expressions.
    auto is_operator(TokenType type) -> bool;

    using NG::ast::ASTNode;

    /**
     * @brief Represents the state of the parser.
     */
    struct ParseState
    {
        Vec<Token> tokens; ///< The tokens to parse.
        size_t size;       ///< The number of tokens.
        size_t index;      ///< The current index in the tokens.

        explicit ParseState(const Vec<Token> &tokens);

        /**
         * @brief Returns the current token.
         *
         * @return The current token.
         */
        [[nodiscard]] auto current() const -> const Token &;

        auto operator->() -> const Token *
        {
            return &current();
        }

        /**
         * @brief Returns whether the end of the tokens has been reached.
         *
         * @return `true` if the end of the tokens has been reached, `false` otherwise.
         */
        [[nodiscard]] auto eof() const -> bool;

        /**
         * @brief Advances the parser by `n` tokens.
         *
         * @param n The number of tokens to advance.
         */
        void next(int n = 1);

        /**
         * @brief Reverts the parser by `n` tokens.
         *
         * @param n The number of tokens to revert.
         */
        void revert(size_t n = 1);

        /**
         * @brief Creates a parsing error.
         *
         * @param message The error message.
         * @param expected The expected token types.
         * @return The parsing error.
         */
        auto error(Str message, std::list<TokenType> expected = {}) -> ParseError
        {
            Token tok{};
            if (!eof())
            {
                tok = current();
            }
            else
            {
                tok.repr = "<eof>";
                // Keep position stable if possible.
                if (!tokens.empty())
                {
                    tok.position = tokens.back().position;
                }
            }
            return ParseError{
                .token = std::move(tok),
                .message = std::move(message),
                .expected = std::move(expected),
            };
        }
    };

    /**
     * @brief The parser.
     */
    struct Parser : NonCopyable
    {
        ParseState state;    ///< The state of the parser.
        Str module_filename; ///< The filename of the module being parsed.

    public:
        explicit Parser(ParseState state) : state(std::move(state)) {}

        /**
         * @brief Parses the tokens.
         *
         * @param filename The filename of the module being parsed.
         * @return The parsed AST.
         * @throws ParseException on syntax or lexical errors.
         */
        auto parse(const Str &filename = "[noname]") -> ast::ASTRef<ASTNode>;
    };

} // namespace NG
