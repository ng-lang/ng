#include <debug.hpp>
#include <typecheck/typeinfo.hpp>

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
    if (length)
    {
      return "array<" + elementType->repr() + ", " + length->repr() + ">";
    }
    return "array<" + elementType->repr() + ", ?>";
  }
  auto ArrayType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() != typeinfo_tag::ARRAY)
    {
      return false;
    }
    const ArrayType &arrayType = static_cast<const ArrayType &>(other);
    if (arrayType.elementType->tag() == typeinfo_tag::UNTYPED || elementType->tag() == typeinfo_tag::UNTYPED)
    {
      return true;
    }
    if (length && arrayType.length && !length->match(*arrayType.length))
    {
      return false;
    }
    return elementType->match(*(arrayType.elementType));
  }

  auto ArrayType::containing(const TypeInfo &other) const -> bool
  {
    return elementType->match(other);
  }

  auto VectorType::repr() const -> Str
  {
    return "vector<" + elementType->repr() + ">";
  }

  auto VectorType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() != typeinfo_tag::VECTOR)
    {
      return false;
    }
    const auto &vectorType = static_cast<const VectorType &>(other);
    if (vectorType.elementType->tag() == typeinfo_tag::UNTYPED || elementType->tag() == typeinfo_tag::UNTYPED)
    {
      return true;
    }
    return elementType->match(*vectorType.elementType);
  }

  auto VectorType::containing(const TypeInfo &other) const -> bool
  {
    return elementType->match(other);
  }

  auto SpanType::repr() const -> Str
  {
    return "span<" + elementType->repr() + ">";
  }

  auto SpanType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() != typeinfo_tag::SPAN)
    {
      return false;
    }
    const auto &spanType = static_cast<const SpanType &>(other);
    if (spanType.elementType->tag() == typeinfo_tag::UNTYPED || elementType->tag() == typeinfo_tag::UNTYPED)
    {
      return true;
    }
    return elementType->match(*spanType.elementType);
  }

  auto SpanType::containing(const TypeInfo &other) const -> bool
  {
    return elementType->match(other);
  }

  auto RangeType::repr() const -> Str
  {
    return "Range<" + elementType->repr() + ">";
  }

  auto RangeType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() != typeinfo_tag::RANGE)
    {
      return false;
    }
    const auto &rangeType = static_cast<const RangeType &>(other);
    if (rangeType.elementType->tag() == typeinfo_tag::UNTYPED || elementType->tag() == typeinfo_tag::UNTYPED)
    {
      return true;
    }
    return elementType->match(*rangeType.elementType);
  }

  auto RangeType::containing(const TypeInfo &other) const -> bool
  {
    return elementType->match(other);
  }
} // namespace NG::typecheck
