
#include <intp/runtime.hpp>

namespace NG::runtime
{

    Str ObjectBase::show() const
    {
        return NG::Str();
    }

    bool ObjectBase::boolValue() const
    {
        return false;
    }

    ObjectBase::~ObjectBase() = default;

}