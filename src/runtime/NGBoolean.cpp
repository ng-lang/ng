
#include <intp/runtime.hpp>

namespace NG::runtime
{

    auto NGBoolean::show() const -> Str
    {
        return value ? "true" : "false";
    }

    auto NGBoolean::opEquals(RuntimeRef<NGObject> other) const -> bool
    {
        if (auto otherBoolean = std::dynamic_pointer_cast<NGBoolean>(other); otherBoolean != nullptr)
        {
            return otherBoolean->value == value;
        }
        return false;
    }

    auto NGBoolean::boolValue() const -> bool
    {
        return value;
    }

}