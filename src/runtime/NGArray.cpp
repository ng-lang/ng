
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
namespace NG::runtime
{

    auto NGArray::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
    {

        auto ngInt = std::dynamic_pointer_cast<NumeralBase>(index);
        if (ngInt == nullptr)
        {
            throw IllegalTypeException("Not a valid index");
        }

        auto indexVal = NGIntegral<int32_t>::valueOf(ngInt.get());
        if (indexVal < 0 || static_cast<size_t>(indexVal) >= items->size())
        {
            throw RuntimeException("Index out of bounds: " + std::to_string(indexVal));
        }

        return (*this->items)[indexVal];
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    auto NGArray::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
    {
        auto ngInt = std::dynamic_pointer_cast<NumeralBase>(index);
        if (ngInt == nullptr)
        {
            throw IllegalTypeException("Not a valid index");
        }
        auto indexVal = NGIntegral<uint32_t>::valueOf(ngInt.get());
        if (indexVal < 0 || static_cast<size_t>(indexVal) >= items->size())
        {
            throw RuntimeException("Index out of bounds: " + std::to_string(indexVal));
        }

        return (*items)[indexVal] = newValue;
    }

    auto NGArray::show() const -> Str
    {
        Str result{};

        for (const auto &item : (*this->items))
        {
            if (!result.empty())
            {
                result += ", ";
            }

            result += item->show();
        }

        return "[" + result + "]";
    }

    auto NGArray::opEquals(RuntimeRef<NGObject> other) const -> bool
    {

        if (auto array = std::dynamic_pointer_cast<NGArray>(other); array != nullptr)
        {
            if (items->size() != array->items->size())
            {
                return false;
            }
            for (size_t i = 0; i < items->size(); ++i)
            {
                if (!(*items)[i]->opEquals((*array->items)[i]))
                {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    auto NGArray::boolValue() const -> bool
    {
        return !items->empty();
    }

    auto NGArray::opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
    {
        items->push_back(other);
        auto resp = makert<NGArray>();
        resp->items = items;
        return resp;
    }
}
