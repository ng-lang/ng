
#include <intp/runtime.hpp>

namespace NG::runtime {
    NGObject *OperatorsBase::opIndex(NGObject *index) const {
        return nullptr;
    }

    NGObject *OperatorsBase::opIndex(NGObject *index, NGObject *newValue) {
        return nullptr;
    }

    bool OperatorsBase::opGreaterThan(NGObject *other) const {
        return false;
    }

    bool OperatorsBase::opGreaterEqual(NGObject *other) const {
        return false;
    }

    bool OperatorsBase::opLessThan(NGObject *other) const { return false; }

    bool OperatorsBase::opLessEqual(NGObject *other) const { return false; }

    bool OperatorsBase::opEquals(NGObject *other) const { return false; }

    bool OperatorsBase::opNotEqual(NGObject *other) const { return false; }

    NGObject *OperatorsBase::opPlus(NGObject *other) const { return nullptr; }

    NGObject *OperatorsBase::opMinus(NGObject *other) const { return nullptr; }

    NGObject *OperatorsBase::opTimes(NGObject *other) const { return nullptr; }

    NGObject *OperatorsBase::opModulus(NGObject *other) const { return nullptr; }

    NGObject *OperatorsBase::opDividedBy(NGObject *other) const { return nullptr; }


    NGObject *OperatorsBase::respond(const Str &member, NGContext *context,
                                            NGInvocationContext *invocationContext) {
        return nullptr;
    }

    NGObject *OperatorsBase::opLShift(NGObject *object) {
        return nullptr;
    }

    NGObject *OperatorsBase::opRShift(NGObject *object) {
        return nullptr;
    }

    OperatorsBase::~OperatorsBase() noexcept = default;
}