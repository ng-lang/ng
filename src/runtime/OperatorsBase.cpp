
#include <intp/runtime.hpp>

namespace NG::runtime {
    RuntimeRef<NGObject> OperatorsBase::opIndex(RuntimeRef<NGObject> index) const {
        return nullptr;
    }

    RuntimeRef<NGObject> OperatorsBase::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) {
        return nullptr;
    }

    bool OperatorsBase::opGreaterThan(RuntimeRef<NGObject> other) const {
        return false;
    }

    bool OperatorsBase::opGreaterEqual(RuntimeRef<NGObject> other) const {
        return false;
    }

    bool OperatorsBase::opLessThan(RuntimeRef<NGObject> other) const { return false; }

    bool OperatorsBase::opLessEqual(RuntimeRef<NGObject> other) const { return false; }

    bool OperatorsBase::opEquals(RuntimeRef<NGObject> other) const { return false; }

    bool OperatorsBase::opNotEqual(RuntimeRef<NGObject> other) const { return false; }

    RuntimeRef<NGObject> OperatorsBase::opPlus(RuntimeRef<NGObject> other) const { return nullptr; }

    RuntimeRef<NGObject> OperatorsBase::opMinus(RuntimeRef<NGObject> other) const { return nullptr; }

    RuntimeRef<NGObject> OperatorsBase::opTimes(RuntimeRef<NGObject> other) const { return nullptr; }

    RuntimeRef<NGObject> OperatorsBase::opModulus(RuntimeRef<NGObject> other) const { return nullptr; }

    RuntimeRef<NGObject> OperatorsBase::opDividedBy(RuntimeRef<NGObject> other) const { return nullptr; }


    RuntimeRef<NGObject> OperatorsBase::respond(const Str &member, RuntimeRef<NGContext> context,
                                            RuntimeRef<NGInvocationContext> invocationContext) {
        return nullptr;
    }

    RuntimeRef<NGObject> OperatorsBase::opLShift(RuntimeRef<NGObject> object) {
        return nullptr;
    }

    RuntimeRef<NGObject> OperatorsBase::opRShift(RuntimeRef<NGObject> object) {
        return nullptr;
    }

    OperatorsBase::~OperatorsBase() noexcept = default;
}