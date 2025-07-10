
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
namespace NG::runtime {

    RuntimeRef<NGObject> NGArray::opIndex(RuntimeRef<NGObject> index) const {

        auto ngInt = std::dynamic_pointer_cast<NumeralBase>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        auto indexVal = NGIntegral<int32_t>::valueOf(ngInt.get());

        return (*this->items)[indexVal];
    }

    RuntimeRef<NGObject> NGArray::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) {
        auto ngInt = std::dynamic_pointer_cast<NumeralBase>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }
        auto indexVal = NGIntegral<uint32_t>::valueOf(ngInt.get());

        return (*items)[indexVal] = newValue;
    }

    Str NGArray::show() const {
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

    bool NGArray::boolValue() const {
        return !items->empty();
    }

    RuntimeRef<NGObject> NGArray::opLShift(RuntimeRef<NGObject> other) {
        items->push_back(other);
        auto resp = makert<NGArray>();
        resp->items = items;
        return resp;
    }
}
