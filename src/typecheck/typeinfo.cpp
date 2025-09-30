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
    return false;
  }

  Untyped::~Untyped() = default;
} // namespace NG::typecheck
