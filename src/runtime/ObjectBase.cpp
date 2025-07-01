
#include <intp/runtime.hpp>

namespace NG::runtime {

    Str ObjectBase::show() {
        return NG::Str();
    }

    bool ObjectBase::boolValue() {
        return false;
    }

    ObjectBase::~ObjectBase() = default;

}