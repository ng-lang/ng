
#include <parser.hpp>
#include <token.hpp>

namespace NG::parsing {

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

}