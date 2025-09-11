
#include <parser.hpp>
#include <token.hpp>

namespace NG::parsing
{
    using namespace NG;

    ParseState::ParseState(const Vec<Token> &tokens) : tokens(tokens), size(tokens.size()), index(0)
    {
    }

    auto ParseState::current() const -> const Token &
    {
        if (!eof())
        {
            return tokens.at(index);
        }
        throw EOFException();
    }

    auto ParseState::eof() const -> bool
    {
        return index >= size;
    }

    void ParseState::next(int n)
    {
        if (!eof())
        {
            index += n;
        }
    }

    void ParseState::revert(size_t n)
    {
        if (n > index)
        {
            return;
        }
        index = n;
    }
}