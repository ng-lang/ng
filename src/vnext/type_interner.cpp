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
    namedTypes_.emplace("i64", builtin::I64);
    namedTypes_.emplace("u8", builtin::U8);
    namedTypes_.emplace("bool", builtin::Bool);
    namedTypes_.emplace("unit", builtin::Unit);
    namedTypes_.emplace("string", builtin::String);
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

  auto TypeInterner::internTuple(const std::vector<TypeId> &elements) -> TypeId
  {
    for (uint32_t index = 6; index < descriptors_.size(); ++index)
      if (descriptors_[index].kind == TypeKind::Tuple && descriptors_[index].elements == elements) return TypeId{index};
    return append(TypeDescriptor{.kind = TypeKind::Tuple,
                                 .name = "tuple",
                                 .element = TypeId{},
                                 .length = static_cast<uint64_t>(elements.size()),
                                 .elements = elements});
  }

  auto TypeInterner::declareStruct(hir::StructId id, std::string name) -> TypeId
  {
    if (const auto found = structTypes_.find(id.value); found != structTypes_.end()) return found->second;
    const TypeId result = append(TypeDescriptor{.kind = TypeKind::Struct,
                                                 .name = std::move(name),
                                                 .element = TypeId{},
                                                 .length = std::nullopt,
                                                 .nominalId = id.value});
    structTypes_.emplace(id.value, result);
    namedTypes_.emplace(descriptors_[result.value].name, result);
    return result;
  }

  void TypeInterner::defineStruct(hir::StructId id, std::vector<std::string> fields, std::vector<TypeId> types)
  {
    const auto type = typeForStruct(id);
    auto &descriptor = descriptors_.at(type.value);
    descriptor.fieldNames = std::move(fields);
    descriptor.elements = std::move(types);
    descriptor.length = descriptor.elements.size();
  }

  auto TypeInterner::typeForStruct(hir::StructId id) const -> TypeId
  {
    return structTypes_.at(id.value);
  }

  auto TypeInterner::declareEnum(hir::EnumId id, std::string name, std::vector<std::string> genericParameters) -> TypeId
  {
    if (const auto found = enumTypes_.find(id.value); found != enumTypes_.end()) return found->second;
    const TypeId result = append(TypeDescriptor{.kind = TypeKind::Enum,
                                                 .name = std::move(name),
                                                 .element = TypeId{},
                                                 .length = std::nullopt,
                                                 .nominalId = id.value});
    enumTypes_.emplace(id.value, result);
    enumGenericParameters_.emplace(id.value, std::move(genericParameters));
    namedTypes_.emplace(descriptors_[result.value].name, result);
    return result;
  }

  void TypeInterner::registerEnumTemplate(const hir::Enum &enumeration)
  {
    enumTemplates_[enumeration.id.value] = &enumeration;
  }

  void TypeInterner::defineEnum(hir::EnumId id, std::vector<std::string> variants, std::vector<TypeId> payloads,
                                std::vector<bool> hasPayload)
  {
    auto &descriptor = descriptors_.at(typeForEnum(id).value);
    descriptor.fieldNames = std::move(variants);
    descriptor.elements = std::move(payloads);
    descriptor.variantHasPayload = std::move(hasPayload);
    descriptor.length = descriptor.elements.size();
  }

  auto TypeInterner::typeForEnum(hir::EnumId id) const -> TypeId { return enumTypes_.at(id.value); }

  auto TypeInterner::enumGenericArity(hir::EnumId id) const -> size_t
  {
    return enumGenericParameters_.at(id.value).size();
  }

  auto TypeInterner::resolveWithBindings(const hir::Type &type, const std::unordered_map<std::string, TypeId> &bindings) -> TypeId
  {
    if (type.kind == hir::TypeKind::Named)
    {
      if (const auto found = bindings.find(type.name); found != bindings.end()) return found->second;
      return resolve(type);
    }
    if (type.kind != hir::TypeKind::Applied || type.target == nullptr || type.target->kind != hir::TypeKind::Named)
      return resolve(type);
    if (type.target->name == "array")
    {
      if (type.arguments.size() != 1 && type.arguments.size() != 2)
        throw TypeError(std::format("array type expects 1 or 2 arguments, got {}", type.arguments.size()), type.span);
      const TypeId element = resolveWithBindings(*type.arguments[0].type, bindings);
      return type.arguments.size() == 1 ? internDynamicArray(element)
                                        : internFixedArray(element, type.arguments[1].constInteger);
    }
    if (type.target->name == "tuple")
    {
      std::vector<TypeId> elements;
      for (const auto &argument : type.arguments) elements.push_back(resolveWithBindings(*argument.type, bindings));
      return internTuple(elements);
    }
    const auto constructor = namedTypes_.find(type.target->name);
    if (constructor == namedTypes_.end() || descriptors_[constructor->second.value].kind != TypeKind::Enum) return resolve(type);
    const uint32_t enumId = *descriptors_[constructor->second.value].nominalId;
    const auto &parameters = enumGenericParameters_.at(enumId);
    if (type.arguments.size() != parameters.size())
      throw TypeError(std::format("enum type `{}` expects {} arguments, got {}", type.target->name, parameters.size(), type.arguments.size()), type.span);
    std::unordered_map<std::string, TypeId> nested;
    std::vector<TypeId> arguments;
    for (size_t index = 0; index < parameters.size(); ++index)
    {
      const TypeId argument = resolveWithBindings(*type.arguments[index].type, bindings);
      nested.emplace(parameters[index], argument);
      arguments.push_back(argument);
    }
    for (uint32_t index = 6; index < descriptors_.size(); ++index)
      if (descriptors_[index].kind == TypeKind::Enum && descriptors_[index].nominalId == enumId &&
          descriptors_[index].typeArguments == arguments) return TypeId{index};
    const auto *enumeration = enumTemplates_.at(enumId);
    std::vector<TypeId> payloads;
    std::vector<bool> hasPayload;
    std::vector<std::string> names;
    for (const auto &variant : enumeration->variants)
    {
      names.push_back(variant.name);
      hasPayload.push_back(variant.payloadType != nullptr);
      payloads.push_back(variant.payloadType != nullptr ? resolveWithBindings(*variant.payloadType, nested) : builtin::Unit);
    }
    return append(TypeDescriptor{.kind = TypeKind::Enum, .name = type.target->name, .element = TypeId{},
                                 .length = payloads.size(), .elements = std::move(payloads), .nominalId = enumId,
                                 .fieldNames = std::move(names), .variantHasPayload = std::move(hasPayload),
                                 .typeArguments = std::move(arguments)});
  }

  auto TypeInterner::resolve(const hir::Type &type) -> TypeId
  {
    if (type.kind == hir::TypeKind::Named)
    {
      if (const auto found = namedTypes_.find(type.name); found != namedTypes_.end())
      {
        const auto &descriptor = descriptors_[found->second.value];
        if (descriptor.kind == TypeKind::Enum && descriptor.nominalId.has_value())
        {
          const size_t arity = enumGenericParameters_.at(*descriptor.nominalId).size();
          if (arity != 0)
            throw TypeError(std::format("enum type `{}` expects {} arguments, got 0", type.name, arity), type.span);
        }
        return found->second;
      }
      throw TypeError(std::format("unknown type `{}`", type.name), type.span);
    }
    if (type.kind != hir::TypeKind::Applied || type.target == nullptr || type.target->kind != hir::TypeKind::Named)
      throw TypeError("unsupported type form", type.span);
    if (type.target->name != "array" && type.target->name != "tuple")
    {
      const auto constructor = namedTypes_.find(type.target->name);
      if (constructor == namedTypes_.end() || descriptors_[constructor->second.value].kind != TypeKind::Enum)
        throw TypeError(std::format("unknown type constructor `{}`", type.target->name), type.span);
      const auto enumId = descriptors_[constructor->second.value].nominalId.value();
      const auto &parameters = enumGenericParameters_.at(enumId);
      if (type.arguments.size() != parameters.size())
        throw TypeError(std::format("enum type `{}` expects {} arguments, got {}", type.target->name, parameters.size(), type.arguments.size()), type.span);
      std::unordered_map<std::string, TypeId> bindings;
      for (size_t index = 0; index < parameters.size(); ++index)
      {
        if (type.arguments[index].kind != syntax::GenericArgumentKind::Type || type.arguments[index].type == nullptr)
          throw TypeError("enum type arguments must be types", type.arguments[index].span);
        bindings.emplace(parameters[index], resolve(*type.arguments[index].type));
      }
      const auto *enumeration = enumTemplates_.at(enumId);
      std::vector<TypeId> payloads;
      std::vector<bool> hasPayload;
      for (const auto &variant : enumeration->variants)
      {
        hasPayload.push_back(variant.payloadType != nullptr);
        payloads.push_back(variant.payloadType != nullptr ? resolveWithBindings(*variant.payloadType, bindings) : builtin::Unit);
      }
      for (uint32_t index = 6; index < descriptors_.size(); ++index)
        if (descriptors_[index].kind == TypeKind::Enum && descriptors_[index].nominalId == enumId && descriptors_[index].typeArguments.size() == bindings.size())
        {
          bool same = true;
          for (size_t arg = 0; arg < type.arguments.size(); ++arg) same = same && descriptors_[index].typeArguments[arg] == bindings.at(parameters[arg]);
          if (same) return TypeId{index};
        }
      std::vector<TypeId> arguments;
      for (const auto &parameter : parameters) arguments.push_back(bindings.at(parameter));
      return append(TypeDescriptor{.kind = TypeKind::Enum, .name = type.target->name, .element = TypeId{},
                                   .length = payloads.size(), .elements = std::move(payloads),
                                   .nominalId = enumId, .fieldNames = [&enumeration] { std::vector<std::string> names; for (const auto &v : enumeration->variants) names.push_back(v.name); return names; }(),
                                   .variantHasPayload = std::move(hasPayload), .typeArguments = std::move(arguments)});
    }
    if (type.target->name == "tuple")
    {
      if (type.arguments.empty()) throw TypeError("tuple type expects at least 1 argument, got 0", type.span);
      std::vector<TypeId> elements;
      elements.reserve(type.arguments.size());
      for (const auto &argument : type.arguments)
      {
        if (argument.kind != syntax::GenericArgumentKind::Type || argument.type == nullptr)
          throw TypeError("tuple arguments must be types", argument.span);
        elements.push_back(resolve(*argument.type));
      }
      return internTuple(elements);
    }
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
    if (item.kind == TypeKind::FixedArray) return std::format("array<{}, {}>", display(item.element), *item.length);
    if (item.kind == TypeKind::Struct || item.kind == TypeKind::Enum) return item.name;
    std::string result{"tuple<"};
    for (size_t index = 0; index < item.elements.size(); ++index)
    {
      if (index != 0) result += ", ";
      result += display(item.elements[index]);
    }
    return result + ">";
  }
} // namespace NG::vnext::typecheck
