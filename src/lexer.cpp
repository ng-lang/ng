
#include "parser.hpp"
#include "token.hpp"
#include "common.hpp"
#include <sstream>
#include <unordered_map>
#include <functional>
#include <array>
#include <cctype>
#include <algorithm>

namespace NG::Parsing {

    template<class K, class V>
    using Map = std::unordered_map<K, V>;

    using Stream = std::stringstream;

    constexpr std::array<char, 6> brackets{'(', ')', '{', '}', '[', ']'};

    constexpr std::array<char, 8> operators{'>', '<', '=', '-', '+', '*', '/', '%'};

    template<class Container>
    inline bool is(const Container &container, char c) {
        return std::find(container.begin(), container.end(), c) != container.end();
    }

    static const Map<Str, Operators> operator_types = {
            {"=",  Operators::ASSIGN},
            {"+",  Operators::PLUS},
            {"-",  Operators::MINUS},
            {"*",  Operators::TIMES},
            {"/",  Operators::DIVIDE},
            {"%",  Operators::MODULUS},
            {"==", Operators::EQUAL},
            {"!=", Operators::NOT_EQUAL},
            {">",  Operators::GT},
            {"<",  Operators::LT},
            {">=", Operators::GE},
            {"<=", Operators::LE},
            {"<<", Operators::LSHIFT},
    };

    const static Map<Str, TokenType> tokenType = {
            {"type", TokenType::KEYWORD_TYPE},
            {"val", TokenType::KEYWORD_VAL},
            {"sig", TokenType::KEYWORD_SIG},
            {"fun", TokenType::KEYWORD_FUN},
            {"cons", TokenType::KEYWORD_CONS},

            {"module", TokenType::KEYWORD_MODULE},
            {"export", TokenType::KEYWORD_EXPORT},
            {"use", TokenType::KEYWORD_USE},

            {"if", TokenType::KEYWORD_IF},
            {"then", TokenType::KEYWORD_THEN},
            {"else", TokenType::KEYWORD_ELSE},
            {"loop", TokenType::KEYWORD_LOOP},
            {"collect", TokenType::KEYWORD_COLLECT},
            {"switch", TokenType::KWYWORD_SWITCH},
            {"case", TokenType::KEYWORD_CASE},
            {"otherwise", TokenType::KEYWORD_OTHERWISE},
            {"return", TokenType::KEYWORD_RETURN},
            {"break", TokenType::KEYWORD_BREAK},
            {"continue", TokenType::KEYWORD_CONTINUE},

            {"true", TokenType::KEYWORD_TRUE},
            {"false", TokenType::KEYWORD_FALSE},
            {"unit", TokenType::KEYWORD_UNIT},

            {"(", TokenType::LEFT_PAREN},
            {")", TokenType::RIGHT_PAREN},
            {"[", TokenType::LEFT_SQUARE},
            {"]", TokenType::RIGHT_SQUARE},
            {"{", TokenType::LEFT_CURLY},
            {"}", TokenType::RIGHT_CURLY},

            {"=>", TokenType::DUAL_ARROW},
            {"->", TokenType::SINGLE_ARROW},
            {"::", TokenType::SEPERATOR},
// include
#include "reserved.inc"

    };

    LexState::LexState(const Str &_source) : source(_source), size(_source.size()), index(0), line(1), col(1) {}

    char LexState::current() const {
        if (!eof())
            return source.at(index);
        return '\0';
    }

    bool LexState::eof() const {
        return index >= size;
    }

    char LexState::lookAhead() const {
        if (eof()) {
            return '\0';
        }
        return source.at(index + 1);
    }

    void LexState::next(int n) {
        if (!eof()) {
            index += n;
            col += n;
        }
    }

    static void resetLineAndCol(LexState &state, size_t index) {
        if (index > state.size)
            return;
        state.line = 1;
        state.col = 0;
        for (size_t i = 0; i <= index; i++) {
            state.col++;
            if (state.source[i] == '\n') {
                state.line++;
                state.col = 0;
            }
        }
    }

    void LexState::revert(size_t n) {
        if (n > index)
            return;
        if (index - n > col)
            resetLineAndCol(*this, n);
        else
            col -= (index - n);
        index = n;
    }

    void LexState::nextLine() {
        line++;
        col = 0;
    }

    static void parseSymbol(LexState &state, Vec<Token> &tokens);

    static void parseNumber(LexState &state, Vec<Token> &tokens);

    static void parseOperator(LexState &state, Vec<Token> &tokens);

    static void parseString(LexState &state, Vec<Token> &tokens);

    static bool isTermintator(char c) {
        return c == ',' || c == ';' || c == ')' || c == ']' || c == '}' || c == '.';
    }

    static bool isNumSign(char c) {
        return c == '-' || c == '+';
    }

    static Str withStream(LexState &state, const std::function<void(LexState &state, Stream &stream)> &fn) {
        auto current = state.index;
        try {
            Stream stream{};
            fn(state, stream);
            return stream.str();
        }
        catch (std::exception &ex) {
            state.revert(current);
            return "";
        }
    }

    Vec<Token> Lexer::lex() {
        Vec<Token> tokens{};
        while (char c = state.current()) {
            TokenPosition pos{state.line, state.col};
            if (isblank(c) || isspace(c)) {
                if (c == '\n')
                    state.nextLine();
                state.next();
                continue;
            }
            if (isalpha(c))
                parseSymbol(state, tokens);
            else if (isdigit(c))
                parseNumber(state, tokens);
            else if (c == '"') {
                parseString(state, tokens);
            } else if (is(brackets, c)) {
                Str result{};
                result += c;
                tokens.push_back(Token{tokenType.at(result), result, pos});
                state.next();
            } else if (is(operators, c))
                parseOperator(state, tokens);
            else if (c == ':') {
                if (state.lookAhead() == ':') {
                    tokens.push_back(Token{TokenType::SEPERATOR, "::", pos});
                    state.next(2);
                } else {
                    tokens.push_back(Token{TokenType::COLON, ":", pos});
                    state.next();
                }
            } else if (c == ';') {
                tokens.push_back(Token{TokenType::SEMICOLON, ";", pos});
                state.next();
            } else if (c == ',') {
                tokens.push_back(Token{TokenType::COMMA, ",", pos});
                state.next();
            } else if (c == '.') {
                tokens.push_back(Token{TokenType::DOT, ".", pos});
                state.next();
            }
        }

        return tokens;
    }

    static void parseSymbol(LexState &state, Vec<Token> &tokens) {
        TokenPosition pos{state.line, state.col};
        Str result = withStream(state, [](LexState &state, Stream &stream) {
            while (isalnum(state.current())) {
                stream << state.current();
                state.next();
            }
        });

        if (tokenType.find(result) != tokenType.end()) {
            auto type = tokenType.at(result);
            if (type == TokenType::RESERVED) {
                throw LexException("You are using a reserved token");
            }
            tokens.push_back(Token{tokenType.at(result), result, pos});
        } else
            tokens.push_back(Token{TokenType::ID, result, pos});
    }

    static void parseNumber(LexState &state, Vec<Token> &tokens) {
        TokenPosition pos{state.line, state.col};
        Str result = withStream(state, [](LexState &state, Stream &stream) {
            auto c = state.current();
            while (c && !(isblank(c) || isspace(c) || isTermintator(c) || is(operators, c))) {
                if (isdigit(c))
                    stream << state.current();
                else
                    throw LexException();
                state.next();
                c = state.current();
            }
        });

        if (!result.empty())
            tokens.push_back(Token{TokenType::NUMBER, result, pos});
        else
            throw LexException();
    }

    static Map<char, char> escapeCharValues = {
            {'\'', 0x27},
            {'"',  0x22},
            {'?',  0x3f},
            {'\\', 0x5c},
            {'a',  0x07},
            {'b',  0x08},
            {'f',  0x0c},
            {'n',  0x0a},
            {'r',  0x0d},
            {'t',  0x09},
            {'v',  0x0b},
    };

    static bool isOctalDigit(char c) {
        return c >= '0' && c <= '7';
    }

    static char octalVal(LexState &state) {
        char val = 0;
        while (isOctalDigit(state.current())) {
            int newVal = val * 8 + (state.current() - '0');
            if (newVal >= 256) {
                return val;
            }
            val = static_cast<char>(newVal);
            state.next();
        }
        return val;
    }

    static int hex2dec(char c) {
        switch (c) {
            case '0' ... '9':
                return c - '0';
            case 'a' ... 'f':
                return (c - 'a') + 10;
            case 'A' ... 'F':
                return (c - 'A') + 10;
            default:
                throw LexException("Unknown hex digit");
        }
    }

    static char hexVal(LexState &state) {
        char val = 0;
        while (isxdigit(state.current())) {
            int newVal = val * 16 + hex2dec(state.current());
            if (newVal >= 256) {
                return val;
            }
            val = static_cast<char>(newVal);
            state.next();
        }
        return val;
    }

    static char escapeCharacter(LexState &state) {
        if (state.current() == '\\') {
            state.next();
            if (escapeCharValues.find(state.current()) != escapeCharValues.end()) {
                return escapeCharValues.at(state.current());
            }
            if (isdigit(state.current())) {
                return octalVal(state);
            } else if (state.current() == 'x') {
                state.next();
                return hexVal(state);
            }
        }
        return state.current();
    }

    static void parseString(LexState &state, Vec<Token> &tokens) {
        TokenPosition pos{state.line, state.col};

        Str result = withStream(state, [](LexState &state, Stream &stream) {
            state.next();
            while (state.current() != '"') {
                if (state.current() == '\\') {
                    stream << escapeCharacter(state);
                } else {
                    stream << state.current();
                    state.next();
                }
            }
        });
        state.next();

        tokens.push_back(Token{TokenType::STRING, result, pos});
    }

    static void parseOperator(LexState &state, Vec<Token> &tokens) {
        TokenPosition pos{state.line, state.col};
        Str result = withStream(state, [](LexState &state, Stream &stream) {
            auto c = state.current();
            while (is(operators, c)) {
                stream << c;
                state.next();
                c = state.current();
            }
        });

        if (tokenType.find(result) != tokenType.end()) {
            tokens.push_back(Token{tokenType.at(result), result, pos});
            return;
        }

        Operators operatorType = Operators::UNKNOWN;
        if (operator_types.find(result) != operator_types.end())
            operatorType = operator_types.at(result);

        tokens.push_back(Token{TokenType::OPERATOR, result, pos, operatorType});
    }

} // namespace NG
