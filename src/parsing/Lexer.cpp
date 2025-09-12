
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

    constexpr std::array<char, 10> operators{'>', '<', '=', '-', '+', '*', '/', '%', '!', '?'};

    constexpr std::array<int, 6> bitlengths{8, 16, 32, 64, 128};

    template <class Container, class T>
    inline auto is(const Container &container, T item) -> bool
    {
        return std::find(container.begin(), container.end(), item) != container.end();
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
        {">>", Operators::RSHIFT},
        {"!", Operators::NOT},
        {"?", Operators::QUERY},
        {"???", Operators::UNDEFINED},
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
        {"native", TokenType::KEYWORD_NATIVE},

        {"if", TokenType::KEYWORD_IF},
        {"then", TokenType::KEYWORD_THEN},
        {"else", TokenType::KEYWORD_ELSE},
        {"loop", TokenType::KEYWORD_LOOP},
        {"collect", TokenType::KEYWORD_COLLECT},
        {"next", TokenType::KEYWORD_NEXT},
        {"switch", TokenType::KWYWORD_SWITCH},
        {"case", TokenType::KEYWORD_CASE},
        {"otherwise", TokenType::KEYWORD_OTHERWISE},
        {"return", TokenType::KEYWORD_RETURN},
        {"break", TokenType::KEYWORD_BREAK},
        {"continue", TokenType::KEYWORD_CONTINUE},
        {"in", TokenType::KEYWORD_IN},
        {"is", TokenType::KEYWORD_IS},

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

    static Token lexSymbol(LexState &state, Vec<Token> &tokens);

    static Token lexNumber(LexState &state, Vec<Token> &tokens);

    static Token lexOperator(LexState &state, Vec<Token> &tokens);

    static Token lexString(LexState &state, Vec<Token> &tokens);

    [[nodiscard]] inline auto isTermintator(char character) -> bool
    {
        return character == ',' || character == ';' || character == ')' || character == ']' || character == '}';
    }

    [[nodiscard]] inline auto withStream(LexState &state, const std::function<void(LexState &state, Stream &stream)> &func) -> Str
    {
        auto current = state.index;
        try
        {
            Stream stream{};
            func(state, stream);
            return stream.str();
        }
        catch (std::exception &)
        {
            state.revert(current);
            return "";
        }
    }

    [[nodiscard]] inline auto isBlankSpaceTerminatorOrOperator(char current) -> bool
    {
        return isblank(current) || isspace(current) || isTermintator(current) || is(operators, current);
    }

    auto Lexer::next() -> Token
    {
        while (const char current = state.current())
        {
            TokenPosition pos{.line = state.line, .col = state.col};
            if (isblank(current) || isspace(current))
            {
                if (current == '\n')
                {
                    state.nextLine();
                }
                state.next();
                continue;
            }
            if (isalpha(current) || current == '_')
            {
                return lexSymbol(state, tokens);
            }
            else if (isdigit(current))
            {
                return lexNumber(state, tokens);
            }
            else if (current == '"')
            {
                return lexString(state, tokens);
            }
            else if (is(brackets, current))
            {
                Str result{};
                result += current;
                Token token{.type = tokenType.at(result), .repr = result, .position = pos};
                tokens.push_back(token);
                state.next();
                return token;
            }
            else if (current == '/')
            {
                if (state.lookAhead() == '/')
                {
                    while (state.current() != '\n')
                    {
                        state.next();
                    }
                    state.next();
                }
                else if (state.lookAhead() == '*')
                {
                    while (!(state.current() == '*' && state.lookAhead() == '/'))
                    {
                        state.next();
                    }
                    state.next(2);
                }
                else
                {
                    return lexOperator(state, tokens);
                }
            }
            else if (current == '#')
            {
                while (state.current() != '\n')
                {
                    state.next();
                }
                state.next();
            }
            else if (current == '-')
            {
                return lexOperator(state, tokens);
            }
            else if (is(operators, current))
            {
                return lexOperator(state, tokens);
            }
            else if (current == ':')
            {
                if (state.lookAhead() == ':')
                {
                    Token token{.type = TokenType::SEPERATOR, .repr = "::", .position = pos};
                    tokens.push_back(token);
                    state.next(2);
                    return token;
                }
                else
                {
                    Token token{.type = TokenType::COLON, .repr = ":", .position = pos};
                    tokens.push_back(token);
                    state.next();
                    return token;
                }
            }
            else if (current == ';')
            {
                Token token{.type = TokenType::SEMICOLON, .repr = ";", .position = pos};
                tokens.push_back(token);
                state.next();
                return token;
            }
            else if (current == ',')
            {
                Token token{.type = TokenType::COMMA, .repr = ",", .position = pos};
                tokens.push_back(token);
                state.next();
                return token;
            }
            else if (current == '.')
            {
                Token token{.type = TokenType::DOT, .repr = ".", .position = pos};
                tokens.push_back(token);
                state.next();
                return token;
            }
            else
            {
                throw LexException("Unknown token: " + std::string(1, current));
            }
        }
        return {};
    }

    auto Lexer::lex() -> Vec<Token> // NOLINT(readability-function-cognitive-complexity)
    {
        while (const char current = state.current())
        {
            next();
        }

        return tokens;
    }

    static Token lexSymbol(LexState &state, Vec<Token> &tokens)
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
                throw LexException("You are using a reserved token: " + result);
            }
            Token token{.type = tokenType.at(result), .repr = result, .position = pos};
            tokens.push_back(token);
            return token;
        }
        else
        {
            Token token{.type = TokenType::ID, .repr = result, .position = pos};
            tokens.push_back(token);
            return token;
        }
    }

    enum class Words : uint16_t
    {
        BYTE = 8,
        WORD = 16,
        DUALWORD = 32,
        QUADWORD = 64,
        OCTOWORD = 128,
        HEXOWORD = 256,
    };

    enum class Floats : uint16_t
    {
        HALF = 16,
        SINGLE = 32,
        DOUBLE = 64,
        QUADRUPLE = 128,
        OCTUPLE = 256,
    };

    static auto resolveIntegralType(char sign, Words words) -> TokenType
    {
        TokenType result = TokenType::NUMBER_I32;
        switch (words)
        {
        case Words::BYTE:
            result = TokenType::NUMBER_I8;
            break;
        case Words::WORD:
            result = TokenType::NUMBER_I16;
            break;
        case Words::DUALWORD:
            result = TokenType::NUMBER_I32;
            break;
        case Words::QUADWORD:
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

    static auto resolveFloatingPointType(Floats floats) -> TokenType
    {
        TokenType result = TokenType::NUMBER_F32;
        switch (floats)
        {
        case Floats::HALF:
            result = TokenType::NUMBER_F16;
            break;
        case Floats::SINGLE:
            result = TokenType::NUMBER_F32;
            break;
        case Floats::DOUBLE:
            result = TokenType::NUMBER_F64;
            break;
        case Floats::QUADRUPLE:
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
            auto current = state.current();
            while(isdigit(current)) {
                stream << current;
                state.next();
                current = state.current();
            } });

        int result = std::stoi(postfix);
        if (!is(bitlengths, result))
        {
            throw LexException("Invalid bit length");
        }
        return result;
    }

    static Token lexNumber(LexState &state, Vec<Token> &tokens)
    {
        TokenPosition pos{.line = state.line, .col = state.col};
        TokenType tokenType = TokenType::NUMBER;
        Str result = withStream(state, [&tokenType](LexState &state, Stream &stream)
                                {
            auto current = state.current();
            bool decimalPointSet = false;
            bool exponentalSet = false;
            while (current && !(isBlankSpaceTerminatorOrOperator(current))) {
                if (isdigit(current)) {
                    stream << state.current();
                } else if(current == '_') {
                    // skip just as seperator.
                } else if (current == '.' && !decimalPointSet && !exponentalSet) {
                    if (!isdigit(state.lookAhead())) {
                        return;
                    }
                    decimalPointSet = true;
                    tokenType = TokenType::FLOATING_POINT;
                    stream << current;
                } else if (current == 'e' && !exponentalSet) {
                    exponentalSet = true;
                    tokenType = TokenType::FLOATING_POINT;
                    stream << current;
                    if (state.lookAhead() == '-') {
                        state.next();
                        stream << state.current();
                    }
                } else if ((tolower(current) == 'u' || tolower(current) == 'i') && tokenType != TokenType::FLOATING_POINT) {
                    state.next();
                    int bitlength = numberTypePostfix(state);
                    tokenType = resolveIntegralType(current, from_code<Words>(bitlength));
                    return;
                } else if ((tolower(current) == 'f')) {
                    state.next();
                    int bitlength = numberTypePostfix(state);
                    tokenType = resolveFloatingPointType(from_code<Floats>(bitlength));
                    return;
                } else {
                    throw LexException();
                }
                state.next();
                current = state.current();
            } });

        if (!result.empty())
        {
            Token token{.type = tokenType, .repr = result, .position = pos};
            tokens.push_back(token);
            return token;
        }
        else
        {
            throw LexException();
        }
    }

    // NOLINTBEGIN(*-magic-numbers)
    static const Map<char, char> escapeCharValues = {
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
    // NOLINTEND(*-magic-numbers)

    static auto isOctalDigit(char character) -> bool
    {
        return character >= '0' && character <= '7';
    }

    static auto octalVal(LexState &state) -> char
    {
        char val = 0;
        constexpr int octaBase = 8;
        constexpr int charLimit = 256;
        while (isOctalDigit(state.current()))
        {
            int newVal = (val * octaBase) + (state.current() - '0');
            if (newVal >= charLimit)
            {
                return val;
            }
            val = static_cast<char>(newVal);
            state.next();
        }
        return val;
    }

    static auto hex2dec(char character) -> int
    {
        constexpr int decimalBase = 10;
        switch (character)
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
            return character - '0';
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            return (character - 'a') + decimalBase;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            return (character - 'A') + decimalBase;
        default:
            throw LexException("Unknown hex digit");
        }
    }

    static auto hexVal(LexState &state) -> char
    {
        char val = 0;
        constexpr int hexoBase = 16;
        constexpr int charLimit = 256;
        while (isxdigit(state.current()) != 0)
        {
            int newVal = (val * hexoBase) + hex2dec(state.current());
            if (newVal >= charLimit)
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

    static Token lexString(LexState &state, Vec<Token> &tokens)
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
        Token token{.type = TokenType::STRING, .repr = result, .position = pos};
        tokens.push_back(token);
        return token;
    }

    static Token lexOperator(LexState &state, Vec<Token> &tokens)
    {
        TokenPosition pos{.line = state.line, .col = state.col};
        Str result = withStream(state, [](LexState &state, Stream &stream)
                                {
            auto current = state.current();
            while (is(operators, current)) {
                stream << current;
                state.next();
                current = state.current();
            } });

        if (tokenType.contains(result))
        {
            Token token{.type = tokenType.at(result), .repr = result, .position = pos};
            tokens.push_back(token);
            return token;
        }

        Operators operatorType = Operators::UNKNOWN;
        if (operator_types.contains(result))
        {
            operatorType = operator_types.at(result);
        }
        Token token{.type = TokenType::OPERATOR, .repr = result, .position = pos, .operatorType = operatorType};
        tokens.push_back(token);
        return token;
    }
}