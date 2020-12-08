
#include <intp/runtime.hpp>

namespace NG::runtime {
    NGObject *IOverloadedOperators::opIndex(NGObject *index) const {
        return nullptr;
    }

    NGObject *IOverloadedOperators::opIndex(NGObject *index, NGObject *newValue) {
        return nullptr;
    }

    bool IOverloadedOperators::opGreaterThan(NGObject *other) const {
        return false;
    }

    bool IOverloadedOperators::opGreaterEqual(NGObject *other) const {
        return false;
    }

    bool IOverloadedOperators::opLessThan(NGObject *other) const { return false; }

    bool IOverloadedOperators::opLessEqual(NGObject *other) const { return false; }

    bool IOverloadedOperators::opEquals(NGObject *other) const { return false; }

    bool IOverloadedOperators::opNotEqual(NGObject *other) const { return false; }

    NGObject *IOverloadedOperators::opPlus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opMinus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opTimes(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opModulus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opDividedBy(NGObject *other) const { return nullptr; }


    NGObject *IOverloadedOperators::respond(const Str &member, NGContext *context,
                                            NGInvocationContext *invocationContext) {
        return nullptr;
    }

    NGObject *IOverloadedOperators::opLShift(NGObject *object) {
        return nullptr;
    }

    NGObject *IOverloadedOperators::opRShift(NGObject *object) {
        return nullptr;
    }

    IOverloadedOperators::~IOverloadedOperators() noexcept = default;
}