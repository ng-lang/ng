
#ifndef __NG_INTP_INTERPRETER_HPP
#define __NG_INTP_INTERPRETER_HPP

#include <fwd.hpp>
#include <visitor.hpp>

namespace NG::intp {

    struct ISummarizable {
        virtual void summary() = 0;

        virtual ~ISummarizable() = 0;
    };

    struct IInterperter: public virtual ISummarizable, public virtual NG::ast::IASTVisitor {
        virtual NG::runtime::NGContext* intpContext() = 0;
    };

    IInterperter *interpreter();
}

#endif // __NG_INTERPRETER_HPP
