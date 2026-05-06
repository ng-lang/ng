#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{

  auto TypeInfo::tag() const -> typeinfo_tag
  {
    return typeinfo_tag::UNTYPED;
  }
  auto TypeInfo::repr() const -> Str
  {
    return "";
  }
  auto TypeInfo::match(const TypeInfo &other) const -> bool
  {
    return false;
  }
  TypeInfo::~TypeInfo() = default;

  auto Untyped::tag() const -> typeinfo_tag
  {
    return typeinfo_tag::UNTYPED;
  }

  auto Untyped::repr() const -> Str
  {
    return "[untyped]";
  }

  auto Untyped::match(const TypeInfo &other) const -> bool
  {
    return true;
  }

  Untyped::~Untyped() = default;

  auto CustomizedType::tag() const -> typeinfo_tag
  {
    return typeinfo_tag::CUSTOMIZED;
  }

  auto CustomizedType::repr() const -> Str
  {
    return name;
  }

  auto CustomizedType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() == typeinfo_tag::UNTYPED)
    {
      return true;
    }
    if (auto otherCustom = dynamic_cast<const CustomizedType *>(&other); otherCustom != nullptr)
    {
      return name == otherCustom->name;
    }
    return false;
  }

  auto TypeAliasType::tag() const -> typeinfo_tag { return typeinfo_tag::TYPE_ALIAS; }
  auto TypeAliasType::repr() const -> Str { return name; }
  auto TypeAliasType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() == typeinfo_tag::UNTYPED) return true;
    // TypeAlias is transparent — delegate to underlying type
    if (auto otherAlias = dynamic_cast<const TypeAliasType *>(&other))
    {
      return underlyingType->match(*otherAlias->underlyingType);
    }
    // Check underlying type against other, or other against underlying type (symmetric)
    return underlyingType->match(other) || other.match(*underlyingType);
  }

  auto NewTypeType::tag() const -> typeinfo_tag { return typeinfo_tag::NEW_TYPE; }
  auto NewTypeType::repr() const -> Str { return name; }
  auto NewTypeType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() == typeinfo_tag::UNTYPED) return true;
    // NewType is opaque — only matches same newtype by name
    if (auto otherNew = dynamic_cast<const NewTypeType *>(&other))
    {
      return name == otherNew->name;
    }
    return false;
  }

  // TaggedUnionType — nominal match by name
  auto TaggedUnionType::repr() const -> Str
  {
    Str out = name + " = ";
    bool first = true;
    for (auto &[variantName, payloadTypes] : variants)
    {
      if (!first) out += " | ";
      first = false;
      out += variantName + "(";
      for (size_t i = 0; i < payloadTypes.size(); ++i)
      {
        if (i > 0) out += ", ";
        out += payloadTypes[i]->repr();
      }
      out += ")";
    }
    return out;
  }

  auto TaggedUnionType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() == typeinfo_tag::UNTYPED) return true;
    if (auto otherTU = dynamic_cast<const TaggedUnionType *>(&other))
    {
      return name == otherTU->name;
    }
    // A VariantType of this union matches
    if (auto otherVar = dynamic_cast<const VariantType *>(&other))
    {
      return name == otherVar->unionName;
    }
    return false;
  }

  // VariantType — matches its parent TaggedUnionType or same variant
  auto VariantType::repr() const -> Str
  {
    Str out = variantName + "(";
    for (size_t i = 0; i < payloadTypes.size(); ++i)
    {
      if (i > 0) out += ", ";
      out += payloadTypes[i]->repr();
    }
    out += ")";
    return out;
  }

  auto VariantType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() == typeinfo_tag::UNTYPED) return true;
    // Variant matches its parent tagged union
    if (auto otherTU = dynamic_cast<const TaggedUnionType *>(&other))
    {
      return unionName == otherTU->name;
    }
    // Same variant
    if (auto otherVar = dynamic_cast<const VariantType *>(&other))
    {
      return unionName == otherVar->unionName && variantName == otherVar->variantName;
    }
    return false;
  }

  // UnionType — matches if other is one of the member types
  auto UnionType::repr() const -> Str
  {
    Str out;
    for (size_t i = 0; i < types.size(); ++i)
    {
      if (i > 0) out += " | ";
      out += types[i]->repr();
    }
    return out;
  }

  auto UnionType::match(const TypeInfo &other) const -> bool
  {
    if (other.tag() == typeinfo_tag::UNTYPED) return true;
    for (auto &t : types)
    {
      if (t->match(other)) return true;
    }
    return false;
  }
} // namespace NG::typecheck
