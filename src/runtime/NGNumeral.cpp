
#include <intp/runtime_numerals.hpp>

namespace NG::runtime
{
    size_t NumeralBase::bytesize() const
    {
        return 0;
    }

    bool NumeralBase::signedness() const
    {
        return false;
    }

    bool NumeralBase::floating_point() const
    {
        return false;
    }

    RuntimeRef<NGObject> NumeralBase::opPlus(const NumeralBase *) const
    {
        return nullptr;
    }
    RuntimeRef<NGObject> NumeralBase::opMinus(const NumeralBase *) const
    {
        return nullptr;
    }
    RuntimeRef<NGObject> NumeralBase::opTimes(const NumeralBase *) const
    {
        return nullptr;
    }
    RuntimeRef<NGObject> NumeralBase::opDividedBy(const NumeralBase *) const
    {
        return nullptr;
    }
    RuntimeRef<NGObject> NumeralBase::opModulus(const NumeralBase *) const
    {
        return nullptr;
    }
}