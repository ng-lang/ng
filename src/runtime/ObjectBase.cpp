
#include <intp/runtime.hpp>

namespace NG::runtime
{

    auto ObjectBase::show() const -> Str
    {
        return {};
    }

    auto ObjectBase::boolValue() const -> bool
    {
        return false;
    }

    ObjectBase::~ObjectBase() = default;

}