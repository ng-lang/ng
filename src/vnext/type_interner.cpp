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

  auto TypeInterner::internDependentArray(TypeId element, uint32_t constParameterIndex, std::string name) -> TypeId
  {
    for (uint32_t index = 6; index < descriptors_.size(); ++index)
      if (descriptors_[index].kind == TypeKind::DependentArray && descriptors_[index].element == element &&
          descriptors_[index].constParameterIndex == constParameterIndex)
        return TypeId{index};
    return append(TypeDescriptor{.kind = TypeKind::DependentArray, .name = "array", .element = element,
                                 .length = std::nullopt, .constParameterIndex = constParameterIndex,
                                 .constParameterName = std::move(name)});
  }

  auto TypeInterner::internReference(TypeId target, bool mutableReference) -> TypeId
  {
    for (uint32_t index = 6; index < descriptors_.size(); ++index)
      if (descriptors_[index].kind == TypeKind::Reference && descriptors_[index].element == target &&
          descriptors_[index].referenceMutable == mutableReference)
        return TypeId{index};
    return append(TypeDescriptor{.kind = TypeKind::Reference, .name = "ref", .element = target,
                                 .length = std::nullopt, .referenceMutable = mutableReference});
  }

  auto TypeInterner::internRawPointer(TypeId target, bool mutablePointee) -> TypeId
  {
    for (uint32_t index = 6; index < descriptors_.size(); ++index)
      if (descriptors_[index].kind == TypeKind::RawPointer && descriptors_[index].element == target &&
          descriptors_[index].referenceMutable == mutablePointee)
        return TypeId{index};
    return append(TypeDescriptor{.kind = TypeKind::RawPointer, .name = "pointer", .element = target,
                                 .length = std::nullopt, .referenceMutable = mutablePointee});
  }

  auto TypeInterner::internTypePack(TypeId element) -> TypeId
  {
    return append(TypeDescriptor{.kind = TypeKind::TypePack, .name = "type pack", .element = element});
  }

  auto TypeInterner::internRange(TypeId element) -> TypeId
  {
    return append(TypeDescriptor{.kind = TypeKind::Range, .name = "range", .element = element});
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

  auto TypeInterner::declareOpaqueType(std::string name, bool abstract, syntax::SourceSpan span) -> TypeId
  {
    if (namedTypes_.contains(name))
      throw TypeError(std::format("duplicate type declaration `{}`", name), span);
    TypeId type = append(TypeDescriptor{.kind = TypeKind::Opaque, .name = name, .abstractType = abstract});
    namedTypes_.emplace(std::move(name), type);
    return type;
  }

  auto TypeInterner::declareStruct(hir::StructId id, std::string name) -> TypeId
  {
    if (const auto found = structTypes_.find(id.value); found != structTypes_.end()) return found->second;
    // The base descriptor is a declaration marker; generic instantiations
    // intern their own descriptors with concrete field types. A zero length
    // keeps the marker valid for bytecode verification.
    const TypeId result = append(TypeDescriptor{.kind = TypeKind::Struct,
                                                 .name = std::move(name),
                                                 .element = TypeId{},
                                                 .length = 0,
                                                 .nominalId = id.value});
    structTypes_.emplace(id.value, result);
    namedTypes_.emplace(descriptors_[result.value].name, result);
    return result;
  }

  void TypeInterner::registerStructTemplate(const hir::Struct &structure)
  {
    structTemplates_[structure.id.value] = &structure;
    structGenericParameters_[structure.id.value] = structure.genericParameters;
  }

  auto TypeInterner::structGenericArity(hir::StructId id) const -> size_t
  {
    return structGenericParameters_.at(id.value).size();
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

  auto TypeInterner::specialize(TypeId type, const std::unordered_map<uint32_t, TypeId> &bindings) -> TypeId
  {
    return specialize(type, bindings, {});
  }

  auto TypeInterner::specialize(TypeId type, const std::unordered_map<uint32_t, TypeId> &bindings,
                                const ConstSubstitution &constBindings) -> TypeId
  {
    const auto parameter = bindings.find(type.value);
    if (parameter != bindings.end()) return parameter->second;
    const auto &source = descriptor(type);
    if (source.kind == TypeKind::Range)
      return internRange(specialize(source.element, bindings, constBindings));
    if (source.kind == TypeKind::DynamicArray)
      return internDynamicArray(specialize(source.element, bindings, constBindings));
    if (source.kind == TypeKind::FixedArray)
      return internFixedArray(specialize(source.element, bindings, constBindings), *source.length);
    if (source.kind == TypeKind::DependentArray)
    {
      const TypeId element = specialize(source.element, bindings, constBindings);
      const auto bound = constBindings.find(*source.constParameterIndex);
      if (bound == constBindings.end())
        return internDependentArray(element, *source.constParameterIndex, source.constParameterName);
      const auto &value = constInterner_.value(bound->second);
      if (value.kind != const_eval::ConstValueKind::Integer)
        throw std::logic_error("const generic argument is not an integer");
      return internFixedArray(element, static_cast<uint64_t>(value.integerValue));
    }
    if (source.kind == TypeKind::Reference)
      return internReference(specialize(source.element, bindings, constBindings), source.referenceMutable);
    if (source.kind == TypeKind::RawPointer)
      return internRawPointer(specialize(source.element, bindings, constBindings), source.referenceMutable);
    if (source.kind == TypeKind::Tuple)
    {
      std::vector<TypeId> elements;
      for (const auto element : source.elements) elements.push_back(specialize(element, bindings, constBindings));
      return internTuple(elements);
    }
    if (source.kind == TypeKind::Enum && !source.typeArguments.empty())
    {
      std::vector<TypeId> arguments;
      for (const auto argument : source.typeArguments) arguments.push_back(specialize(argument, bindings, constBindings));
      for (uint32_t index = 6; index < descriptors_.size(); ++index)
        if (descriptors_[index].kind == TypeKind::Enum && descriptors_[index].nominalId == source.nominalId &&
            descriptors_[index].typeArguments == arguments) return TypeId{index};
      std::vector<TypeId> payloads;
      for (const auto payload : source.elements) payloads.push_back(specialize(payload, bindings, constBindings));
      auto copy = source;
      copy.typeArguments = std::move(arguments);
      copy.elements = std::move(payloads);
      return append(std::move(copy));
    }
    return type;
  }

  auto TypeInterner::internConstInteger(int64_t value) -> const_eval::ConstValueId
  {
    return constInterner_.internInteger(value);
  }

  auto TypeInterner::resolveInScope(const hir::Type &type, const std::unordered_map<std::string, TypeId> &bindings,
                                    const ConstParamBindings &constBindings) -> TypeId
  {
    return resolveWithBindings(type, bindings, constBindings);
  }

  auto TypeInterner::resolveWithBindings(const hir::Type &type, const std::unordered_map<std::string, TypeId> &bindings,
                                         const ConstParamBindings &constBindings) -> TypeId
  {
    if (type.kind == hir::TypeKind::Named)
    {
      if (const auto found = bindings.find(type.name); found != bindings.end()) return found->second;
      return resolve(type);
    }
    if (type.kind == hir::TypeKind::ScopedReference && type.target != nullptr)
      return internReference(resolveWithBindings(*type.target, bindings, constBindings), type.isMutable);
    if (type.kind == hir::TypeKind::Pack && type.target != nullptr)
      return internTypePack(resolveWithBindings(*type.target, bindings, constBindings));
    if (type.kind == hir::TypeKind::RawPointer && type.target != nullptr)
      return internRawPointer(resolveWithBindings(*type.target, bindings, constBindings), type.isMutable);
    if (type.kind != hir::TypeKind::Applied || type.target == nullptr || type.target->kind != hir::TypeKind::Named)
      return resolve(type);
    if (type.target->name == "array")
    {
      if (type.arguments.size() != 1 && type.arguments.size() != 2)
        throw TypeError(std::format("array type expects 1 or 2 arguments, got {}", type.arguments.size()), type.span);
      const TypeId element = resolveWithBindings(*type.arguments[0].type, bindings, constBindings);
      if (type.arguments.size() == 1) return internDynamicArray(element);
      if (const auto *identifier = dynamic_cast<const syntax::ConstIdentifier *>(type.arguments[1].constExpr.get()))
      {
        if (const auto found = constBindings.find(identifier->name); found != constBindings.end())
          return internDependentArray(element, found->second, identifier->name);
      }
      return internFixedArray(element, evaluateArrayLength(type.arguments[1]));
    }
    if (type.target->name == "tuple")
    {
      std::vector<TypeId> elements;
      for (const auto &argument : type.arguments) elements.push_back(resolveWithBindings(*argument.type, bindings, constBindings));
      return internTuple(elements);
    }
    if (type.target->name == "range")
    {
      if (type.arguments.size() != 1 || type.arguments[0].type == nullptr)
        throw TypeError(std::format("range type expects 1 element argument, got {}", type.arguments.size()), type.span);
      return internRange(resolveWithBindings(*type.arguments[0].type, bindings, constBindings));
    }
    if (const auto introspected = resolveTupleIntrospection(type, bindings, constBindings); introspected.has_value())
      return *introspected;
    const auto constructor = namedTypes_.find(type.target->name);
    if (constructor == namedTypes_.end() ||
        (descriptors_[constructor->second.value].kind != TypeKind::Enum &&
         descriptors_[constructor->second.value].kind != TypeKind::Struct))
      return resolve(type);
    if (descriptors_[constructor->second.value].kind == TypeKind::Struct)
    {
      const uint32_t structId = *descriptors_[constructor->second.value].nominalId;
      const auto &parameters = structGenericParameters_.at(structId);
      if (type.arguments.size() != parameters.size())
        throw TypeError(std::format("struct type `{}` expects {} arguments, got {}", type.target->name,
                                    parameters.size(), type.arguments.size()), type.span);
      std::unordered_map<std::string, TypeId> nested;
      std::vector<TypeId> arguments;
      for (size_t index = 0; index < parameters.size(); ++index)
      {
        const TypeId argument = resolveWithBindings(*type.arguments[index].type, bindings, constBindings);
        nested.emplace(parameters[index], argument);
        arguments.push_back(argument);
      }
      for (uint32_t index = 6; index < descriptors_.size(); ++index)
        if (descriptors_[index].kind == TypeKind::Struct && descriptors_[index].nominalId == structId &&
            descriptors_[index].typeArguments == arguments)
          return TypeId{index};
      const auto *structure = structTemplates_.at(structId);
      std::vector<std::string> fields;
      std::vector<TypeId> fieldTypes;
      for (const auto &field : structure->fields)
      {
        fields.push_back(field.name);
        fieldTypes.push_back(resolveWithBindings(field.type, nested, constBindings));
      }
      return append(TypeDescriptor{.kind = TypeKind::Struct, .name = type.target->name, .element = TypeId{},
                                   .length = fieldTypes.size(), .elements = std::move(fieldTypes),
                                   .nominalId = structId, .fieldNames = std::move(fields),
                                   .typeArguments = std::move(arguments)});
    }
    const uint32_t enumId = *descriptors_[constructor->second.value].nominalId;
    const auto &parameters = enumGenericParameters_.at(enumId);
    if (type.arguments.size() != parameters.size())
      throw TypeError(std::format("enum type `{}` expects {} arguments, got {}", type.target->name, parameters.size(), type.arguments.size()), type.span);
    std::unordered_map<std::string, TypeId> nested;
    std::vector<TypeId> arguments;
    for (size_t index = 0; index < parameters.size(); ++index)
    {
      const TypeId argument = resolveWithBindings(*type.arguments[index].type, bindings, constBindings);
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
      payloads.push_back(variant.payloadType != nullptr ? resolveWithBindings(*variant.payloadType, nested, constBindings) : builtin::Unit);
    }
    return append(TypeDescriptor{.kind = TypeKind::Enum, .name = type.target->name, .element = TypeId{},
                                 .length = payloads.size(), .elements = std::move(payloads), .nominalId = enumId,
                                 .fieldNames = std::move(names), .variantHasPayload = std::move(hasPayload),
                                 .typeArguments = std::move(arguments)});
  }

  auto TypeInterner::resolveTupleIntrospection(const hir::Type &type,
                                               const std::unordered_map<std::string, TypeId> &bindings,
                                               const ConstParamBindings &constBindings) -> std::optional<TypeId>
  {
    if (type.target == nullptr || type.target->kind != hir::TypeKind::Named) return std::nullopt;
    const std::string &name = type.target->name;
    if (name == "tuple_element")
    {
      if (type.arguments.size() != 2 || type.arguments[0].type == nullptr)
        throw TypeError("tuple_element<T, I> expects a tuple type and a const index", type.span);
      const TypeId tuple = resolveWithBindings(*type.arguments[0].type, bindings, constBindings);
      const auto &descriptor = this->descriptor(tuple);
      if (descriptor.kind != TypeKind::Tuple)
        throw TypeError(std::format("tuple_element<T, I> expects a tuple type as T, got {}", display(tuple)), type.span);
      const uint64_t index = evaluateArrayLength(type.arguments[1]);
      if (index >= descriptor.elements.size())
        throw TypeError(std::format("tuple_element index out of range: index {}, length {}", index,
                                    descriptor.elements.size()), type.span);
      return descriptor.elements[static_cast<size_t>(index)];
    }
    if (name == "tuple_concat")
    {
      if (type.arguments.size() != 2 || type.arguments[0].type == nullptr || type.arguments[1].type == nullptr)
        throw TypeError("tuple_concat<A, B> expects two tuple types", type.span);
      const TypeId first = resolveWithBindings(*type.arguments[0].type, bindings, constBindings);
      const TypeId second = resolveWithBindings(*type.arguments[1].type, bindings, constBindings);
      const auto &firstDescriptor = descriptor(first);
      const auto &secondDescriptor = descriptor(second);
      if (firstDescriptor.kind != TypeKind::Tuple || secondDescriptor.kind != TypeKind::Tuple)
        throw TypeError("tuple_concat<A, B> expects tuple types", type.span);
      std::vector<TypeId> elements = firstDescriptor.elements;
      elements.insert(elements.end(), secondDescriptor.elements.begin(), secondDescriptor.elements.end());
      return internTuple(elements);
    }
    return std::nullopt;
  }

  auto TypeInterner::internTypeParameter(std::string name, uint32_t index) -> TypeId
  {
    return append(TypeDescriptor{.kind = TypeKind::TypeParameter, .name = std::move(name), .element = TypeId{},
                                 .length = std::nullopt, .nominalId = index});
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
        if (descriptor.kind == TypeKind::Struct && descriptor.nominalId.has_value())
        {
          const size_t arity = structGenericParameters_.at(*descriptor.nominalId).size();
          if (arity != 0)
            throw TypeError(std::format("struct type `{}` expects {} arguments, got 0", type.name, arity), type.span);
        }
        return found->second;
      }
      throw TypeError(std::format("unknown type `{}`", type.name), type.span);
    }
    if (type.kind == hir::TypeKind::ScopedReference && type.target != nullptr)
      return internReference(resolve(*type.target), type.isMutable);
    if (type.kind == hir::TypeKind::RawPointer && type.target != nullptr)
      return internRawPointer(resolve(*type.target), type.isMutable);
    if (type.kind != hir::TypeKind::Applied || type.target == nullptr || type.target->kind != hir::TypeKind::Named)
      throw TypeError("unsupported type form", type.span);
    if (const auto introspected = resolveTupleIntrospection(type, {}, {}); introspected.has_value())
      return *introspected;
    if (type.target->name != "array" && type.target->name != "tuple")
    {
      const auto constructor = namedTypes_.find(type.target->name);
      if (constructor == namedTypes_.end() ||
          (descriptors_[constructor->second.value].kind != TypeKind::Enum &&
           descriptors_[constructor->second.value].kind != TypeKind::Struct))
        throw TypeError(std::format("unknown type constructor `{}`", type.target->name), type.span);
      if (descriptors_[constructor->second.value].kind == TypeKind::Struct)
      {
        const uint32_t structId = descriptors_[constructor->second.value].nominalId.value();
        const auto &parameters = structGenericParameters_.at(structId);
        if (type.arguments.size() != parameters.size())
          throw TypeError(std::format("struct type `{}` expects {} arguments, got {}", type.target->name,
                                      parameters.size(), type.arguments.size()), type.span);
        std::unordered_map<std::string, TypeId> nested;
        std::vector<TypeId> arguments;
        for (size_t index = 0; index < parameters.size(); ++index)
        {
          if (type.arguments[index].kind != syntax::GenericArgumentKind::Type || type.arguments[index].type == nullptr)
            throw TypeError("struct type arguments must be types", type.arguments[index].span);
          const TypeId argument = resolve(*type.arguments[index].type);
          nested.emplace(parameters[index], argument);
          arguments.push_back(argument);
        }
        for (uint32_t index = 6; index < descriptors_.size(); ++index)
          if (descriptors_[index].kind == TypeKind::Struct && descriptors_[index].nominalId == structId &&
              descriptors_[index].typeArguments == arguments)
            return TypeId{index};
        const auto *structure = structTemplates_.at(structId);
        std::vector<std::string> fields;
        std::vector<TypeId> fieldTypes;
        for (const auto &field : structure->fields)
        {
          fields.push_back(field.name);
          fieldTypes.push_back(resolveWithBindings(field.type, nested, {}));
        }
        return append(TypeDescriptor{.kind = TypeKind::Struct, .name = type.target->name, .element = TypeId{},
                                     .length = fieldTypes.size(), .elements = std::move(fieldTypes),
                                     .nominalId = structId, .fieldNames = std::move(fields),
                                     .typeArguments = std::move(arguments)});
      }
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
        payloads.push_back(variant.payloadType != nullptr ? resolveWithBindings(*variant.payloadType, bindings, {}) : builtin::Unit);
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
    if (type.arguments[1].kind != syntax::GenericArgumentKind::ConstExpr)
      throw TypeError("array length argument must be a const expression", type.arguments[1].span);
    return internFixedArray(element, evaluateArrayLength(type.arguments[1]));
  }

  auto TypeInterner::evaluateArrayLength(const hir::TypeArgument &argument) -> uint64_t
  {
    if (argument.constExpr == nullptr) throw TypeError("array length argument must be a const expression", argument.span);
    try
    {
      return const_eval::ConstEvaluator{constInterner_}.evaluateArrayLength(*argument.constExpr, {});
    }
    catch (const const_eval::ConstEvalError &error)
    {
      throw TypeError(error.what(), error.span);
    }
  }

  auto TypeInterner::descriptor(TypeId type) const -> const TypeDescriptor & { return descriptors_.at(type.value); }

  auto TypeInterner::display(TypeId type) const -> std::string
  {
    const auto &item = descriptor(type);
    if (item.kind == TypeKind::Builtin) return item.name;
    if (item.kind == TypeKind::DynamicArray) return std::format("array<{}>", display(item.element));
    if (item.kind == TypeKind::FixedArray) return std::format("array<{}, {}>", display(item.element), *item.length);
    if (item.kind == TypeKind::DependentArray)
      return std::format("array<{}, {}>", display(item.element), item.constParameterName);
    if (item.kind == TypeKind::Reference)
      return std::format("{} {}", display(item.element), item.referenceMutable ? "ref mut" : "ref");
    if (item.kind == TypeKind::RawPointer)
      return std::format("{} {}", display(item.element), item.referenceMutable ? "*mut" : "*const");
    if (item.kind == TypeKind::Struct || item.kind == TypeKind::Enum || item.kind == TypeKind::TypeParameter ||
        item.kind == TypeKind::Opaque)
      return item.name;
    if (item.kind == TypeKind::TypePack) return display(item.element) + "...";
    if (item.kind == TypeKind::Range) return "range<" + display(item.element) + ">";
    std::string result{"tuple<"};
    for (size_t index = 0; index < item.elements.size(); ++index)
    {
      if (index != 0) result += ", ";
      result += display(item.elements[index]);
    }
    return result + ">";
  }
} // namespace NG::vnext::typecheck
