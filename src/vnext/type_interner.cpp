// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/typecheck.hpp"

#include <format>

namespace NG::vnext::typecheck
{
  TypeInterner::TypeInterner()
    : descriptors_({TypeDescriptor{.kind = TypeKind::Builtin, .name = "<invalid>", .element = TypeId{}, .length = std::nullopt},
                   TypeDescriptor{.kind = TypeKind::Builtin, .name = "i64", .element = TypeId{}, .length = std::nullopt},
                   TypeDescriptor{.kind = TypeKind::Builtin, .name = "u8", .element = TypeId{}, .length = std::nullopt},
                   TypeDescriptor{.kind = TypeKind::Builtin, .name = "bool", .element = TypeId{}, .length = std::nullopt},
                   TypeDescriptor{.kind = TypeKind::Builtin, .name = "unit", .element = TypeId{}, .length = std::nullopt},
                   TypeDescriptor{.kind = TypeKind::Builtin, .name = "string", .element = TypeId{}, .length = std::nullopt}})
  {
  }

  auto TypeInterner::append(TypeDescriptor descriptor) -> TypeId
  {
    descriptors_.push_back(std::move(descriptor));
    return TypeId{static_cast<uint32_t>(descriptors_.size() - 1)};
  }

  auto TypeInterner::internDynamicArray(TypeId element) -> TypeId
  {
    for (uint32_t index = 6; index < descriptors_.size(); ++index)
      if (descriptors_[index].kind == TypeKind::DynamicArray && descriptors_[index].element == element) return TypeId{index};
    return append(TypeDescriptor{.kind = TypeKind::DynamicArray, .name = "array", .element = element, .length = std::nullopt});
  }

  auto TypeInterner::internFixedArray(TypeId element, uint64_t length) -> TypeId
  {
    for (uint32_t index = 6; index < descriptors_.size(); ++index)
      if (descriptors_[index].kind == TypeKind::FixedArray && descriptors_[index].element == element &&
          descriptors_[index].length == length)
        return TypeId{index};
    return append(TypeDescriptor{.kind = TypeKind::FixedArray, .name = "array", .element = element, .length = length});
  }

  auto TypeInterner::resolve(const hir::Type &type) -> TypeId
  {
    if (type.kind == hir::TypeKind::Named)
    {
      if (type.name == "i64") return builtin::I64;
      if (type.name == "u8") return builtin::U8;
      if (type.name == "bool") return builtin::Bool;
      if (type.name == "unit") return builtin::Unit;
      if (type.name == "string") return builtin::String;
      throw TypeError(std::format("unknown type `{}`", type.name), type.span);
    }
    if (type.kind != hir::TypeKind::Applied || type.target == nullptr || type.target->kind != hir::TypeKind::Named)
      throw TypeError("unsupported type form", type.span);
    if (type.target->name != "array") throw TypeError(std::format("unknown type constructor `{}`", type.target->name), type.span);
    if (type.arguments.size() != 1 && type.arguments.size() != 2)
      throw TypeError(std::format("array type expects 1 or 2 arguments, got {}", type.arguments.size()), type.span);
    if (type.arguments[0].kind != syntax::GenericArgumentKind::Type || type.arguments[0].type == nullptr)
      throw TypeError("array element argument must be a type", type.arguments[0].span);
    const TypeId element = resolve(*type.arguments[0].type);
    if (type.arguments.size() == 1) return internDynamicArray(element);
    if (type.arguments[1].kind != syntax::GenericArgumentKind::ConstInteger)
      throw TypeError("array length argument must be a const integer", type.arguments[1].span);
    return internFixedArray(element, type.arguments[1].constInteger);
  }

  auto TypeInterner::descriptor(TypeId type) const -> const TypeDescriptor & { return descriptors_.at(type.value); }

  auto TypeInterner::display(TypeId type) const -> std::string
  {
    const auto &item = descriptor(type);
    if (item.kind == TypeKind::Builtin) return item.name;
    if (item.kind == TypeKind::DynamicArray) return std::format("array<{}>", display(item.element));
    return std::format("array<{}, {}>", display(item.element), *item.length);
  }
} // namespace NG::vnext::typecheck
