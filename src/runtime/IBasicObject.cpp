
#include <intp/runtime.hpp>

namespace NG::runtime {

    Str IBasicObject::show() {
        return NG::Str();
    }

    bool IBasicObject::boolValue() {
        return false;
    }

    IBasicObject::~IBasicObject() = default;

}