
#include <parser.hpp>
#include <token.hpp>

namespace NG::parsing
{

    LexState::LexState(const Str &_source) : source(_source), size(_source.size()), index(0), line(1), col(1) {}

    auto LexState::current() const -> char
    {
        if (!eof())
        {
            return source.at(index);
        }
        return '\0';
    }

    auto LexState::eof() const -> bool
    {
        return index >= size;
    }

    auto LexState::lookAhead() const -> char
    {
        if (eof())
        {
            return '\0';
        }
        return source.at(index + 1);
    }

    void LexState::next(int n)
    {
        if (!eof())
        {
            index += n;
            col += n;
        }
    }

    static void resetLineAndCol(LexState &state, size_t index)
    {
        if (index > state.size)
        {
            return;
        }
        state.line = 1;
        state.col = 0;
        for (size_t i = 0; i <= index; i++)
        {
            state.col++;
            if (state.source[i] == '\n')
            {
                state.line++;
                state.col = 0;
            }
        }
    }

    void LexState::revert(size_t n)
    {
        if (n > index)
        {
            return;
        }
        if (index - n > col)
        {
            resetLineAndCol(*this, n);
        }
        else
        {
            col -= (index - n);
        }
        index = n;
    }

    void LexState::nextLine()
    {
        line++;
        col = 0;
    }

}