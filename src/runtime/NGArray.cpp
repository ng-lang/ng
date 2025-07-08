
#include <intp/runtime.hpp>

namespace NG::runtime {

    RuntimeRef<NGObject> NGArray::opIndex(RuntimeRef<NGObject> index) const {

        auto ngInt = std::dynamic_pointer_cast<NGInteger>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return (*this->items)[ngInt->asSize()];
    }

    RuntimeRef<NGObject> NGArray::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) {
        auto ngInt = std::dynamic_pointer_cast<NGInteger>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return (*items)[ngInt->asSize()] = newValue;
    }

    Str NGArray::show() {
        Str result{};

        for (const auto &item : (*this->items)) {
            if (!result.empty()) {
                result += ", ";
            }

            result += item->show();
        }

        return "[" + result + "]";
    }

    bool NGArray::opEquals(RuntimeRef<NGObject> other) const {

        if (auto array = std::dynamic_pointer_cast<NGArray>(other); array != nullptr) {
            if (items->size() != array->items->size()) {
                return false;
            }
            for (size_t i = 0; i < items->size(); ++i) {
                if (!(*items)[i]->opEquals((*array->items)[i])) {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    bool NGArray::boolValue() {
        return !items->empty();
    }

    RuntimeRef<NGObject> NGArray::opLShift(RuntimeRef<NGObject> other) {
        items->push_back(other);
        auto resp = makert<NGArray>();
        resp->items = items;
        return resp;
    }
}
