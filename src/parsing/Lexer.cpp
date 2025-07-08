
#include <parser.hpp>
#include <token.hpp>
#include <common.hpp>
#include <debug.hpp>

#include <sstream>
#include <unordered_map>
#include <functional>
#include <array>
#include <cctype>

namespace NG::parsing {

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
            {"property", TokenType::KEYWORD_PROPERTY},

            {"module", TokenType::KEYWORD_MODULE},
            {"export", TokenType::KEYWORD_EXPORT},
            {"exports", TokenType::KEYWORD_EXPORTS},
            {"import", TokenType::KEYWORD_IMPORT},
            {"new", TokenType::KEYWORD_NEW},

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
            {"int", TokenType::KEYWORD_INT},
            {"bool", TokenType::KEYWORD_BOOL},
            {"string", TokenType::KEYWORD_STRING},
            {"float", TokenType::KEYWORD_FLOAT},
    
            // integer variants
            {"byte", TokenType::KEYWORD_BYTE},
            {"ubyte", TokenType::KEYWORD_UBYTE},
            {"short", TokenType::KEYWORD_SHORT},
            {"ushort", TokenType::KEYWORD_USHORT},
            {"uint", TokenType::KEYWORD_UINT},
            {"long", TokenType::KEYWORD_LONG},
            {"ulong", TokenType::KEYWORD_ULONG},
            {"u8", TokenType::KEYWORD_U8},
            {"i8", TokenType::KEYWORD_I8},
            {"u16", TokenType::KEYWORD_U16},
            {"i16", TokenType::KEYWORD_I16},
            {"u32", TokenType::KEYWORD_U32},
            {"i32", TokenType::KEYWORD_I32},
            {"u64", TokenType::KEYWORD_U64},
            {"i64", TokenType::KEYWORD_I64},
            {"uptr", TokenType::KEYWORD_UPTR},
            {"iptr", TokenType::KEYWORD_IPTR},
    
            // floating point variants
            {"half", TokenType::KEYWORD_HALF},
            {"double", TokenType::KEYWORD_DOUBLE},
            {"quadruple", TokenType::KEYWORD_QUADRUPLE},
            {"f16", TokenType::KEYWORD_F16},
            {"f32", TokenType::KEYWORD_F32},
            {"f64", TokenType::KEYWORD_F64},
            {"f128", TokenType::KEYWORD_F128},

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

    static void lexSymbol(LexState &state, Vec<Token> &tokens);

    static void lexNumber(LexState &state, Vec<Token> &tokens);

    static void lexOperator(LexState &state, Vec<Token> &tokens);

    static void lexString(LexState &state, Vec<Token> &tokens);

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
        catch (std::exception &) {
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
            if (isalpha(c) || c == '_')
                lexSymbol(state, tokens);
            else if (isdigit(c))
                lexNumber(state, tokens);
            else if (c == '"') {
                lexString(state, tokens);
            } else if (is(brackets, c)) {
                Str result{};
                result += c;
                tokens.push_back(Token{tokenType.at(result), result, pos});
                state.next();
            } else if (c == '/') {
                if (state.lookAhead() == '/') {
                    while (state.current() != '\n') {
                        state.next();
                    }
                    state.next();
                } else {
                    lexOperator(state, tokens);
                }
            } else if (is(operators, c))
                lexOperator(state, tokens);
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
        debug_log("tokens", tokens.size());

        return tokens;
    }

    static void lexSymbol(LexState &state, Vec<Token> &tokens) {
        TokenPosition pos{state.line, state.col};
        Str result = withStream(state, [](LexState &state, Stream &stream) {
            while (isalnum(state.current()) || state.current() == '_') {
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

    static void lexNumber(LexState &state, Vec<Token> &tokens) {
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
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return c - '0';
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                return (c - 'a') + 10;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
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

    static void lexString(LexState &state, Vec<Token> &tokens) {
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

    static void lexOperator(LexState &state, Vec<Token> &tokens) {
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
}