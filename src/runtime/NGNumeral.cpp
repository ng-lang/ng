
#include <intp/runtime_numerals.hpp>

namespace NG::runtime
{
    auto NumeralBase::bytesize() const -> size_t
    {
        return 0;
    }

    auto NumeralBase::signedness() const -> bool
    {
        return false;
    }

    auto NumeralBase::floating_point() const -> bool
    {
        return false;
    }

    auto NumeralBase::opPlus(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
    {
        return nullptr;
    }
    auto NumeralBase::opMinus(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
    {
        return nullptr;
    }
    auto NumeralBase::opTimes(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
    {
        return nullptr;
    }
    auto NumeralBase::opDividedBy(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
    {
        return nullptr;
    }
    auto NumeralBase::opModulus(const NumeralBase * /*unused*/) const -> RuntimeRef<NGObject>
    {
        return nullptr;
    }
}