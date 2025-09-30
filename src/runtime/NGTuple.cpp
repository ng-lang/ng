#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

namespace NG::runtime
{

  auto NGTuple::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
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
  };

  auto NGTuple::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
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

    return (*items)[indexVal] = newValue;
  };

  auto NGTuple::show() const -> Str
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

    return "(" + result + ")";
  };

  auto NGTuple::boolValue() const -> bool
  {
    return !items->empty();
  };

  auto NGTuple::opEquals(RuntimeRef<NGObject> other) const -> bool
  {
    if (this == other.get())
    {
      return true;
    }
    auto otherTuple = std::dynamic_pointer_cast<NGTuple>(other);
    if (!otherTuple)
    {
      return false;
    }
    if (items->size() != otherTuple->items->size())
    {
      return false;
    }
    for (size_t i = 0; i < items->size(); ++i)
    {
      if (!(*items)[i]->opEquals((*otherTuple->items)[i]))
      {
        return false;
      }
    }
    return true;
  };

  auto NGTuple::type() const -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> tupleType = makert<NGType>(NGType{.name = "Tuple", .memberFunctions = {}});
    return tupleType;
  };

  auto NGTuple::respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject>
  {
    if (member == "size")
    {
      context->retVal = makert<NGIntegral<uint32_t>>(items->size());
      return context->retVal;
    }
    try
    {
      if (auto result = std::stoi(member); true)
      {
        context->retVal = this->opIndex(makert<NGIntegral<int32_t>>(result));
        return context->retVal;
      }
    }
    catch (const std::invalid_argument &)
    {
      // Not an integer, fall through
    }
    return NGObject::respond(member, context, invocationContext);
  }
} // namespace NG::runtime