// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck_unify.cpp — unification, specialization, instantiation.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

auto Checker::isGeneric(const FunctionTypeIds &signature) const -> bool
{
  return !signature.genericParameters.empty() || !signature.packParameters.empty() ||
          !signature.constParameters.empty() || !signature.constructorParameters.empty();
}

void Checker::requireConstArguments(const FunctionTypeIds &signature, const Substitution &substitution,
                                     std::string_view functionName, syntax::SourceSpan span) const
{
  for (size_t index = 0; index < signature.constParameters.size(); ++index)
  {
    if (!substitution.consts.contains(static_cast<uint32_t>(index)))
      throw TypeError(std::format("cannot infer const generic argument `{}` for function `{}`",
                                  signature.constParameterNames[index], functionName),
                      span);
  }
}

void Checker::record(const hir::Expression &expression, TypeId type)
{
  expressionTypeIds_.insert_or_assign(&expression, type);
  expressionTypes_.insert_or_assign(&expression, interner_.display(type));
}

void Checker::recordLocal(hir::LocalId local, TypeId type)
{
  localTypeIds_.insert_or_assign(local.value, type);
  localDisplayTypes_.insert_or_assign(local.value, interner_.display(type));
}

auto Checker::typeFromNumericSuffix(std::string_view suffix) const -> TypeId
{
  if (suffix == "i8")
    return builtin::I8;
  if (suffix == "i16")
    return builtin::I16;
  if (suffix == "i32")
    return builtin::I32;
  if (suffix == "i64")
    return builtin::I64;
  if (suffix == "u8")
    return builtin::U8;
  if (suffix == "u16")
    return builtin::U16;
  if (suffix == "u32")
    return builtin::U32;
  if (suffix == "u64")
    return builtin::U64;
  if (suffix == "f32")
    return builtin::F32;
  if (suffix == "f64")
    return builtin::F64;
  return TypeId{};
}

auto Checker::parseIntegerLiteralMagnitude(std::string_view text, TypeId type, syntax::SourceSpan span) const
    -> uint64_t
{
  uint64_t magnitude{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), magnitude);
  if (error != std::errc{} || end != text.data() + text.size())
    throw TypeError(
        std::format("integer literal `{}` is out of range for type {}", text, interner_.display(type)), span);
  return magnitude;
}

void Checker::checkIntegerLiteralMagnitude(uint64_t magnitude, std::string_view text, TypeId type,
                                            syntax::SourceSpan span) const
{
  uint64_t maximum{};
  switch (type.value)
  {
  case builtin::I8.value:
    maximum = static_cast<uint64_t>(std::numeric_limits<int8_t>::max());
    break;
  case builtin::I16.value:
    maximum = static_cast<uint64_t>(std::numeric_limits<int16_t>::max());
    break;
  case builtin::I32.value:
    maximum = static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
    break;
  case builtin::I64.value:
    maximum = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    break;
  case builtin::U8.value:
    maximum = std::numeric_limits<uint8_t>::max();
    break;
  case builtin::U16.value:
    maximum = std::numeric_limits<uint16_t>::max();
    break;
  case builtin::U32.value:
    maximum = std::numeric_limits<uint32_t>::max();
    break;
  case builtin::U64.value:
    maximum = std::numeric_limits<uint64_t>::max();
    break;
  default:
    return;
  }
  if (magnitude > maximum)
    throw TypeError(
        std::format("integer literal `{}` is out of range for type {}", text, interner_.display(type)), span);
}

void Checker::checkIntegerLiteral(int64_t value, std::string_view text, TypeId type, syntax::SourceSpan span) const
{
  int64_t minimum{};
  uint64_t maximum{};
  switch (type.value)
  {
  case builtin::I8.value:
    minimum = std::numeric_limits<int8_t>::min();
    maximum = std::numeric_limits<int8_t>::max();
    break;
  case builtin::I16.value:
    minimum = std::numeric_limits<int16_t>::min();
    maximum = std::numeric_limits<int16_t>::max();
    break;
  case builtin::I32.value:
    minimum = std::numeric_limits<int32_t>::min();
    maximum = std::numeric_limits<int32_t>::max();
    break;
  case builtin::I64.value:
    minimum = std::numeric_limits<int64_t>::min();
    maximum = std::numeric_limits<int64_t>::max();
    break;
  case builtin::U8.value:
    minimum = 0;
    maximum = std::numeric_limits<uint8_t>::max();
    break;
  case builtin::U16.value:
    minimum = 0;
    maximum = std::numeric_limits<uint16_t>::max();
    break;
  case builtin::U32.value:
    minimum = 0;
    maximum = std::numeric_limits<uint32_t>::max();
    break;
  case builtin::U64.value:
    minimum = 0;
    maximum = std::numeric_limits<uint64_t>::max();
    break;
  default:
    break;
  }
  if (value < minimum || (value >= 0 && static_cast<uint64_t>(value) > maximum))
    throw TypeError(
        std::format("integer literal `{}` is out of range for type {}", text, interner_.display(type)), span);
}

auto Checker::typeContains(TypeId type, TypeId needle) const -> bool
{
  if (type == needle)
    return true;
  const auto &descriptor = interner_.descriptor(type);
  if (descriptor.kind == TypeKind::Reference || descriptor.kind == TypeKind::RawPointer ||
      descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
      descriptor.kind == TypeKind::DependentArray || descriptor.kind == TypeKind::TypeApplication ||
      descriptor.kind == TypeKind::TypePack || descriptor.kind == TypeKind::Range)
    return typeContains(descriptor.element, needle);
  for (const auto element : descriptor.elements)
    if (typeContains(element, needle))
      return true;
  return false;
}

auto Checker::ensureTraitViewTable(const std::string &traitName, TypeId concreteType) -> bool
{
  if (traitViewTables_.contains(traitName) && traitViewTables_.at(traitName).contains(concreteType.value))
    return true;
  const auto &trait = traits_.at(traitName);
  for (const auto &impl : impls_)
  {
    if (impl.target != concreteType)
      continue;
    std::vector<const TraitInfo *> closure;
    std::unordered_set<std::string> seen;
    collectSupertraits(impl.traitName, closure, seen);
    const bool covers = std::any_of(closure.begin(), closure.end(),
                                    [&](const TraitInfo *candidate) { return candidate->name == traitName; });
    if (!covers)
      continue;
    std::vector<hir::DefId> table;
    for (const auto &methodName : trait.methodOrder)
    {
      hir::DefId methodId;
      if (const auto found = impl.methods.find(methodName); found != impl.methods.end())
        methodId = found->second;
      else if (const auto fallback = trait.methodDefaults.find(methodName);
               fallback != trait.methodDefaults.end())
        methodId = fallback->second;
      else
        return false;
      const auto &methodSignature = signatures_.at(methodId.value);
      if (isGeneric(methodSignature))
      {
        Substitution substitution;
        for (const auto parameter : methodSignature.genericParameters)
          substitution.types.emplace(parameter.value, concreteType);
        methodId = instantiateFunction(methodId, substitution, 0, syntax::SourceSpan{});
      }
      table.push_back(methodId);
    }
    traitViewTables_[traitName].emplace(concreteType.value, std::move(table));
    return true;
  }
  for (const auto &impl : impls_)
  {
    if (!impl.generic)
      continue;
    Substitution substitution;
    if (!matchImplPattern(impl, concreteType, substitution))
      continue;
    std::vector<const TraitInfo *> closure;
    std::unordered_set<std::string> seen;
    collectSupertraits(impl.traitName, closure, seen);
    const bool covers = std::any_of(closure.begin(), closure.end(),
                                    [&](const TraitInfo *candidate) { return candidate->name == traitName; });
    if (!covers)
      continue;
    std::vector<hir::DefId> table;
    for (const auto &methodName : trait.methodOrder)
    {
      hir::DefId methodId;
      if (const auto found = impl.methods.find(methodName); found != impl.methods.end())
        methodId = found->second;
      else if (const auto fallback = trait.methodDefaults.find(methodName);
               fallback != trait.methodDefaults.end())
        methodId = fallback->second;
      else
        return false;
      const auto &methodSignature = signatures_.at(methodId.value);
      if (isGeneric(methodSignature))
      {
        Substitution full = substitution;
        for (const auto parameter : methodSignature.genericParameters)
          if (!full.types.contains(parameter.value))
            full.types.emplace(parameter.value, concreteType);
        methodId = instantiateFunction(methodId, full, 0, syntax::SourceSpan{});
      }
      table.push_back(methodId);
    }
    traitViewTables_[traitName].emplace(concreteType.value, std::move(table));
    return true;
  }
  return false;
}

auto Checker::specificity(TypeId type) const -> int
{
  std::unordered_set<uint32_t> visited;
  return specificityGuarded(type, visited);
}

auto Checker::specificityGuarded(TypeId type, std::unordered_set<uint32_t> &visited) const -> int
{
  if (!visited.insert(type.value).second)
    return 0;
  const auto &descriptor = interner_.descriptor(type);
  if (descriptor.kind == TypeKind::TypeParameter)
    return 0;
  int score = descriptor.kind == TypeKind::Builtin ? 1 : 0;
  for (const auto element : descriptor.elements)
    score += specificityGuarded(element, visited);
  for (const auto argument : descriptor.typeArguments)
    score += specificityGuarded(argument, visited);
  if (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
      descriptor.kind == TypeKind::DependentArray || descriptor.kind == TypeKind::Reference ||
      descriptor.kind == TypeKind::RawPointer)
    score += specificityGuarded(descriptor.element, visited);
  return score;
}

auto Checker::specializeReturnType(const FunctionTypeIds &signature, const Substitution &substitution,
                                    size_t packCount) -> TypeId
{
  const auto &descriptor = interner_.descriptor(signature.returnType);
  if (descriptor.kind != TypeKind::Tuple)
    return specialize(signature.returnType, substitution);
  std::vector<TypeId> elements;
  for (const auto element : descriptor.elements)
  {
    if (interner_.descriptor(element).kind == TypeKind::TypePack)
    {
      const auto foundPack = substitution.packs.find(interner_.descriptor(element).element.value);
      if (foundPack == substitution.packs.end())
        continue;
      elements.insert(elements.end(), foundPack->second.begin(), foundPack->second.end());
    }
    else
    {
      elements.push_back(specialize(element, substitution));
    }
  }
  return interner_.internTuple(elements);
}

auto Checker::instantiateFunction(hir::DefId original, const Substitution &substitution, size_t packCount,
                                   syntax::SourceSpan span,
                                   const std::vector<std::pair<size_t, hir::DefId>> &closureTargets) -> hir::DefId
{
  const auto &source = module_->functions.at(original.value);
  const auto &signature = signatures_.at(original.value);
  if (!isGeneric(signature) && closureTargets.empty())
    return original;
  std::string key = std::format("{}#{}", source.name, original.value);
  std::vector<TypeId> concreteTypes;
  bool concrete = true;
  for (size_t index = 0; index < signature.genericParameters.size(); ++index)
  {
    const TypeId parameter = signature.genericParameters[index];
    const auto found = substitution.types.find(parameter.value);
    const TypeId instantiated = found != substitution.types.end() ? found->second : parameter;
    if (interner_.descriptor(instantiated).kind == TypeKind::TypeParameter)
      concrete = false;
    key += "|" + interner_.display(instantiated);
    concreteTypes.push_back(instantiated);
  }
  for (size_t index = 0; index < signature.constParameters.size(); ++index)
  {
    const auto found = substitution.consts.find(static_cast<uint32_t>(index));
    if (found == substitution.consts.end())
    {
      concrete = false;
      continue;
    }
    const auto &value = interner_.constInterner().value(found->second);
    if (value.kind != const_eval::ConstValueKind::Integer)
      throw TypeError("const generic argument is not an integer", span);
    key += "|" + std::to_string(value.integerValue);
  }
  std::vector<TypeId> concreteConstructors;
  for (size_t index = 0; index < signature.constructorParameters.size(); ++index)
  {
    const TypeId parameter = signature.constructorParameters[index];
    const auto found = substitution.constructors.find(*interner_.descriptor(parameter).nominalId);
    if (found == substitution.constructors.end())
    {
      concrete = false;
      concreteConstructors.push_back(parameter);
      continue;
    }
    key += "|ctor=" + interner_.display(found->second);
    concreteConstructors.push_back(found->second);
  }
  std::vector<TypeId> packElements;
  if (!signature.packParameters.empty())
  {
    const auto packDescriptor = interner_.descriptor(signature.parameters.back());
    const auto foundPack = substitution.packs.find(packDescriptor.element.value);
    if (foundPack == substitution.packs.end())
      concrete = false;
    else
    {
      key += "|pack=" + std::to_string(foundPack->second.size());
      for (const auto element : foundPack->second)
      {
        if (interner_.descriptor(element).kind == TypeKind::TypeParameter)
          concrete = false;
        key += "x" + interner_.display(element);
        packElements.push_back(element);
      }
    }
  }
  const bool variadicParameter = !signature.packParameters.empty();
  for (const auto &[index, defId] : closureTargets)
    key += std::format("|cl{}#{}", index, defId.value);
  if (!concrete)
    return original;
  if (const auto existing = instanceTable_.find(key); existing != instanceTable_.end())
    return existing->second;

  hir::Function clone = hir::cloneFunction(source, nextInstanceLocal_);
  const hir::DefId instanceId{static_cast<uint32_t>(module_->functions.size() + instances_.size())};
  clone.id = instanceId;
  clone.name = source.nativeFunction ? source.name : std::format("{}#{}", source.name, instances_.size());
  FunctionTypeIds instanceSignature;
  instanceSignature.parameters.reserve(signature.parameters.size());
  for (size_t index = 0; index < signature.parameters.size(); ++index)
  {
    if (variadicParameter && index + 1 == signature.parameters.size())
    {
      instanceSignature.parameters.push_back(interner_.internTuple(packElements));
      continue;
    }
    instanceSignature.parameters.push_back(specialize(signature.parameters[index], substitution));
  }
  instanceSignature.genericParameters = std::move(concreteTypes);
  instanceSignature.genericParameterNames = signature.genericParameterNames;
  instanceSignature.constructorParameters = concreteConstructors;
  instanceSignature.constructorParameterNames = signature.constructorParameterNames;
  instanceSignature.constParameters = signature.constParameters;
  instanceSignature.constParameterNames = signature.constParameterNames;
  instanceSignature.returnType = specializeReturnType(signature, substitution, packCount);
  instances_.push_back(std::move(clone));

  // INC-14 S3: closure targets may capture; the specialized instance receives
  // the capture values as hidden leading params (code-function capture groups
  // prepended in closure-target order). Also shift the closure-typed param
  // positions (capture params are prepended before them).
  hir::Function &current = instances_.back();
  unsigned long capturePrefixCount = 0;
  std::vector<std::pair<uint32_t, hir::DefId>> shiftedClosureBindings;
  for (const auto &[index, defId] : closureTargets)
  {
    const auto found = closureCaptureTypesByFunction_.find(defId.value);
    if (found == closureCaptureTypesByFunction_.end())
    {
      // Non-capturing closure: no shift for this target.
      if (index < current.parameters.size())
        shiftedClosureBindings.emplace_back(current.parameters[index + capturePrefixCount].local.value, defId);
      continue;
    }
    std::vector<uint32_t> captureLocalIds;
    captureLocalIds.reserve(found->second.size());
    for (const auto captureType : found->second)
    {
      hir::Parameter captureParameter;
      captureParameter.name = std::format("__instance_cap{}", nextInstanceLocal_);
      captureParameter.local = hir::LocalId{nextInstanceLocal_++};
      captureLocalIds.push_back(captureParameter.local.value);
      current.parameters.insert(current.parameters.begin() + static_cast<long>(capturePrefixCount),
                                std::move(captureParameter));
      instanceSignature.parameters.insert(instanceSignature.parameters.begin() + static_cast<long>(capturePrefixCount),
                                          captureType);
      ++capturePrefixCount;
    }
    // The instance body's closure call (f(...) where f maps to the code
    // function) must prepend these capture params.
    const size_t shiftedIndex = index + capturePrefixCount;
    if (shiftedIndex < current.parameters.size())
    {
      const uint32_t closureParamLocal = current.parameters[shiftedIndex].local.value;
      instanceCaptureParamsByLocal_.emplace(closureParamLocal, captureLocalIds);
      shiftedClosureBindings.emplace_back(closureParamLocal, defId);
    }
  }

  FunctionType displaySignature;
  for (const auto parameter : instanceSignature.parameters)
    displaySignature.parameters.push_back(interner_.display(parameter));
  displaySignature.returnType = interner_.display(instanceSignature.returnType);
  signatures_.emplace(instanceId.value, std::move(instanceSignature));
  functionTypes_.emplace(instanceId.value, std::move(displaySignature));
  instanceTable_.emplace(std::move(key), instanceId);
  const_eval::ConstBindings instanceConstBindings;
  for (size_t index = 0; index < signature.constParameters.size(); ++index)
  {
    const auto found = substitution.consts.find(static_cast<uint32_t>(index));
    if (found != substitution.consts.end())
      instanceConstBindings.emplace(signature.constParameterNames[index], found->second);
  }
  for (const auto &[paramLocal, defId] : shiftedClosureBindings)
    closureLocals_.emplace(paramLocal, defId);
  checkFunction(instances_.back(), instanceConstBindings);
  return instanceId;
}

void Checker::unify(TypeId expected, TypeId actual, Substitution &substitution, syntax::SourceSpan span)
{
  std::unordered_set<uint64_t> visited;
  unifyGuarded(expected, actual, substitution, span, visited);
}

void Checker::unifyGuarded(TypeId expected, TypeId actual, Substitution &substitution, syntax::SourceSpan span,
                            std::unordered_set<uint64_t> &visited)
{
  if (!visited.insert((static_cast<uint64_t>(expected.value) << 32) | actual.value).second)
    return;
  const auto &expectedDescriptor = interner_.descriptor(expected);
  if (expectedDescriptor.kind == TypeKind::TypeParameter)
  {
    if (const auto found = substitution.types.find(expected.value); found != substitution.types.end())
      requireType(found->second, actual, span, "generic argument");
    else
    {
      const auto &bound = interner_.descriptor(actual);
      if (bound.typeArguments.empty() && bound.nominalId.has_value() &&
          ((bound.kind == TypeKind::Struct &&
            interner_.structGenericArity(hir::StructId{*bound.nominalId}) != 0) ||
           (bound.kind == TypeKind::Enum && interner_.enumGenericArity(hir::EnumId{*bound.nominalId}) != 0)))
        throw TypeError(
            std::format("generic argument is a type constructor, not a type: {}", interner_.display(actual)),
            span);
      substitution.types.emplace(expected.value, actual);
    }
    return;
  }
  const auto &actualDescriptor = interner_.descriptor(actual);
  if (expectedDescriptor.kind == TypeKind::TypeApplication)
  {
    const uint32_t constructorIndex = *expectedDescriptor.nominalId;
    if (actualDescriptor.kind == TypeKind::TypeApplication)
    {
      if (*actualDescriptor.nominalId != constructorIndex ||
          expectedDescriptor.elements.size() != actualDescriptor.elements.size())
        throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                    interner_.display(expected), interner_.display(actual)),
                        span);
      for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
        unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span,
                     visited);
      return;
    }
    if (actualDescriptor.kind != TypeKind::Struct && actualDescriptor.kind != TypeKind::Opaque)
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(expected), interner_.display(actual)),
                      span);
    if (!actualDescriptor.nominalId.has_value() ||
        actualDescriptor.typeArguments.size() != expectedDescriptor.elements.size())
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(expected), interner_.display(actual)),
                      span);
    const TypeId templateType = actualDescriptor.kind == TypeKind::Struct
                                    ? interner_.typeForStruct(hir::StructId{*actualDescriptor.nominalId})
                                    : TypeId{*actualDescriptor.nominalId};
    if (const auto bound = substitution.constructors.find(constructorIndex);
        bound != substitution.constructors.end())
    {
      if (bound->second != templateType)
        throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                    interner_.display(expected), interner_.display(actual)),
                        span);
    }
    else
    {
      substitution.constructors.emplace(constructorIndex, templateType);
    }
    for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
      unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.typeArguments[index], substitution, span,
                   visited);
    return;
  }
  if (expected == actual)
    return;
  if (expectedDescriptor.kind == TypeKind::DependentArray || actualDescriptor.kind == TypeKind::DependentArray)
  {
    const auto &dependent =
        expectedDescriptor.kind == TypeKind::DependentArray ? expectedDescriptor : actualDescriptor;
    const auto &concrete =
        expectedDescriptor.kind == TypeKind::DependentArray ? actualDescriptor : expectedDescriptor;
    if (concrete.kind != TypeKind::FixedArray)
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(expected), interner_.display(actual)),
                      span);
    const auto value = interner_.internConstInteger(static_cast<int64_t>(*concrete.length));
    if (const auto bound = substitution.consts.find(*dependent.constParameterIndex);
        bound != substitution.consts.end())
    {
      if (bound->second != value)
        throw TypeError("generic array length mismatch", span);
    }
    else
    {
      substitution.consts.emplace(*dependent.constParameterIndex, value);
    }
    unifyGuarded(expectedDescriptor.element, actualDescriptor.element, substitution, span, visited);
    return;
  }
  if (expectedDescriptor.kind != actualDescriptor.kind)
    throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                interner_.display(expected), interner_.display(actual)),
                    span);
  if (expectedDescriptor.kind == TypeKind::Enum)
  {
    if (expectedDescriptor.nominalId != actualDescriptor.nominalId ||
        expectedDescriptor.elements.size() != actualDescriptor.elements.size())
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(expected), interner_.display(actual)),
                      span);
    for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
      unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span,
                   visited);
    return;
  }
  if (expectedDescriptor.kind == TypeKind::Tuple)
  {
    const bool hasPack =
        std::any_of(expectedDescriptor.elements.begin(), expectedDescriptor.elements.end(),
                    [this](TypeId element) { return interner_.descriptor(element).kind == TypeKind::TypePack; });
    if (!hasPack)
    {
      if (expectedDescriptor.elements.size() != actualDescriptor.elements.size())
        throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                    interner_.display(expected), interner_.display(actual)),
                        span);
      for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
        unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span,
                     visited);
      return;
    }
    if (actualDescriptor.kind != TypeKind::Tuple)
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(expected), interner_.display(actual)),
                      span);
    size_t expectedIndex = 0;
    while (interner_.descriptor(expectedDescriptor.elements[expectedIndex]).kind != TypeKind::TypePack)
      ++expectedIndex;
    const size_t trailing = expectedDescriptor.elements.size() - expectedIndex - 1;
    if (actualDescriptor.elements.size() + 1 < expectedDescriptor.elements.size())
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(expected), interner_.display(actual)),
                      span);
    for (size_t index = 0; index < expectedIndex; ++index)
      unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span,
                   visited);
    const TypeId packElement = interner_.descriptor(expectedDescriptor.elements[expectedIndex]).element;
    const size_t packSize = actualDescriptor.elements.size() - expectedIndex - trailing;
    auto &pack = substitution.packs[packElement.value];
    if (pack.empty())
    {
      for (size_t index = 0; index < packSize; ++index)
        pack.push_back(actualDescriptor.elements[expectedIndex + index]);
    }
    else if (pack.size() != packSize)
    {
      throw TypeError("generic argument type mismatch: expected {}, got {}", span);
    }
    for (size_t index = 0; index < trailing; ++index)
      unifyGuarded(expectedDescriptor.elements[expectedIndex + 1 + index],
                   actualDescriptor.elements[actualDescriptor.elements.size() - trailing + index], substitution,
                   span, visited);
    return;
  }
  if (expectedDescriptor.kind == TypeKind::DynamicArray || expectedDescriptor.kind == TypeKind::FixedArray)
  {
    if (expectedDescriptor.length != actualDescriptor.length)
      throw TypeError("generic array length mismatch", span);
    unifyGuarded(expectedDescriptor.element, actualDescriptor.element, substitution, span, visited);
    return;
  }
  if (expectedDescriptor.kind == TypeKind::Closure)
  {
    if (expectedDescriptor.elements.size() != actualDescriptor.elements.size())
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(expected), interner_.display(actual)),
                      span);
    for (size_t index = 0; index < expectedDescriptor.elements.size(); ++index)
      unifyGuarded(expectedDescriptor.elements[index], actualDescriptor.elements[index], substitution, span, visited);
    unifyGuarded(expectedDescriptor.element, actualDescriptor.element, substitution, span, visited);
    return;
  }
  if (expectedDescriptor.kind == TypeKind::Reference || expectedDescriptor.kind == TypeKind::RawPointer)
  {
    if (expectedDescriptor.referenceMutable != actualDescriptor.referenceMutable)
      throw TypeError("generic reference mutability mismatch", span);
    unifyGuarded(expectedDescriptor.element, actualDescriptor.element, substitution, span, visited);
    return;
  }
  throw TypeError(std::format("generic argument type mismatch: expected {}, got {}", interner_.display(expected),
                              interner_.display(actual)),
                  span);
}

auto Checker::specialize(TypeId type, const Substitution &substitution) -> TypeId
{
  return interner_.specialize(type, substitution.types, substitution.consts, substitution.constructors);
}

void Checker::requireType(TypeId expected, TypeId actual, syntax::SourceSpan span, std::string_view context) const
{
  if (expected != actual)
    throw TypeError(std::format("{} type mismatch: expected {}, got {}", context, interner_.display(expected),
                                interner_.display(actual)),
                    span);
}

} // namespace NG::typecheck::detail
