
#include <intp/runtime.hpp>

namespace NG::runtime {

    Str NGBoolean::show() const {
        return value ? "true" : "false";
    }

    bool NGBoolean::opEquals(RuntimeRef<NGObject> other) const {
        if (auto otherBoolean = std::dynamic_pointer_cast<NGBoolean>(other); otherBoolean != nullptr) {
            return otherBoolean->value == value;
        }
        return false;
    }

    bool NGBoolean::boolValue() const {
        return value;
    }

}