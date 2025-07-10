
#include <intp/runtime.hpp>

namespace NG::runtime
{
    auto OperatorsBase::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
    {
        return nullptr;
    }

    auto OperatorsBase::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
    {
        return nullptr;
    }

    auto OperatorsBase::opGreaterThan(RuntimeRef<NGObject> other) const -> bool
    {
        return false;
    }

    auto OperatorsBase::opGreaterEqual(RuntimeRef<NGObject> other) const -> bool
    {
        return false;
    }

    auto OperatorsBase::opLessThan(RuntimeRef<NGObject> other) const -> bool { return false; }

    auto OperatorsBase::opLessEqual(RuntimeRef<NGObject> other) const -> bool { return false; }

    auto OperatorsBase::opEquals(RuntimeRef<NGObject> other) const -> bool { return false; }

    auto OperatorsBase::opNotEqual(RuntimeRef<NGObject> other) const -> bool { return false; }

    auto OperatorsBase::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> { return nullptr; }

    auto OperatorsBase::opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> { return nullptr; }

    auto OperatorsBase::opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> { return nullptr; }

    auto OperatorsBase::opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> { return nullptr; }

    auto OperatorsBase::opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> { return nullptr; }

    auto OperatorsBase::respond(const Str &member, RuntimeRef<NGContext> context,
                                                RuntimeRef<NGInvocationContext> invocationContext) -> RuntimeRef<NGObject>
    {
        return nullptr;
    }

    auto OperatorsBase::opLShift(RuntimeRef<NGObject> object) -> RuntimeRef<NGObject>
    {
        return nullptr;
    }

    auto OperatorsBase::opRShift(RuntimeRef<NGObject> object) -> RuntimeRef<NGObject>
    {
        return nullptr;
    }

    OperatorsBase::~OperatorsBase() noexcept = default;
}