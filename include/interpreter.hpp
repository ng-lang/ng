
#ifndef __NG_INTERPRETER_HPP
#define __NG_INTERPRETER_HPP

#include <fwd.hpp>

namespace NG::interpreter {

    struct ISummarizable {
        virtual void summary() = 0;

        virtual ~ISummarizable() = 0;
    };

    NG::AST::IASTVisitor *interpreter();
}

#endif // __NG_INTERPRETER_HPP
