
#include <parser.hpp>
#include <token.hpp>
#include <common.hpp>
#include <debug.hpp>

#include <sstream>
#include <unordered_map>
#include <functional>
#include <array>
#include <cctype>

namespace NG::parsing
{

    template <class K, class V>
    using Map = std::unordered_map<K, V>;

    using Stream = std::stringstream;

    constexpr std::array<char, 6> brackets{'(', ')', '{', '}', '[', ']'};

    constexpr std::array<char, 8> operators{'>', '<', '=', '-', '+', '*', '/', '%'};

    constexpr std::array<int, 6> bitlengths{8, 16, 32, 64, 128};

    template <class Container, class T>
    inline auto is(const Container &container, T c) -> bool
    {
        return std::find(container.begin(), container.end(), c) != container.end();
    }

    static const Map<Str, Operators> operator_types = {
        {"=", Operators::ASSIGN},
        {"+", Operators::PLUS},
        {"-", Operators::MINUS},
        {"*", Operators::TIMES},
        {"/", Operators::DIVIDE},
        {"%", Operators::MODULUS},
        {"==", Operators::EQUAL},
        {"!=", Operators::NOT_EQUAL},
        {">", Operators::GT},
        {"<", Operators::LT},
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

    static auto isTermintator(char c) -> bool
    {
        return c == ',' || c == ';' || c == ')' || c == ']' || c == '}';
    }

    static auto isNumSign(char c) -> bool
    {
        return c == '-' || c == '+';
    }

    static auto withStream(LexState &state, const std::function<void(LexState &state, Stream &stream)> &fn) -> Str
    {
        auto current = state.index;
        try
        {
            Stream stream{};
            fn(state, stream);
            return stream.str();
        }
        catch (std::exception &)
        {
            state.revert(current);
            return "";
        }
    }

    auto Lexer::lex() -> Vec<Token>
    {
        Vec<Token> tokens{};
        while (char c = state.current())
        {
            TokenPosition pos{.line = state.line, .col = state.col};
            if ((isblank(c) != 0) || (isspace(c) != 0))
            {
                if (c == '\n')
                {
                    state.nextLine();
                }
                state.next();
                continue;
            }
            if ((isalpha(c) != 0) || c == '_')
            {
                lexSymbol(state, tokens);
            }
            else if (isdigit(c) != 0)
            {
                lexNumber(state, tokens);
            }
            else if (c == '"')
            {
                lexString(state, tokens);
            }
            else if (is(brackets, c))
            {
                Str result{};
                result += c;
                tokens.push_back(Token{.type = tokenType.at(result), .repr = result, .position = pos});
                state.next();
            }
            else if (c == '/')
            {
                if (state.lookAhead() == '/')
                {
                    while (state.current() != '\n')
                    {
                        state.next();
                    }
                    state.next();
                }
                else
                {
                    lexOperator(state, tokens);
                }
            }
            else if (is(operators, c))
            {
                lexOperator(state, tokens);
            }
            else if (c == ':')
            {
                if (state.lookAhead() == ':')
                {
                    tokens.push_back(Token{.type = TokenType::SEPERATOR, .repr = "::", .position = pos});
                    state.next(2);
                }
                else
                {
                    tokens.push_back(Token{.type = TokenType::COLON, .repr = ":", .position = pos});
                    state.next();
                }
            }
            else if (c == ';')
            {
                tokens.push_back(Token{.type = TokenType::SEMICOLON, .repr = ";", .position = pos});
                state.next();
            }
            else if (c == ',')
            {
                tokens.push_back(Token{.type = TokenType::COMMA, .repr = ",", .position = pos});
                state.next();
            }
            else if (c == '.')
            {
                tokens.push_back(Token{.type = TokenType::DOT, .repr = ".", .position = pos});
                state.next();
            }
        }

        return tokens;
    }

    static void lexSymbol(LexState &state, Vec<Token> &tokens)
    {
        TokenPosition pos{.line = state.line, .col = state.col};
        Str result = withStream(state, [](LexState &state, Stream &stream)
                                {
            while (isalnum(state.current()) || state.current() == '_') {
                stream << state.current();
                state.next();
            } });

        if (tokenType.contains(result))
        {
            auto type = tokenType.at(result);
            if (type == TokenType::RESERVED)
            {
                throw LexException("You are using a reserved token");
            }
            tokens.push_back(Token{.type = tokenType.at(result), .repr = result, .position = pos});
        }
        else
        {
            tokens.push_back(Token{.type = TokenType::ID, .repr = result, .position = pos});
        }
    }

    static auto resolveIntegralType(char sign, int bits) -> TokenType
    {
        TokenType result = TokenType::NUMBER_I32;
        switch (bits)
        {
        case 8:
            result = TokenType::NUMBER_I8;
            break;
        case 16:
            result = TokenType::NUMBER_I16;
            break;
        case 32:
            result = TokenType::NUMBER_I32;
            break;
        case 64:
            result = TokenType::NUMBER_I64;
            break;
        default:
            throw LexException("Invalid bits");
        };
        if (tolower(sign) == 'u')
        {
            return from_code<TokenType>(code(result) - 1);
        }
        if (tolower(sign) == 'i')
        {
            return result;
        }
        throw LexException("Invalid sign, should be u or i");
    }

    static auto resolveFloatingPointType(int bits) -> TokenType
    {
        TokenType result = TokenType::NUMBER_F32;
        switch (bits)
        {
        case 16:
            result = TokenType::NUMBER_F16;
            break;
        case 32:
            result = TokenType::NUMBER_F32;
            break;
        case 64:
            result = TokenType::NUMBER_F64;
            break;
        case 128:
            result = TokenType::NUMBER_F128;
            break;
        default:
            throw LexException("Invalid bits");
        };
        return result;
    }

    static auto numberTypePostfix(LexState &state) -> int
    {
        Str postfix = withStream(state, [](LexState &state, Stream &stream)
                                 {
            auto c = state.current();
            while(isdigit(c)) {
                stream << c;
                state.next();
                c = state.current();
            } });

        int result = std::stoi(postfix);
        if (!is(bitlengths, result))
        {
            throw LexException("Invalid bit length");
        }
        return result;
    }

    static void lexNumber(LexState &state, Vec<Token> &tokens)
    {
        TokenPosition pos{.line = state.line, .col = state.col};
        TokenType tokenType = TokenType::NUMBER;
        Str result = withStream(state, [&tokenType](LexState &state, Stream &stream)
                                {
            auto c = state.current();
            bool decimalPointSet = false;
            bool exponentalSet = false;
            while (c && !(isblank(c) || isspace(c) || isTermintator(c) || is(operators, c))) {
                if (isdigit(c)) {
                    stream << state.current();
                } else if(c == '_') {
                    // skip just as seperator.
                } else if (c == '.' && !decimalPointSet && !exponentalSet) {
                    if (!isdigit(state.lookAhead())) {
                        return;
                    }
                    decimalPointSet = true;
                    tokenType = TokenType::FLOATING_POINT;
                    stream << c;
                } else if (c == 'e' && !exponentalSet) {
                    exponentalSet = true;
                    tokenType = TokenType::FLOATING_POINT;
                    stream << c;
                } else if ((tolower(c) == 'u' || tolower(c) == 'i') && tokenType != TokenType::FLOATING_POINT) {
                    state.next();
                    int bitlength = numberTypePostfix(state);
                    tokenType = resolveIntegralType(c, bitlength);
                    return;
                } else if ((tolower(c) == 'f')) {
                    state.next();
                    int bitlength = numberTypePostfix(state);
                    tokenType = resolveFloatingPointType(bitlength);
                    return;
                } else {
                    throw LexException();
                }
                state.next();
                c = state.current();
            } });

        if (!result.empty())
        {
            tokens.push_back(Token{.type = tokenType, .repr = result, .position = pos});
        }
        else
        {
            throw LexException();
        }
    }

    static Map<char, char> escapeCharValues = {
        {'\'', 0x27},
        {'"', 0x22},
        {'?', 0x3f},
        {'\\', 0x5c},
        {'a', 0x07},
        {'b', 0x08},
        {'f', 0x0c},
        {'n', 0x0a},
        {'r', 0x0d},
        {'t', 0x09},
        {'v', 0x0b},
    };

    static auto isOctalDigit(char c) -> bool
    {
        return c >= '0' && c <= '7';
    }

    static auto octalVal(LexState &state) -> char
    {
        char val = 0;
        while (isOctalDigit(state.current()))
        {
            int newVal = (val * 8) + (state.current() - '0');
            if (newVal >= 256)
            {
                return val;
            }
            val = static_cast<char>(newVal);
            state.next();
        }
        return val;
    }

    static auto hex2dec(char c) -> int
    {
        switch (c)
        {
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

    static auto hexVal(LexState &state) -> char
    {
        char val = 0;
        while (isxdigit(state.current()) != 0)
        {
            int newVal = (val * 16) + hex2dec(state.current());
            if (newVal >= 256)
            {
                return val;
            }
            val = static_cast<char>(newVal);
            state.next();
        }
        return val;
    }

    static auto escapeCharacter(LexState &state) -> char
    {
        if (state.current() == '\\')
        {
            state.next();
            if (escapeCharValues.contains(state.current()))
            {
                return escapeCharValues.at(state.current());
            }
            if (isdigit(state.current()) != 0)
            {
                return octalVal(state);
            }
            if (state.current() == 'x')
            {
                state.next();
                return hexVal(state);
            }
        }
        return state.current();
    }

    static void lexString(LexState &state, Vec<Token> &tokens)
    {
        TokenPosition pos{.line = state.line, .col = state.col};

        Str result = withStream(state, [](LexState &state, Stream &stream)
                                {
            state.next();
            while (state.current() != '"') {
                if (state.current() == '\\') {
                    stream << escapeCharacter(state);
                } else {
                    stream << state.current();
                    state.next();
                }
            } });
        state.next();

        tokens.push_back(Token{.type = TokenType::STRING, .repr = result, .position = pos});
    }

    static void lexOperator(LexState &state, Vec<Token> &tokens)
    {
        TokenPosition pos{.line = state.line, .col = state.col};
        Str result = withStream(state, [](LexState &state, Stream &stream)
                                {
            auto c = state.current();
            while (is(operators, c)) {
                stream << c;
                state.next();
                c = state.current();
            } });

        if (tokenType.contains(result))
        {
            tokens.push_back(Token{.type = tokenType.at(result), .repr = result, .position = pos});
            return;
        }

        Operators operatorType = Operators::UNKNOWN;
        if (operator_types.contains(result))
        {
            operatorType = operator_types.at(result);
        }

        tokens.push_back(Token{.type = TokenType::OPERATOR, .repr = result, .position = pos, .operatorType = operatorType});
    }
}