#include <typecheck/typeinfo.hpp>
#include <debug.hpp>

namespace NG::typecheck
{
    // inline bool isParamWithDefault(const CheckingRef<TypeInfo> &ref)
    // {
    //     return std::dynamic_pointer_cast<ParamWithDefaultValueType>(ref) != nullptr;
    // }

    // inline CheckingRef<TypeInfo> unwrapParamWithDefault(const CheckingRef<TypeInfo> &ref)
    // {
    //     if (auto param = std::dynamic_pointer_cast<ParamWithDefaultValueType>(ref); param != nullptr)
    //     {
    //         return param->paramType;
    //     }
    //     return ref;
    // }

    auto ArrayType::tag() const -> typeinfo_tag
    {
        return typeinfo_tag::ARRAY;
    }
    auto ArrayType::repr() const -> Str
    {
        return "[" + elementType->repr() + "]";
    }
    auto ArrayType::match(const TypeInfo &other) const -> bool
    {
        if (other.tag() != typeinfo_tag::ARRAY)
        {
            return false;
        }
        const ArrayType &arrayType = static_cast<const ArrayType &>(other);
        // empty array?
        if (arrayType.elementType->tag() == typeinfo_tag::UNTYPED || elementType->tag() == typeinfo_tag::UNTYPED)
        {
            return true;
        }
        return elementType->match(*(arrayType.elementType));
    }

    auto ArrayType::containing(const TypeInfo &other) const -> bool
    {
        return elementType->match(other);
    }
}