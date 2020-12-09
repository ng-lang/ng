
#include <intp/runtime.hpp>

namespace NG::runtime {

    NGObject *NGArray::opIndex(NGObject *index) const {

        auto ngInt = dynamic_cast<NGInteger *>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return this->items[ngInt->asSize()];
    }

    NGObject *NGArray::opIndex(NGObject *index, NGObject *newValue) {
        auto ngInt = dynamic_cast<NGInteger *>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return items[ngInt->asSize()] = newValue;
    }

    Str NGArray::show() {
        Str result{};

        for (const auto &item : this->items) {
            if (!result.empty()) {
                result += ", ";
            }

            result += item->show();
        }

        return "[" + result + "]";
    }

    bool NGArray::opEquals(NGObject *other) const {

        if (auto array = dynamic_cast<NGArray *>(other); array != nullptr) {
            if (items.size() != array->items.size()) {
                return false;
            }
            for (size_t i = 0; i < items.size(); ++i) {
                if (!items[i]->opEquals(array->items[i])) {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    bool NGArray::boolValue() {
        return !items.empty();
    }

    NGObject *NGArray::opLShift(NGObject *other) {
        items.push_back(other);

        return this;
    }
}
