#include <algorithm>
#include <debug.hpp>
#include <functional>
#include <iterator>
#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{
  inline bool isParamWithDefault(const CheckingRef<TypeInfo> &ref)
  {
    return std::dynamic_pointer_cast<ParamWithDefaultValueType>(ref) != nullptr;
  }

  inline CheckingRef<TypeInfo> unwrapParamWithDefault(const CheckingRef<TypeInfo> &ref)
  {
    if (auto param = std::dynamic_pointer_cast<ParamWithDefaultValueType>(ref); param != nullptr)
    {
      return param->paramType;
    }
    return ref;
  }

  auto FunctionType::tag() const -> typeinfo_tag
  {
    return typeinfo_tag::FUNCTION;
  }
  auto FunctionType::repr() const -> Str
  {
    Str result = "fun (";

    for (size_t i = 0; i < parametersType.size(); i++)
    {
      result += parametersType[i]->repr();
      if (i < parametersType.size() - 1)
      {
        result += ", ";
      }
    }
    result += (") -> " + returnType->repr());

    return result;
  }
  auto FunctionType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() == typeinfo_tag::FUNCTION)
    {
      const FunctionType &otherFunction = static_cast<const FunctionType &>(other);
      if (!this->returnType->match(*otherFunction.returnType))
      {
        return false;
      }
      Vec<CheckingRef<TypeInfo>> actualTypes{};
      actualTypes.reserve(this->parametersType.size());
      std::transform(this->parametersType.begin(), this->parametersType.end(), std::back_inserter(actualTypes),
                     unwrapParamWithDefault);
      return otherFunction.applyWith(actualTypes);
    }
    return false;
  }

  auto FunctionType::applyWith(const Vec<CheckingRef<TypeInfo>> &arguments) const -> bool
  {

    // how many required arguments (that without default value)
    auto requiredSize =
        std::count_if(this->parametersType.begin(), this->parametersType.end(), std::not_fn(isParamWithDefault));

    if (arguments.size() < requiredSize || arguments.size() > parametersType.size())
    {
      return false;
    }
    for (size_t i = 0; i < arguments.size(); i++)
    {
      if (!parametersType[i]->match(*arguments[i]))
      {
        return false;
      }
    }
    return true;
  }

  auto ParamWithDefaultValueType::repr() const -> Str
  {
    return paramType->repr() + " = default";
  }

  auto ParamWithDefaultValueType::match(const TypeInfo &other) const -> bool
  {
    return paramType->match(other);
  }

  auto ParamWithDefaultValueType::tag() const -> typeinfo_tag
  {
    return typeinfo_tag::PARAM_WITH_DEFAULT_VALUE;
  }
} // namespace NG::typecheck