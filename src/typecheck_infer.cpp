// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck_infer.cpp — expression type inference and expected-type propagation.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

namespace
{
  /// True when a List<T> Cons payload's second field is the recursive tail:
  /// either `ref<List<T>>` (legacy representation) or `Box<List<T>>`
  /// (INC-20c representation, `Box.handle` is the recursive target).
  [[nodiscard]] auto isListTail(const TypeDescriptor &field, TypeId expected, const TypeInterner &interner) -> bool
  {
    if (field.kind == TypeKind::Reference && field.element == expected)
      return true;
    return field.kind == TypeKind::Struct && field.name == "Box" && field.typeArguments.size() == 1 &&
           field.typeArguments[0] == expected;
  }
} // namespace

auto Checker::analyzeCallSpread(const hir::Expression &expression, size_t firstArgument,
                                const LocalTypes &locals, CallSpreadInfo &info) -> bool
{
  bool anySpread = false;
  for (size_t index = firstArgument; index < expression.operands.size(); ++index)
  {
    const auto &argument = *expression.operands[index];
    if (argument.kind == hir::ExpressionKind::Prefix && argument.text == "...")
    {
      const TypeId operand = infer(*argument.operands[0], locals);
      const auto &descriptor = interner_.descriptor(operand);
      if (descriptor.kind != TypeKind::Tuple)
        throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)), argument.span);
      info.spreadPositions.push_back(index - firstArgument);
      info.expandedTypes.insert(info.expandedTypes.end(), descriptor.elements.begin(), descriptor.elements.end());
      for (const auto element : descriptor.elements)
      {
        static_cast<void>(element);
        info.spreadSlot.push_back(true);
        info.argumentExpressions.push_back(argument.operands[0].get());
      }
      anySpread = true;
    }
    else
    {
      // INC-14 S2c: closure literal arguments cannot be inferred standalone;
      // the call checks them against the closure parameter in a deferred pass.
      // Their expandedTypes slot stays unused unless the same call spreads
      // elsewhere, which higher-order calls do not.
      info.expandedTypes.push_back(argument.kind == hir::ExpressionKind::Closure ? TypeId{}
                                                                                : infer(argument, locals));
      info.spreadSlot.push_back(false);
      info.argumentExpressions.push_back(&argument);
    }
  }
  info.expandedCount = info.expandedTypes.size();
  return anySpread;
}

auto Checker::analyzeFoldCall(const hir::Expression &expression, const LocalTypes &locals) -> FoldInfo
{
  FoldInfo info;
  if (expression.operands.size() < 2)
    return info;
  for (size_t index = 1; index < expression.operands.size(); ++index)
  {
    const auto &argument = *expression.operands[index];
    if (argument.kind != hir::ExpressionKind::Prefix || argument.text != "...")
      continue;
    const TypeId operand = infer(*argument.operands[0], locals);
    const auto &descriptor = interner_.descriptor(operand);
    if (descriptor.kind != TypeKind::DynamicArray && descriptor.kind != TypeKind::FixedArray &&
        descriptor.kind != TypeKind::DependentArray && descriptor.kind != TypeKind::Range)
      continue;
    if (descriptor.kind == TypeKind::Range && descriptor.element != builtin::I64)
      throw TypeError("fold over ranges currently requires i64 elements", argument.span);
    info.fold = true;
    info.spreadPosition = index - 1;
    info.elementType = descriptor.element;
    break;
  }
  if (info.fold && expression.operands.size() == 3)
    info.accumulatorPosition = 1 - info.spreadPosition;
  return info;
}

auto Checker::explicitSubstitution(const hir::Expression &expression, const FunctionTypeIds &signature) -> Substitution
{
  Substitution substitution;
  const size_t expected = signature.explicitParameterOrder.size();
  if (expression.genericArguments.size() != expected)
    throw TypeError(std::format("generic argument count mismatch: expected {}, got {}", expected,
                                expression.genericArguments.size()),
                    expression.span);
  size_t typeIndex{};
  size_t constructorIndex{};
  size_t constIndex{};
  for (size_t index = 0; index < expression.genericArguments.size(); ++index)
  {
    const auto &argument = expression.genericArguments[index];
    switch (signature.explicitParameterOrder[index])
    {
    case syntax::GenericParameterKind::Type:
    {
      if (argument.kind != syntax::GenericArgumentKind::Type)
        throw TypeError(std::format("generic argument {} must be a type", index + 1), argument.span);
      const TypeId resolved = interner_.resolveInScope(*argument.type, genericBindings_);
      const auto &descriptor = interner_.descriptor(resolved);
      if (descriptor.typeArguments.empty() && descriptor.nominalId.has_value() &&
          ((descriptor.kind == TypeKind::Struct &&
            interner_.structGenericArity(hir::StructId{*descriptor.nominalId}) != 0) ||
           (descriptor.kind == TypeKind::Enum &&
            interner_.enumGenericArity(hir::EnumId{*descriptor.nominalId}) != 0)))
        throw TypeError(std::format("generic argument {} is a type constructor, not a type", index + 1),
                        argument.span);
      substitution.types.emplace(signature.genericParameters[typeIndex].value, resolved);
      ++typeIndex;
      break;
    }
    case syntax::GenericParameterKind::TypeConstructor:
    case syntax::GenericParameterKind::VariadicTypeConstructor:
    {
      if (argument.kind != syntax::GenericArgumentKind::Type)
        throw TypeError(std::format("generic argument {} must be a type constructor", index + 1), argument.span);
      const TypeId templateType = interner_.templateForName(argument.type->name, argument.span);
      const auto &descriptor = interner_.descriptor(templateType);
      if ((descriptor.kind != TypeKind::Struct && descriptor.kind != TypeKind::Opaque) ||
          !descriptor.nominalId.has_value())
        throw TypeError(std::format("generic argument {} must be a struct or opaque type constructor", index + 1),
                        argument.span);
      substitution.constructors.emplace(
          *interner_.descriptor(signature.constructorParameters[constructorIndex]).nominalId, templateType);
      ++constructorIndex;
      break;
    }
    case syntax::GenericParameterKind::Const:
    {
      if (argument.kind != syntax::GenericArgumentKind::ConstExpr)
        throw TypeError(std::format("generic argument {} must be a const expression", index + 1), argument.span);
      const const_eval::ConstValueId value =
          const_eval::ConstEvaluator{interner_.constInterner()}.evaluate(*argument.constExpr, {});
      substitution.consts.emplace(static_cast<uint32_t>(constIndex), value);
      ++constIndex;
      break;
    }
    case syntax::GenericParameterKind::Pack:
      break;
    }
  }
  return substitution;
}

auto Checker::inferExpectedEnum(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
    -> TypeId
{
  const auto &descriptor = interner_.descriptor(expected);
  if (!expression.enumId.has_value() || descriptor.nominalId != expression.enumId->value)
    throw TypeError(std::format("enum constructor type mismatch: expected {}, got {}",
                                interner_.display(expected), expression.text),
                    expression.span);
  const uint32_t variant = expression.variant.value();
  if (variant >= descriptor.elements.size())
    throw TypeError("enum variant is out of range", expression.span);
  if (descriptor.variantHasPayload[variant] &&
      interner_.descriptor(descriptor.elements[variant]).kind == TypeKind::Tuple)
  {
    const auto &tuple = interner_.descriptor(descriptor.elements[variant]);
    if (expression.operands.size() != tuple.elements.size())
      throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}",
                                  descriptor.fieldNames[variant], tuple.elements.size(),
                                  expression.operands.size()),
                      expression.span);
    for (size_t index = 0; index < expression.operands.size(); ++index)
    {
      static_cast<void>(
          inferExpected(*expression.operands[index], tuple.elements[index], locals, "variant payload"));
      trackConsumption(*expression.operands[index], locals);
    }
    record(expression, expected);
    return expected;
  }
  const size_t wanted = descriptor.variantHasPayload[variant] ? 1 : 0;
  if (expression.operands.size() != wanted)
    throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}",
                                descriptor.fieldNames[variant], wanted, expression.operands.size()),
                    expression.span);
  if (wanted == 1)
  {
    static_cast<void>(
        inferExpected(*expression.operands[0], descriptor.elements[variant], locals, "variant payload"));
    trackConsumption(*expression.operands[0], locals);
  }
  record(expression, expected);
  return expected;
}

auto Checker::inferExpectedStruct(const hir::Expression &expression, TypeId expected,
                                   const LocalTypes &locals) -> TypeId
{
  const auto &descriptor = interner_.descriptor(expected);
  if (!expression.structId.has_value() || descriptor.nominalId != expression.structId->value)
    throw TypeError(std::format("struct literal type mismatch: expected {}, got {}", interner_.display(expected),
                                expression.text),
                    expression.span);
  std::vector<bool> seen(descriptor.fieldNames.size());
  for (size_t index = 0; index < expression.operands.size(); ++index)
  {
    const auto found =
        std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.memberNames[index]);
    if (found == descriptor.fieldNames.end())
      throw TypeError(
          std::format("unknown field `{}` in struct `{}`", expression.memberNames[index], descriptor.name),
          expression.span);
    const size_t field = static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found));
    if (seen[field])
      throw TypeError(std::format("duplicate field `{}` in struct literal", expression.memberNames[index]),
                      expression.span);
    seen[field] = true;
    const TypeId fieldType = inferExpected(*expression.operands[index], descriptor.elements[field], locals,
                                           std::format("field `{}`", expression.memberNames[index]));
    trackConsumption(*expression.operands[index], locals);
    if (interner_.descriptor(fieldType).kind == TypeKind::Reference)
      throw TypeError("references cannot be stored in struct fields", expression.operands[index]->span);
  }
  if (std::find(seen.begin(), seen.end(), false) != seen.end())
    throw TypeError(std::format("missing field in struct `{}`", descriptor.name), expression.span);
  record(expression, expected);
  return expected;
}

auto Checker::inferMethodCall(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
{
  const auto &callee = *expression.operands[0];
  const auto &receiverNode = *callee.operands[0];
  const bool qualified = receiverNode.kind == hir::ExpressionKind::ResolvedName &&
                          !receiverNode.resolvedName.has_value() && traits_.contains(receiverNode.text);
  if (receiverNode.kind == hir::ExpressionKind::ResolvedName && !receiverNode.resolvedName.has_value() &&
      !traits_.contains(receiverNode.text))
    throw TypeError(std::format("unknown trait `{}` in qualified call", receiverNode.text), receiverNode.span);
  const hir::Expression &receiver = qualified ? *expression.operands[1] : receiverNode;
  const size_t firstArgument = qualified ? 2 : 1;
  const TypeId receiverType = infer(receiver, locals);
  TypeId dispatchType = receiverType;
  bool passReferenceThrough = false;
  if (interner_.descriptor(receiverType).kind == TypeKind::Reference)
  {
    dispatchType = interner_.descriptor(receiverType).element;
    passReferenceThrough = true;
  }
  if (interner_.descriptor(dispatchType).kind == TypeKind::TypeParameter)
  {
    const std::string parameterName = interner_.descriptor(dispatchType).name;
    const hir::Function *current = currentFunctionId_.value < module_->functions.size()
                                        ? &module_->functions.at(currentFunctionId_.value)
                                        : nullptr;
    const TraitInfo *owning = nullptr;
    if (current != nullptr)
    {
      for (const auto &[boundParameter, boundTraits] : current->traitBounds)
      {
        if (boundParameter != parameterName)
          continue;
        for (const auto &traitName : boundTraits)
        {
          const auto found = traits_.find(traitName);
          if (found != traits_.end() && found->second.methodParameters.contains(expression.text))
          {
            owning = &found->second;
            break;
          }
        }
        if (owning != nullptr)
          break;
      }
    }
    if (owning == nullptr)
    {
      const auto owner = defaultMethodTraits_.find(currentFunctionId_.value);
      if (owner != defaultMethodTraits_.end())
      {
        const auto traitIt = traits_.find(owner->second);
        if (traitIt != traits_.end() && traitIt->second.methodParameters.contains(expression.text))
          owning = &traitIt->second;
      }
    }
    if (owning == nullptr)
      throw TypeError(std::format("no trait bound provides method `{}` for type parameter `{}`", expression.text,
                                  parameterName),
                      expression.span);
    deferredMethodFunctions_.insert(currentFunctionId_.value);
    const TypeId returnType = interner_.specialize(owning->methodReturns.at(expression.text),
                                                    {{owning->selfParameter.value, dispatchType}});
    record(expression, returnType);
    return returnType;
  }
  if (interner_.descriptor(dispatchType).kind == TypeKind::TraitReference)
  {
    const std::string traitName = interner_.descriptor(dispatchType).name;
    const auto &trait = traits_.at(traitName);
    const auto foundMethod = std::find(trait.methodOrder.begin(), trait.methodOrder.end(), expression.text);
    if (foundMethod == trait.methodOrder.end())
      throw TypeError(std::format("trait `{}` has no method `{}`", traitName, expression.text), expression.span);
    const size_t methodIndex = static_cast<size_t>(std::distance(trait.methodOrder.begin(), foundMethod));
    const TypeId returnType = trait.methodReturns.at(expression.text);
    if (typeContains(returnType, trait.selfParameter))
      throw TypeError("trait object methods with Self in the return type are not yet supported", expression.span);
    const auto &parameters = trait.methodParameters.at(expression.text);
    const size_t supplied = expression.operands.size() - firstArgument;
    if (supplied + 1 != parameters.size())
      throw TypeError(
          std::format("method argument count mismatch: expected {}, got {}", parameters.size() - 1, supplied),
          expression.span);
    for (size_t index = 0; index < supplied; ++index)
    {
      const TypeId parameter = parameters[index + 1];
      if (typeContains(parameter, trait.selfParameter))
        throw TypeError("trait object method arguments typed with Self are not yet supported", expression.span);
      static_cast<void>(inferExpected(*expression.operands[firstArgument + index], parameter, locals,
                                      std::format("method argument {}", index + 1)));
    }
    traitViewCalls_.insert_or_assign(&expression, std::pair<std::string, size_t>{traitName, methodIndex});
    record(expression, returnType);
    return returnType;
  }
  if (expression.text == "clone" && isBox(dispatchType))
  {
    if (expression.operands.size() != firstArgument)
      throw TypeError("derived clone takes no arguments", expression.span);
    methodReceiverMutable_.insert_or_assign(&expression, false);
    methodReceiverRefTypes_.insert_or_assign(&expression, interner_.internReference(dispatchType, false));
    derivedCloneCalls_.insert_or_assign(&expression, dispatchType);
    boxCloneCalls_.insert_or_assign(&expression, dispatchType);
    record(expression, dispatchType);
    return dispatchType;
  }
  if (expression.text == "clone" && derivedCloneTypes_.contains(dispatchType.value))
  {
    if (expression.operands.size() != firstArgument)
      throw TypeError("derived clone takes no arguments", expression.span);
    methodReceiverMutable_.insert_or_assign(&expression, false);
    methodReceiverRefTypes_.insert_or_assign(&expression, interner_.internReference(dispatchType, false));
    derivedCloneCalls_.insert_or_assign(&expression, dispatchType);
    record(expression, dispatchType);
    return dispatchType;
  }
  if (expression.text == "show" && derivedShowTypes_.contains(dispatchType.value))
  {
    if (expression.operands.size() != firstArgument)
      throw TypeError("derived show takes no arguments", expression.span);
    methodReceiverMutable_.insert_or_assign(&expression, false);
    methodReceiverRefTypes_.insert_or_assign(&expression, interner_.internReference(dispatchType, false));
    derivedShowCalls_.insert_or_assign(&expression, dispatchType);
    record(expression, builtin::String);
    return builtin::String;
  }
  hir::DefId selected;
  bool selectedFound = false;
  Substitution matchedSubstitution;
  for (const auto &impl : impls_)
  {
    if (impl.generic || impl.target != dispatchType)
      continue;
    if (const auto found = impl.methods.find(expression.text); found != impl.methods.end())
    {
      selected = found->second;
      selectedFound = true;
      break;
    }
  }
  if (!selectedFound)
  {
    for (const auto &impl : impls_)
    {
      if (!impl.generic)
        continue;
      Substitution substitution;
      if (!matchImplPattern(impl, dispatchType, substitution))
        continue;
      if (const auto found = impl.methods.find(expression.text); found != impl.methods.end())
      {
        selected = found->second;
        matchedSubstitution = std::move(substitution);
        selectedFound = true;
        break;
      }
    }
  }
  if (!selectedFound)
    throw TypeError(
        std::format("no method `{}` for value of type {}", expression.text, interner_.display(dispatchType)),
        expression.span);
  const auto &signature = signatures_.at(selected.value);
  hir::DefId target = selected;
  if (isGeneric(signature))
  {
    Substitution substitution = matchedSubstitution;
    for (const auto parameter : signature.genericParameters)
      if (!substitution.types.contains(parameter.value))
        substitution.types.emplace(parameter.value, dispatchType);
    target = instantiateFunction(selected, substitution, 0, expression.span);
  }
  const auto &effectiveSignature = signatures_.at(target.value);
  const size_t supplied = expression.operands.size() - firstArgument;
  if (supplied != effectiveSignature.parameters.size() - 1)
    throw TypeError(std::format("method argument count mismatch: expected {}, got {}",
                                effectiveSignature.parameters.size() - 1, supplied),
                    expression.span);
  const TypeId receiverParameter = effectiveSignature.parameters.front();
  const auto &receiverDescriptor = interner_.descriptor(receiverParameter);
  if (passReferenceThrough)
  {
    requireType(receiverParameter, receiverType, receiver.span, "method receiver");
  }
  else
  {
    requireType(receiverDescriptor.element, receiverType, receiver.span, "method receiver");
    if (receiverDescriptor.referenceMutable)
      requireMutableRoot(receiver, receiver.span);
  }
  for (size_t index = 0; index < supplied; ++index)
  {
    const auto &parameterDescriptor = interner_.descriptor(effectiveSignature.parameters[index + 1]);
    if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer)
      trackConsumption(*expression.operands[firstArgument + index], locals);
  }
  methodReceiverMutable_.insert_or_assign(&expression, receiverDescriptor.referenceMutable);
  methodReceiverRefTypes_.insert_or_assign(&expression, receiverParameter);
  for (size_t index = 0; index < supplied; ++index)
    static_cast<void>(inferExpected(*expression.operands[firstArgument + index],
                                    effectiveSignature.parameters[index + 1], locals,
                                    std::format("method argument {}", index + 1)));
  const TypeId returnType = effectiveSignature.returnType;
  record(expression, returnType);
  callTargets_.insert_or_assign(&expression, target);
  return returnType;
}

auto Checker::inferExpected(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                             std::string_view context) -> TypeId
{
  const auto &descriptor = interner_.descriptor(expected);
  if (descriptor.kind != TypeKind::Union && expression.kind == hir::ExpressionKind::ResolvedName)
  {
    const TypeId valueType = infer(expression, locals);
    if (interner_.descriptor(valueType).kind == TypeKind::Union &&
        std::find(interner_.descriptor(valueType).elements.begin(),
                  interner_.descriptor(valueType).elements.end(),
                  expected) != interner_.descriptor(valueType).elements.end())
    {
      record(expression, expected);
      return expected;
    }
  }
  if (expression.kind == hir::ExpressionKind::IntegerLiteral && isIntegerBuiltin(expected))
  {
    if (!expression.numericSuffix.empty())
    {
      const TypeId suffixType = typeFromNumericSuffix(expression.numericSuffix);
      if (suffixType != expected)
        throw TypeError(std::format("integer literal suffix `{}` conflicts with expected type {}",
                                    expression.numericSuffix, interner_.display(expected)),
                        expression.span);
    }
    checkIntegerLiteralMagnitude(parseIntegerLiteralMagnitude(expression.text, expected, expression.span),
                                 expression.text, expected, expression.span);
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::FloatLiteral && isFloatBuiltin(expected))
  {
    if (!expression.numericSuffix.empty() && typeFromNumericSuffix(expression.numericSuffix) != expected)
      throw TypeError(std::format("float literal suffix `{}` conflicts with expected type {}",
                                  expression.numericSuffix, interner_.display(expected)),
                      expression.span);
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::IntegerLiteral && isFloatBuiltin(expected) &&
      expression.numericSuffix.empty())
  {
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::ArrayLiteral && descriptor.kind == TypeKind::Enum)
  {
    std::optional<uint32_t> consVariant;
    std::optional<uint32_t> nilVariant;
    TypeId element{};
    for (uint32_t variant = 0; variant < descriptor.elements.size(); ++variant)
    {
      if (!descriptor.variantHasPayload[variant])
      {
        if (!nilVariant.has_value())
          nilVariant = variant;
        continue;
      }
      const auto &payloadDescriptor = interner_.descriptor(descriptor.elements[variant]);
      if (payloadDescriptor.kind == TypeKind::Tuple && payloadDescriptor.elements.size() == 2 &&
          isListTail(interner_.descriptor(payloadDescriptor.elements[1]), expected, interner_))
      {
        consVariant = variant;
        element = payloadDescriptor.elements[0];
      }
    }
    if (consVariant.has_value() && nilVariant.has_value())
    {
      for (const auto &candidate : expression.operands)
      {
        if (candidate->kind == hir::ExpressionKind::Prefix && candidate->text == "...")
          throw TypeError("list collection literals do not support spreads", candidate->span);
        static_cast<void>(inferExpected(*candidate, element, locals, "list element"));
      }
      record(expression, expected);
      return expected;
    }
  }
  if (expression.kind == hir::ExpressionKind::Prefix && (expression.text == "-" || expression.text == "+") &&
      isIntegerBuiltin(expected) && expression.operands[0]->kind == hir::ExpressionKind::IntegerLiteral)
  {
    const std::string text = std::format("{}{}", expression.text, expression.operands[0]->text);
    if (!expression.operands[0]->numericSuffix.empty())
    {
      const TypeId suffixType = typeFromNumericSuffix(expression.operands[0]->numericSuffix);
      if (suffixType != expected)
        throw TypeError(std::format("integer literal suffix `{}` conflicts with expected type {}",
                                    expression.operands[0]->numericSuffix, interner_.display(expected)),
                        expression.span);
    }
    record(*expression.operands[0], expected);
    const uint64_t magnitude =
        parseIntegerLiteralMagnitude(expression.operands[0]->text, expected, expression.span);
    if (expression.text == "-")
    {
      if (!isSignedIntegerBuiltin(expected))
        throw TypeError(std::format("integer literal `-{}` is out of range for type {}",
                                    expression.operands[0]->text, interner_.display(expected)),
                        expression.span);
      constexpr uint64_t MaxMagnitude = 1ULL << 63;
      if (magnitude > MaxMagnitude)
        throw TypeError(
            std::format("integer literal `{}` is out of range for type {}", text, interner_.display(expected)),
            expression.span);
      checkIntegerLiteral(magnitude == MaxMagnitude ? std::numeric_limits<int64_t>::min()
                                                    : -static_cast<int64_t>(magnitude),
                          text, expected, expression.span);
    }
    else
    {
      checkIntegerLiteralMagnitude(magnitude, text, expected, expression.span);
    }
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::Prefix && (expression.text == "-" || expression.text == "+") &&
      isFloatBuiltin(expected) && expression.operands[0]->kind == hir::ExpressionKind::FloatLiteral)
  {
    static_cast<void>(inferExpected(*expression.operands[0], expected, locals, "float literal"));
    record(expression, expected);
    return expected;
  }
  if (descriptor.kind == TypeKind::Union)
  {
    const TypeId valueType = infer(expression, locals);
    if (valueType == expected)
    {
      record(expression, expected);
      return expected;
    }
    const auto member = std::find(descriptor.elements.begin(), descriptor.elements.end(), valueType);
    if (member == descriptor.elements.end())
      throw TypeError(std::format("value of type {} is not a member of union {}", interner_.display(valueType),
                                  interner_.display(expected)),
                      expression.span);
    record(expression, expected);
    return expected;
  }
  if (descriptor.kind == TypeKind::TraitReference)
  {
    const std::string traitName = descriptor.name;
    const TypeId valueType = infer(expression, locals);
    TypeId concrete = valueType;
    if (interner_.descriptor(valueType).kind == TypeKind::TraitReference)
    {
      requireType(expected, valueType, expression.span, context);
      record(expression, expected);
      return expected;
    }
    if (interner_.descriptor(valueType).kind == TypeKind::Reference)
      concrete = interner_.descriptor(valueType).element;
    if (interner_.descriptor(concrete).kind == TypeKind::TraitReference ||
        interner_.descriptor(concrete).kind == TypeKind::Trait)
      throw TypeError(std::format("cannot build a `{}` view from {}", interner_.display(expected),
                                  interner_.display(valueType)),
                      expression.span);
    if (!hasImpl(traitName, concrete) || !ensureTraitViewTable(traitName, concrete))
      throw TypeError(
          std::format("value of type {} does not implement trait `{}`", interner_.display(concrete), traitName),
          expression.span);
    traitViewCoercions_.insert_or_assign(&expression, std::pair<std::string, TypeId>{traitName, concrete});
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::Binary && expression.text == ".." &&
      descriptor.kind == TypeKind::Range)
  {
    static_cast<void>(inferExpected(*expression.operands[0], descriptor.element, locals, "range start"));
    static_cast<void>(inferExpected(*expression.operands[1], descriptor.element, locals, "range end"));
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::ArrayLiteral &&
      (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray ||
       descriptor.kind == TypeKind::DependentArray))
  {
    if (descriptor.kind == TypeKind::FixedArray && expression.operands.size() != *descriptor.length)
      throw TypeError(std::format("fixed array length mismatch: expected {}, got {}", *descriptor.length,
                                  expression.operands.size()),
                      expression.span);
    for (const auto &element : expression.operands)
      static_cast<void>(inferExpected(*element, descriptor.element, locals, "array element"));
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::EnumLiteral && descriptor.kind == TypeKind::Enum)
    return inferExpectedEnum(expression, expected, locals);
  if (expression.kind == hir::ExpressionKind::Call && !expression.operands.empty() &&
      expression.operands[0]->resolvedName.has_value() &&
      expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function)
  {
    hir::DefId selected{expression.operands[0]->resolvedName->id};
    if (expression.operands[0]->functionCandidates.size() > 1)
    {
      int bestScore{-1};
      for (const auto candidate : expression.operands[0]->functionCandidates)
      {
        const auto &candidateSignature = signatures_.at(candidate.value);
        if (candidateSignature.parameters.size() != expression.operands.size() - 1)
          continue;
        if (!isGeneric(candidateSignature))
        {
          try
          {
            for (size_t index = 0; index < candidateSignature.parameters.size(); ++index)
              requireType(candidateSignature.parameters[index],
                          infer(*expression.operands[index + 1], locals),
                          expression.operands[index + 1]->span, "specialization argument");
          }
          catch (const TypeError &)
          {
            continue;
          }
        }
        int score = isGeneric(candidateSignature) ? 0 : 100;
        for (const auto parameter : candidateSignature.parameters)
          score += specificity(parameter);
        if (score > bestScore)
        {
          bestScore = score;
          selected = candidate;
        }
        else if (score == bestScore && candidate != selected)
        {
          throw TypeError("ambiguous function specialization", expression.span);
        }
      }
    }
    const auto &signature = signatures_.at(selected.value);
    {
      const FoldInfo fold = analyzeFoldCall(expression, locals);
      if (fold.fold)
      {
        if (expression.operands.size() != 3)
          throw TypeError("fold calls require exactly one accumulator argument", expression.span);
        if (signature.parameters.size() != 2)
          throw TypeError("fold function must take exactly two arguments", expression.span);
        Substitution substitution;
        unify(signature.parameters[fold.spreadPosition], fold.elementType, substitution, expression.span);
        const TypeId accumulatorType = infer(*expression.operands[fold.accumulatorPosition + 1], locals);
        unify(signature.parameters[fold.accumulatorPosition], accumulatorType, substitution, expression.span);
        const TypeId result = specializeReturnType(signature, substitution, 0);
        unify(signature.parameters[fold.accumulatorPosition], result, substitution, expression.span);
        requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
        requireType(expected, result, expression.span, context);
        callFoldSpreadPositions_.insert_or_assign(&expression, std::vector<size_t>{fold.spreadPosition});
        callFoldAccumulatorPositions_.insert_or_assign(&expression,
                                                        std::vector<size_t>{fold.accumulatorPosition});
        callTargets_.insert_or_assign(&expression, selected);
        record(expression, result);
        return result;
      }
    }
    if (isGeneric(signature))
    {
      Substitution substitution;
      const size_t supplied = expression.operands.size() - 1;
      const bool variadic = !signature.packParameters.empty();
      const size_t fixedParameters = signature.parameters.size() - (variadic ? 1 : 0);
      if (variadic ? supplied < fixedParameters : supplied != signature.parameters.size())
        throw TypeError(std::format("call argument count mismatch: expected {}, got {}",
                                    signature.parameters.size(), supplied),
                        expression.span);
      const size_t packCount = variadic ? supplied - fixedParameters : 0;
      std::vector<std::pair<size_t, hir::DefId>> closureTargets;
      if (!expression.genericArguments.empty())
      {
        substitution = explicitSubstitution(expression, signature);
        for (size_t index = 0; index < supplied; ++index)
        {
          const TypeId parameterType = index < fixedParameters
                                          ? signature.parameters[index]
                                          : interner_.descriptor(signature.parameters.back()).element;
          static_cast<void>(inferExpected(*expression.operands[index + 1],
                                          specialize(parameterType, substitution), locals,
                                          std::format("call argument {}", index + 1)));
        }
      }
      else
      {
        unify(signature.returnType, expected, substitution, expression.span);
        for (size_t index = 0; index < supplied; ++index)
        {
          const auto &argument = *expression.operands[index + 1];
          const TypeId parameterType = index < fixedParameters
                                          ? signature.parameters[index]
                                          : interner_.descriptor(signature.parameters.back()).element;
          if (argument.kind == hir::ExpressionKind::Closure && index < fixedParameters)
          {
            static_cast<void>(inferCallClosureArgument(argument, parameterType, index, locals, substitution,
                                                       closureTargets));
            // INC-14 S3/S4: capture values of a literal passed to a
            // higher-order call thread into the specialized instance as
            // leading args (recorded once, before the call target commits).
            if (callTargets_.find(&expression) == callTargets_.end() && !argument.closureCaptures.empty())
            {
              std::vector<uint32_t> captureIds;
              for (const auto &cap : argument.closureCaptures)
                captureIds.push_back(cap.local.value);
              auto &callCaptureList = callCaptures_[&expression];
              callCaptureList.insert(callCaptureList.end(), captureIds.begin(), captureIds.end());
            }
            continue;
          }
          // INC-14 S3 follow-up / S4: a closure-typed local or bound closure
          // parameter forwarded into a higher-order call records a closure
          // target so the callee specializes with the forwarding closure bound.
          if (index < fixedParameters &&
              interner_.descriptor(parameterType).kind == TypeKind::Closure &&
              argument.resolvedName.has_value() &&
              argument.resolvedName->kind == hir::ResolvedNameKind::Local)
          {
            const auto bound = closureLocals_.find(argument.resolvedName->id);
            if (bound != closureLocals_.end() &&
                interner_.descriptor(infer(argument, locals)).kind == TypeKind::Closure)
            {
              closureTargets.emplace_back(index, bound->second);
              if (callTargets_.find(&expression) == callTargets_.end())
              {
                const auto instanceParams = instanceCaptureParamsByLocal_.find(argument.resolvedName->id);
                auto &callCaptureList = callCaptures_[&expression];
                if (instanceParams != instanceCaptureParamsByLocal_.end())
                  callCaptureList.insert(callCaptureList.end(), instanceParams->second.begin(),
                                         instanceParams->second.end());
                else
                {
                  const auto foundCaptures = closureCapturesByLocal_.find(argument.resolvedName->id);
                  if (foundCaptures != closureCapturesByLocal_.end())
                  {
                    for (const auto &cap : foundCaptures->second)
                      callCaptureList.push_back(cap.localId);
                  }
                }
              }
              continue;
            }
          }
          if (variadic && index >= fixedParameters)
          {
            const TypeId argumentType = infer(argument, locals);
            auto &pack = substitution.packs[parameterType.value];
            const size_t packPosition = index - fixedParameters;
            if (packPosition < pack.size())
            {
              if (pack[packPosition] != argumentType)
                throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                            interner_.display(pack[packPosition]),
                                            interner_.display(argumentType)),
                                argument.span);
            }
            else
            {
              pack.push_back(argumentType);
            }
            continue;
          }
          const auto &parameterDescriptor = interner_.descriptor(parameterType);
          if (argument.kind == hir::ExpressionKind::EnumLiteral && parameterDescriptor.kind == TypeKind::Enum)
          {
            const uint32_t variant = argument.variant.value();
            if (argument.operands.size() != (parameterDescriptor.variantHasPayload[variant] ? 1u : 0u))
              throw TypeError("generic enum constructor payload arity mismatch", argument.span);
            if (!argument.operands.empty())
              unify(parameterDescriptor.elements[variant], infer(*argument.operands.front(), locals),
                    substitution, argument.operands.front()->span);
          }
          else
          {
            unify(parameterType, infer(argument, locals), substitution, argument.span);
          }
        }
      }
      const TypeId specialized = specializeReturnType(signature, substitution, packCount);
      requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
      bool substitutionConcrete = true;
      for (const auto &[parameter, argument] : substitution.types)
        if (interner_.descriptor(argument).kind == TypeKind::TypeParameter)
          substitutionConcrete = false;
      if (substitutionConcrete && module_ != nullptr && selected.value < module_->functions.size() &&
          module_->functions.at(selected.value).whereClause != nullptr &&
          !evaluateWhereCondition(*module_->functions.at(selected.value).whereClause, substitution, signature))
        throw TypeError(std::format("call to `{}` does not satisfy its where clause",
                                    module_->functions.at(selected.value).name),
                        expression.span);
      const bool externTarget =
          selected.value < module_->functions.size() && module_->functions.at(selected.value).externC;
      for (size_t index = 0; index < supplied; ++index)
      {
        const TypeId parameterType = index < fixedParameters
                                        ? signature.parameters[index]
                                        : interner_.descriptor(signature.parameters.back()).element;
        const auto &parameterDescriptor = interner_.descriptor(parameterType);
        if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer &&
            !externTarget)
          trackConsumption(*expression.operands[index + 1], locals);
      }
      if (variadic)
      {
        callPackArgCounts_.insert_or_assign(&expression, packCount);
        std::vector<TypeId> packedTypes;
        for (size_t index = fixedParameters; index < supplied; ++index)
          packedTypes.push_back(infer(*expression.operands[index + 1], locals));
        callPackTupleTypes_.insert_or_assign(&expression, interner_.internTuple(std::move(packedTypes)));
      }
      selected = instantiateFunction(selected, substitution, packCount, expression.span, closureTargets);
      requireType(expected, specialized, expression.span, context);
      record(expression, specialized);
      callTargets_.insert_or_assign(&expression, selected);
      return specialized;
    }
  }
  if (expression.kind == hir::ExpressionKind::StructLiteral && descriptor.kind == TypeKind::Struct)
  {
    return inferExpectedStruct(expression, expected, locals);
  }
  if (expression.kind == hir::ExpressionKind::TupleLiteral && descriptor.kind == TypeKind::Tuple)
  {
    size_t expectedIndex = 0;
    for (const auto &element : expression.operands)
    {
      if (element->kind == hir::ExpressionKind::Prefix && element->text == "...")
      {
        const TypeId operand = infer(*element->operands[0], locals);
        const auto &operandDescriptor = interner_.descriptor(operand);
        if (expectedIndex < descriptor.elements.size() &&
            interner_.descriptor(descriptor.elements[expectedIndex]).kind == TypeKind::TypePack)
        {
          ++expectedIndex;
          continue;
        }
        if (operandDescriptor.kind == TypeKind::TypePack)
        {
          expectedIndex = descriptor.elements.size();
          continue;
        }
        if (operandDescriptor.kind != TypeKind::Tuple)
          throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)),
                          element->span);
        if (expectedIndex + operandDescriptor.elements.size() > descriptor.elements.size())
          throw TypeError("tuple splice exceeds the expected tuple length", element->span);
        for (size_t index = 0; index < operandDescriptor.elements.size(); ++index)
          requireType(descriptor.elements[expectedIndex + index], operandDescriptor.elements[index],
                      element->span, "tuple splice element");
        expectedIndex += operandDescriptor.elements.size();
        continue;
      }
      if (expectedIndex >= descriptor.elements.size())
        throw TypeError(std::format("tuple length mismatch: expected {}, got {}", descriptor.elements.size(),
                                    expression.operands.size()),
                        expression.span);
      static_cast<void>(inferExpected(*element, descriptor.elements[expectedIndex++], locals,
                                      std::format("tuple element {}", expectedIndex)));
    }
    if (expectedIndex != descriptor.elements.size())
      throw TypeError(
          std::format("tuple length mismatch: expected {}, got {}", descriptor.elements.size(), expectedIndex),
          expression.span);
    record(expression, expected);
    return expected;
  }
  if (expression.kind == hir::ExpressionKind::Closure && descriptor.kind == TypeKind::Closure)
  {
    if (expression.closureParameterIds.size() != descriptor.elements.size())
      throw TypeError(std::format("closure arity mismatch: expected {}, got {}", descriptor.elements.size(),
                                  expression.closureParameterIds.size()),
                      expression.span);
    std::vector<TypeId> parameterTypes;
    parameterTypes.reserve(expression.closureParameterIds.size());
    for (size_t index = 0; index < expression.closureParameterIds.size(); ++index)
    {
      if (expression.closureParameterTypes[index] == nullptr)
      {
        parameterTypes.push_back(descriptor.elements[index]);
        continue;
      }
      const TypeId declared =
          interner_.resolveInScope(*expression.closureParameterTypes[index], genericBindings_);
      parameterTypes.push_back(declared);
    }
    const TypeId actual = inferClosure(expression, parameterTypes, locals);
    requireType(expected, actual, expression.span, context);
    record(expression, actual);
    return actual;
  }
  const TypeId actual = infer(expression, locals);
  requireType(expected, actual, expression.span, context);
  return actual;
}

auto Checker::infer(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
{
  TypeId type;
  switch (expression.kind)
  {
  case hir::ExpressionKind::IntegerLiteral:
    type = expression.numericSuffix.empty() ? builtin::I64 : typeFromNumericSuffix(expression.numericSuffix);
    if (isIntegerBuiltin(type))
      checkIntegerLiteralMagnitude(parseIntegerLiteralMagnitude(expression.text, type, expression.span),
                                   expression.text, type, expression.span);
    break;
  case hir::ExpressionKind::FloatLiteral:
    type = expression.numericSuffix.empty() ? builtin::F64 : typeFromNumericSuffix(expression.numericSuffix);
    static_cast<void>(std::stod(expression.text));
    break;
  case hir::ExpressionKind::StringLiteral:
    type = builtin::String;
    break;
  case hir::ExpressionKind::BooleanLiteral:
    type = builtin::Bool;
    break;
  case hir::ExpressionKind::ArrayLiteral:
  {
    if (expression.operands.empty())
      throw TypeError("cannot infer the type of an empty array literal", expression.span);
    size_t mapSpreads = 0;
    size_t valueSpreads = 0;
    TypeId element{};
    for (const auto &candidate : expression.operands)
    {
      if (candidate->kind == hir::ExpressionKind::Prefix && candidate->text == "...")
      {
        bool filterMode = false;
        const hir::Expression *inner = candidate->operands[0].get();
        if (inner->kind == hir::ExpressionKind::Prefix && inner->text == "?")
        {
          filterMode = true;
          inner = inner->operands[0].get();
        }
        const bool mapSpread = inner->kind == hir::ExpressionKind::Call && !inner->operands.empty() &&
                                inner->operands[0]->resolvedName.has_value() &&
                                inner->operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function;
        if (!mapSpread)
        {
          ++valueSpreads;
          if (filterMode)
            throw TypeError("the filter marker `?` requires a map spread", candidate->span);
          const TypeId source = infer(*inner, locals);
          const auto &sourceDescriptor = interner_.descriptor(source);
          const bool arraySource = sourceDescriptor.kind == TypeKind::DynamicArray ||
                                    sourceDescriptor.kind == TypeKind::FixedArray ||
                                    sourceDescriptor.kind == TypeKind::DependentArray;
          const bool rangeSource = sourceDescriptor.kind == TypeKind::Range;
          bool listSource = false;
          TypeId listElement{};
          if (sourceDescriptor.kind == TypeKind::Enum)
          {
            for (const auto payload : sourceDescriptor.elements)
            {
              const auto &payloadDescriptor = interner_.descriptor(payload);
              if (payloadDescriptor.kind == TypeKind::Tuple && payloadDescriptor.elements.size() == 2 &&
                  isListTail(interner_.descriptor(payloadDescriptor.elements[1]), source, interner_))
              {
                listSource = true;
                listElement = payloadDescriptor.elements[0];
              }
            }
          }
          if (!arraySource && !rangeSource && !listSource)
            throw TypeError(std::format("cannot spread value of type {}", interner_.display(source)),
                            candidate->span);
          if (rangeSource && sourceDescriptor.element != builtin::I64)
            throw TypeError("spreading ranges currently requires i64 elements", candidate->span);
          const TypeId sourceElement = listSource ? listElement : sourceDescriptor.element;
          if (element.value == 0)
            element = sourceElement;
          else
            requireType(element, sourceElement, candidate->span, "array spread");
          continue;
        }
        ++mapSpreads;
        if (inner->operands.size() != 2)
          throw TypeError("map spread function must take exactly one argument", candidate->span);
        const TypeId source = infer(*inner->operands[1], locals);
        const auto &sourceDescriptor = interner_.descriptor(source);
        const bool arraySource = sourceDescriptor.kind == TypeKind::DynamicArray ||
                                  sourceDescriptor.kind == TypeKind::FixedArray ||
                                  sourceDescriptor.kind == TypeKind::DependentArray;
        const bool rangeSource = sourceDescriptor.kind == TypeKind::Range;
        if (!arraySource && !rangeSource)
          throw TypeError(
              std::format("map spread source must be an array or range, got {}", interner_.display(source)),
              inner->operands[1]->span);
        if (rangeSource && sourceDescriptor.element != builtin::I64)
          throw TypeError("map spread over ranges currently requires i64 elements", inner->operands[1]->span);
        const TypeId sourceElement = sourceDescriptor.element;
        const auto &innerSignature = signatures_.at(inner->operands[0]->resolvedName->id);
        if (innerSignature.parameters.size() != 1)
          throw TypeError("map spread function must take exactly one argument", candidate->span);
        Substitution mapSubstitution;
        unify(innerSignature.parameters.front(), sourceElement, mapSubstitution, inner->operands[1]->span);
        requireConstArguments(innerSignature, mapSubstitution, inner->operands[0]->text, inner->span);
        if (module_ != nullptr && inner->operands[0]->resolvedName->id < module_->functions.size() &&
            module_->functions.at(inner->operands[0]->resolvedName->id).whereClause != nullptr &&
            !evaluateWhereCondition(*module_->functions.at(inner->operands[0]->resolvedName->id).whereClause,
                                    mapSubstitution, innerSignature))
          throw TypeError(std::format("call to `{}` does not satisfy its where clause",
                                      module_->functions.at(inner->operands[0]->resolvedName->id).name),
                          inner->span);
        const TypeId mapped = specializeReturnType(innerSignature, mapSubstitution, 0);
        if (filterMode)
          requireType(builtin::Bool, mapped, candidate->span, "filter predicate result");
        callTargets_.insert_or_assign(inner, hir::DefId{inner->operands[0]->resolvedName->id});
        record(*inner, mapped);
        if (mapSpreads == 1)
          element = filterMode ? sourceElement : mapped;
        else
          requireType(element, filterMode ? sourceElement : mapped, candidate->span, "map spread result");
        continue;
      }
      const TypeId candidateType = infer(*candidate, locals);
      if (interner_.descriptor(candidateType).kind == TypeKind::Reference)
        throw TypeError("references cannot be stored in arrays", candidate->span);
      if (element.value == 0)
        element = candidateType;
      requireType(element, candidateType, candidate->span, "array element");
    }
    if (mapSpreads + valueSpreads > 1)
      throw TypeError("array literals support at most one spread", expression.span);
    type = interner_.internDynamicArray(element);
    break;
  }
  case hir::ExpressionKind::StructLiteral:
  {
    if (!expression.structId.has_value())
      throw TypeError("struct literal has no resolved type", expression.span);
    if (interner_.structGenericArity(*expression.structId) != 0)
      throw TypeError(
          std::format("cannot infer generic arguments for struct literal `{}`; annotate the binding type",
                      expression.text),
          expression.span);
    type = interner_.typeForStruct(*expression.structId);
    return inferExpectedStruct(expression, type, locals);
  }
  case hir::ExpressionKind::EnumLiteral:
  {
    if (!expression.enumId.has_value() || !expression.variant.has_value())
      throw TypeError("enum constructor has no resolved variant", expression.span);
    if (interner_.enumGenericArity(*expression.enumId) != 0)
      throw TypeError(std::format("cannot infer generic arguments for enum constructor `{}`", expression.text),
                      expression.span);
    type = interner_.typeForEnum(*expression.enumId);
    const auto &descriptor = interner_.descriptor(type);
    const uint32_t variant = *expression.variant;
    if (descriptor.variantHasPayload.at(variant) &&
        interner_.descriptor(descriptor.elements.at(variant)).kind == TypeKind::Tuple)
    {
      const auto &tuple = interner_.descriptor(descriptor.elements.at(variant));
      if (expression.operands.size() != tuple.elements.size())
        throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}",
                                    descriptor.fieldNames.at(variant), tuple.elements.size(),
                                    expression.operands.size()),
                        expression.span);
      for (size_t index = 0; index < expression.operands.size(); ++index)
        static_cast<void>(inferExpected(*expression.operands[index], tuple.elements[index], locals,
                                        std::format("variant `{}` payload", descriptor.fieldNames.at(variant))));
      break;
    }
    const size_t expected = descriptor.variantHasPayload.at(variant) ? 1 : 0;
    if (expression.operands.size() != expected)
      throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}",
                                  descriptor.fieldNames.at(variant), expected, expression.operands.size()),
                      expression.span);
    if (expected == 1)
      static_cast<void>(inferExpected(*expression.operands[0], descriptor.elements.at(variant), locals,
                                      std::format("variant `{}` payload", descriptor.fieldNames.at(variant))));
    break;
  }
  case hir::ExpressionKind::TupleLiteral:
  {
    std::vector<TypeId> elements;
    for (const auto &element : expression.operands)
    {
      if (element->kind == hir::ExpressionKind::Prefix && element->text == "...")
      {
        const TypeId operand = infer(*element->operands[0], locals);
        const auto &descriptor = interner_.descriptor(operand);
        if (descriptor.kind == TypeKind::Tuple)
          elements.insert(elements.end(), descriptor.elements.begin(), descriptor.elements.end());
        else if (descriptor.kind == TypeKind::TypePack)
          elements.push_back(descriptor.element);
        else
          throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)),
                          element->span);
        continue;
      }
      const TypeId elementType = infer(*element, locals);
      if (interner_.descriptor(elementType).kind == TypeKind::Reference)
        throw TypeError("references cannot be stored in tuples", element->span);
      elements.push_back(elementType);
    }
    type = interner_.internTuple(elements);
    break;
  }
  case hir::ExpressionKind::ResolvedName:
    if (expression.resolvedName->kind == hir::ResolvedNameKind::Function)
      throw TypeError("function name cannot be used as a value", expression.span);
    if (expression.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
      throw TypeError(std::format("const parameter `{}` is not a runtime value", expression.text),
                      expression.span);
    if (moveState_.isWholeMoved(expression.resolvedName->id))
      throw TypeError("E-move-used", std::format("use of moved value `{}`", expression.text), expression.span);
    type = locals.at(expression.resolvedName->id);
    break;
  case hir::ExpressionKind::Grouped:
    type = infer(*expression.operands[0], locals);
    break;
  case hir::ExpressionKind::GenericApplication:
  {
    if (const auto builtin =
            evaluateTraitIntrospection(expression.text, expression.genericArguments, expression.span);
        builtin.has_value())
    {
      type = builtin::Bool;
      break;
    }
    std::vector<TypeId> typeArguments;
    for (const auto &argument : expression.genericArguments)
    {
      if (argument.kind != syntax::GenericArgumentKind::Type)
        throw TypeError("const arguments on const predicates are not yet supported", argument.span);
      const TypeId resolved = interner_.resolveInScope(*argument.type, genericBindings_);
      if (interner_.descriptor(resolved).kind == TypeKind::TypeParameter)
        throw TypeError(std::format("cannot evaluate const declaration `{}` for abstract type parameter `{}`",
                                    expression.text, interner_.display(resolved)),
                        argument.span);
      typeArguments.push_back(resolved);
    }
    if (const auto builtin = evaluateTupleIntrospection(expression.text, typeArguments, expression.span);
        builtin.has_value())
    {
      type = interner_.constInterner().value(*builtin).kind == const_eval::ConstValueKind::Bool ? builtin::Bool
                                                                                                 : builtin::I64;
      break;
    }
    static_cast<void>(selectConstDeclaration(expression.text, typeArguments, expression.span));
    type = builtin::Bool;
    break;
  }
  case hir::ExpressionKind::Prefix:
  {
    TypeId operand{};
    if ((expression.text == "-" || expression.text == "+") &&
        expression.operands[0]->kind == hir::ExpressionKind::IntegerLiteral &&
        expression.operands[0]->numericSuffix.empty())
    {
      operand = builtin::I64;
      record(*expression.operands[0], operand);
      const uint64_t magnitude =
          parseIntegerLiteralMagnitude(expression.operands[0]->text, builtin::I64, expression.span);
      constexpr uint64_t MaxMagnitude = 1ULL << 63;
      if (expression.text == "+")
      {
        checkIntegerLiteralMagnitude(magnitude, expression.operands[0]->text, builtin::I64, expression.span);
      }
      else if (magnitude > MaxMagnitude)
      {
        throw TypeError(std::format("integer literal `-{}` is out of range", expression.operands[0]->text),
                        expression.span);
      }
    }
    else
    {
      operand = infer(*expression.operands[0], locals);
    }
    if (expression.text == "ref" || expression.text == "ref mut")
    {
      if (!isPlace(*expression.operands[0]))
        throw TypeError("reference operand is not a place", expression.span);
      // A1.3 (deferred): creating a reference rooted through an existing
      // scoped reference would need ref-of-ref shadows; reads/writes through
      // `arr[i]` / `s.field` on reference receivers are supported (A1.2), but
      // `ref` built on such a place waits for the next slice.
      if (const auto root = rootLocalOf(*expression.operands[0]); root.has_value() &&
                                                                    interner_.descriptor(locals.at(root->value)).kind == TypeKind::Reference)
        throw TypeError("creating a reference through a scoped reference is deferred (A1.3); dereference the "
                        "reference first",
                        expression.span);
      const auto &operandDescriptor = interner_.descriptor(operand);
      if (operandDescriptor.kind == TypeKind::Reference)
        throw TypeError("cannot create a reference to a reference", expression.span);
      if (expression.text == "ref mut")
        requireMutableRoot(*expression.operands[0], expression.span);
      if (const auto root = rootLocalOf(*expression.operands[0]); root.has_value())
      {
        if (borrowExpressions_.insert(&expression).second)
        {
          auto &counts = borrowState_.counts[root->value];
          if (expression.text == "ref mut")
          {
            if (counts.shared != 0)
              throw TypeError("E-borrow-conflict",
                              std::format("cannot mutably borrow `{}` while it is shared-borrowed",
                                          expression.operands[0]->text),
                              expression.span);
            if (counts.mutableRefs != 0)
              throw TypeError("E-borrow-conflict",
                              std::format("cannot mutably borrow `{}` while it is already mutably borrowed",
                                          expression.operands[0]->text),
                              expression.span);
            borrowState_.add(&expression, root->value, 0, true);
          }
          else
          {
            if (counts.mutableRefs != 0)
              throw TypeError("E-borrow-conflict",
                              std::format("cannot shared-borrow `{}` while it is mutably borrowed",
                                          expression.operands[0]->text),
                              expression.span);
            if (counts.shared >= BorrowState::MaxShared)
              throw TypeError("E-borrow-too-many", "too many shared borrows of one binding", expression.span);
            borrowState_.add(&expression, root->value, 0, false);
          }
        }
      }
      type = interner_.internReference(operand, expression.text == "ref mut");
      break;
    }
    if (expression.text == "*")
    {
      const auto &descriptor = interner_.descriptor(operand);
      if (descriptor.kind != TypeKind::Reference)
        throw TypeError(std::format("cannot dereference value of type {}", interner_.display(operand)),
                        expression.span);
      type = descriptor.element;
      break;
    }
    if (expression.text == "move" || expression.text == "clone")
    {
      type = operand;
      break;
    }
    if (expression.text == "...")
    {
      throw TypeError("spread is only valid inside tuple literals", expression.span);
    }
    if (expression.text == "!")
    {
      type = builtin::Bool;
      requireType(type, operand, expression.span, "prefix operand");
    }
    else if (isIntegerBuiltin(operand) || isFloatBuiltin(operand))
    {
      type = operand;
    }
    else
    {
      type = builtin::I64;
      requireType(type, operand, expression.span, "prefix operand");
    }
    break;
  }
  case hir::ExpressionKind::Binary:
  {
    TypeId left = infer(*expression.operands[0], locals);
    TypeId right = infer(*expression.operands[1], locals);
    const bool equality = expression.text == "==" || expression.text == "!=";
    const bool ordering =
        expression.text == "<" || expression.text == "<=" || expression.text == ">" || expression.text == ">=";
    {
      if (expression.operands[1]->kind == hir::ExpressionKind::IntegerLiteral &&
          expression.operands[1]->numericSuffix.empty() &&
          (isIntegerBuiltin(left) || ((equality || ordering) && isFloatBuiltin(left))) && right != left)
        right = inferExpected(*expression.operands[1], left, locals, "integer literal");
      if (expression.operands[0]->kind == hir::ExpressionKind::IntegerLiteral &&
          expression.operands[0]->numericSuffix.empty() &&
          (isIntegerBuiltin(right) || ((equality || ordering) && isFloatBuiltin(right))) && left != right)
        left = inferExpected(*expression.operands[0], right, locals, "integer literal");
      if (expression.operands[1]->kind == hir::ExpressionKind::FloatLiteral &&
          expression.operands[1]->numericSuffix.empty() && isFloatBuiltin(left) && right != left)
        right = inferExpected(*expression.operands[1], left, locals, "float literal");
      if (expression.operands[0]->kind == hir::ExpressionKind::FloatLiteral &&
          expression.operands[0]->numericSuffix.empty() && isFloatBuiltin(right) && left != right)
        left = inferExpected(*expression.operands[0], right, locals, "float literal");
    }
    if (expression.text == "..")
    {
      if (!isIntegerBuiltin(left))
        requireType(builtin::I64, left, expression.operands[0]->span, "range start");
      if (!isIntegerBuiltin(right))
        requireType(builtin::I64, right, expression.operands[1]->span, "range end");
      requireType(left, right, expression.span, "range bounds");
      type = interner_.internRange(left);
      break;
    }
    if (expression.text == "<<")
    {
      const auto &leftDescriptor = interner_.descriptor(left);
      if (leftDescriptor.kind == TypeKind::DynamicArray)
      {
        if (expression.operands[1]->kind == hir::ExpressionKind::IntegerLiteral &&
            isIntegerBuiltin(leftDescriptor.element) && right != leftDescriptor.element)
          right = inferExpected(*expression.operands[1], leftDescriptor.element, locals, "appended element");
        requireType(leftDescriptor.element, right, expression.operands[1]->span, "appended element");
        type = left;
        break;
      }
    }
    if (equality || ordering)
    {
      if (interner_.descriptor(left).kind == TypeKind::Union)
      {
        const auto &members = interner_.descriptor(left).elements;
        if (std::find(members.begin(), members.end(), right) != members.end())
        {
          record(*expression.operands[0], right);
          left = right;
        }
      }
      if (interner_.descriptor(right).kind == TypeKind::Union)
      {
        const auto &members = interner_.descriptor(right).elements;
        if (std::find(members.begin(), members.end(), left) != members.end())
        {
          record(*expression.operands[1], left);
          right = left;
        }
      }
    }
    const bool numericPair = isNumericBuiltin(left) && isNumericBuiltin(right);
    if ((equality || ordering) && numericPair && left != right)
    {
      const bool bothIntegers = isIntegerBuiltin(left) && isIntegerBuiltin(right);
      const bool mixedKind = isFloatBuiltin(left) != isFloatBuiltin(right);
      const bool mixedSign = bothIntegers && isSignedIntegerBuiltin(left) != isSignedIntegerBuiltin(right);
      if (mixedSign)
        throw TypeError("E-compare-mixed-sign",
                        std::format("implicit mixed-sign comparison between {} and {}; convert explicitly "
                                    "with toFloat or toSigned",
                                    interner_.display(left), interner_.display(right)),
                        expression.span);
      if (mixedKind)
        throw TypeError("E-compare-mixed-kind",
                        std::format("implicit mixed numeric-kind comparison between {} and {}; convert "
                                    "explicitly with toFloat or toSigned",
                                    interner_.display(left), interner_.display(right)),
                        expression.span);
    }
    if (!((equality || ordering) && numericPair))
      requireType(left, right, expression.span, "binary operands");
    if (equality)
      type = builtin::Bool;
    else if (ordering)
    {
      if (!numericPair)
        requireType(builtin::I64, left, expression.span, "comparison operand");
      type = builtin::Bool;
    }
    else if (expression.text == "&&" || expression.text == "||")
    {
      requireType(builtin::Bool, left, expression.span, "logical operand");
      type = builtin::Bool;
    }
    else
    {
      if (expression.text != "+" || left != builtin::String)
      {
        if (!isIntegerBuiltin(left) && !isFloatBuiltin(left))
          requireType(builtin::I64, left, expression.span, "binary operand");
      }
      type = left;
    }
    break;
  }
  case hir::ExpressionKind::Call:
  {
    if (expression.methodCall)
      return inferMethodCall(expression, locals);
    if (expression.operands[0]->resolvedName.has_value() &&
        expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Local)
    {
      const TypeId calleeType = infer(*expression.operands[0], locals);
      if (interner_.descriptor(calleeType).kind == TypeKind::Closure)
      {
        const auto foundCode = closureLocals_.find(expression.operands[0]->resolvedName->id);
        if (foundCode == closureLocals_.end())
        {
          // INC-14 S2c/S4: templates call closure-typed parameters or generic
          // closure locals with no concrete binding yet; the model types the
          // call from the descriptor and defers codegen to the
          // closure-specialized instance.
          if (placeholderFunctions_.contains(currentFunctionId_.value) &&
              (parameterLocals_.contains(expression.operands[0]->resolvedName->id) ||
               deferredClosureLocals_.contains(expression.operands[0]->resolvedName->id)))
          {
            const auto &closure = interner_.descriptor(calleeType);
            if (closure.elements.size() != expression.operands.size() - 1)
              throw TypeError(std::format("closure argument count mismatch: expected {}, got {}",
                                          closure.elements.size(), expression.operands.size() - 1),
                              expression.span);
            for (size_t index = 0; index < closure.elements.size(); ++index)
              static_cast<void>(inferExpected(*expression.operands[index + 1], closure.elements[index], locals,
                                              std::format("closure argument {}", index + 1)));
            type = closure.element;
            break;
          }
          throw TypeError(std::format("closure value `{}` was not created directly by a closure literal",
                                      expression.operands[0]->text),
                          expression.span);
        }
        const auto &closure = interner_.descriptor(calleeType);
        if (closure.elements.size() != expression.operands.size() - 1)
          throw TypeError(std::format("closure argument count mismatch: expected {}, got {}",
                                      closure.elements.size(), expression.operands.size() - 1),
                          expression.span);
        for (size_t index = 0; index < closure.elements.size(); ++index)
          static_cast<void>(inferExpected(*expression.operands[index + 1], closure.elements[index], locals,
                                          std::format("closure argument {}", index + 1)));
        callTargets_.insert_or_assign(&expression, foundCode->second);
        // INC-14 S3: record capture values for call-site threading. Original
        // frames look up the closure local's capture locals; instance frames
        // look up the instance's capture params.
        std::vector<uint32_t> captureIds;
        const auto foundCaptures = closureCapturesByLocal_.find(expression.operands[0]->resolvedName->id);
        if (foundCaptures != closureCapturesByLocal_.end())
        {
          for (const auto &cap : foundCaptures->second)
            captureIds.push_back(cap.localId);
        }
        else
        {
          const auto foundInstanceCaptures = instanceCaptureParamsByLocal_.find(expression.operands[0]->resolvedName->id);
          if (foundInstanceCaptures != instanceCaptureParamsByLocal_.end())
            captureIds = foundInstanceCaptures->second;
        }
        if (!captureIds.empty())
          callCaptures_.emplace(&expression, std::move(captureIds));
        type = closure.element;
        break;
      }
    }
    if (!expression.operands[0]->resolvedName.has_value() ||
        expression.operands[0]->resolvedName->kind != hir::ResolvedNameKind::Function)
      throw TypeError("call target is not a function", expression.operands[0]->span);
    {
      const FoldInfo fold = analyzeFoldCall(expression, locals);
      if (fold.fold)
      {
        if (expression.operands.size() != 3)
          throw TypeError("fold calls require exactly one accumulator argument", expression.span);
        const auto &signature = signatures_.at(expression.operands[0]->resolvedName->id);
        if (signature.parameters.size() != 2)
          throw TypeError("fold function must take exactly two arguments", expression.span);
        Substitution substitution;
        unify(signature.parameters[fold.spreadPosition], fold.elementType, substitution, expression.span);
        const TypeId accumulatorType = infer(*expression.operands[fold.accumulatorPosition + 1], locals);
        unify(signature.parameters[fold.accumulatorPosition], accumulatorType, substitution, expression.span);
        const TypeId result = specialize(signature.returnType, substitution);
        unify(signature.parameters[fold.accumulatorPosition], result, substitution, expression.span);
        requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
        if (module_ != nullptr && expression.operands[0]->resolvedName->id < module_->functions.size() &&
            module_->functions.at(expression.operands[0]->resolvedName->id).whereClause != nullptr &&
            !evaluateWhereCondition(*module_->functions.at(expression.operands[0]->resolvedName->id).whereClause,
                                    substitution, signature))
          throw TypeError(std::format("call to `{}` does not satisfy its where clause",
                                      module_->functions.at(expression.operands[0]->resolvedName->id).name),
                          expression.span);
        callFoldSpreadPositions_.insert_or_assign(&expression, std::vector<size_t>{fold.spreadPosition});
        callFoldAccumulatorPositions_.insert_or_assign(&expression,
                                                        std::vector<size_t>{fold.accumulatorPosition});
        callTargets_.insert_or_assign(&expression, hir::DefId{expression.operands[0]->resolvedName->id});
        type = specialize(signature.returnType, substitution);
        break;
      }
    }
    hir::DefId selected{expression.operands[0]->resolvedName->id};
    if (expression.operands[0]->functionCandidates.size() > 1)
    {
      int bestScore{-1};
      for (const auto candidate : expression.operands[0]->functionCandidates)
      {
        const auto &candidateSignature = signatures_.at(candidate.value);
        if (candidateSignature.parameters.size() != expression.operands.size() - 1)
          continue;
        int score = isGeneric(candidateSignature) ? 0 : 100;
        for (const auto parameter : candidateSignature.parameters)
          score += specificity(parameter);
        if (!isGeneric(candidateSignature))
        {
          try
          {
            for (size_t index = 0; index < candidateSignature.parameters.size(); ++index)
              requireType(candidateSignature.parameters[index], infer(*expression.operands[index + 1], locals),
                          expression.operands[index + 1]->span, "specialization argument");
          }
          catch (const TypeError &)
          {
            continue;
          }
        }
        if (score > bestScore)
        {
          bestScore = score;
          selected = candidate;
        }
        else if (score == bestScore && candidate != selected)
        {
          throw TypeError("ambiguous function specialization", expression.span);
        }
      }
      if (bestScore < 0)
        throw TypeError("no matching function specialization", expression.span);
    }
    const auto &signature = signatures_.at(selected.value);
    CallSpreadInfo spreadInfo;
    const bool hasSpread = analyzeCallSpread(expression, 1, locals, spreadInfo);
    const size_t supplied = hasSpread ? spreadInfo.expandedCount : expression.operands.size() - 1;
    const bool variadic = !signature.packParameters.empty();
    const size_t fixedParameters = signature.parameters.size() - (variadic ? 1 : 0);
    if (variadic ? supplied < fixedParameters : supplied != signature.parameters.size())
      throw TypeError(
          std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied),
          expression.span);
    const size_t packCount = variadic ? supplied - fixedParameters : 0;
    if (hasSpread)
      callSpreadPositions_.insert_or_assign(&expression, spreadInfo.spreadPositions);
    Substitution substitution;
    // INC-14 S2c: closure-typed parameters receive closure literal arguments
    // inside their generic-argument machinery. Literal arguments are deferred
    // until the substitution is bound; both literal and variable arguments
    // record the closure's code function so instantiateFunction can re-check
    // the callee with the concrete closure binding.
    std::vector<size_t> deferredClosureArgs;
    std::vector<std::pair<size_t, hir::DefId>> closureTargets;
    // INC-17: reuse the argument types precomputed by analyzeCallSpread instead
    // of re-inferring. Re-inferring a nested call re-reads its by-value affine
    // locals (which the first pass already consumed), raising a spurious
    // E-move-used on the second pass. Closure-literal slots are left empty
    // (TypeId{}) by analyzeCallSpread and are inferred on demand instead.
    const auto argumentTypeAt = [&](size_t index) -> TypeId
    {
      if (hasSpread)
        return spreadInfo.expandedTypes[index];
      if (expression.operands[index + 1]->kind == hir::ExpressionKind::Closure)
        return infer(*expression.operands[index + 1], locals);
      return spreadInfo.expandedTypes[index];
    };
    if (!expression.genericArguments.empty())
    {
      substitution = explicitSubstitution(expression, signature);
      for (size_t index = 0; index < supplied; ++index)
      {
        const TypeId parameterType = index < fixedParameters
                                        ? signature.parameters[index]
                                        : interner_.descriptor(signature.parameters.back()).element;
        if (hasSpread && spreadInfo.spreadSlot[index])
          requireType(specialize(parameterType, substitution), argumentTypeAt(index),
                      spreadInfo.argumentExpressions[index]->span, std::format("call argument {}", index + 1));
        else
          static_cast<void>(inferExpected(*expression.operands[index + 1],
                                          specialize(parameterType, substitution), locals,
                                          std::format("call argument {}", index + 1)));
      }
    }
    else
    {
      for (size_t index = 0; index < supplied; ++index)
      {
        const hir::Expression &argument =
            hasSpread ? *spreadInfo.argumentExpressions[index] : *expression.operands[index + 1];
        const TypeId parameterType = index < fixedParameters
                                        ? signature.parameters[index]
                                        : interner_.descriptor(signature.parameters.back()).element;
        if (!hasSpread && index < fixedParameters && argument.kind == hir::ExpressionKind::Closure)
        {
          deferredClosureArgs.push_back(index);
          continue;
        }
        const TypeId argumentType = argumentTypeAt(index);
        if (!hasSpread && index < fixedParameters && argument.resolvedName.has_value() &&
            argument.resolvedName->kind == hir::ResolvedNameKind::Local &&
            interner_.descriptor(argumentType).kind == TypeKind::Closure)
        {
          const auto bound = closureLocals_.find(argument.resolvedName->id);
          if (bound != closureLocals_.end())
          {
            closureTargets.emplace_back(index, bound->second);
            // INC-14 S3/S4: a capturing closure passed into a higher-order
            // function threads its capture values into the specialized
            // instance as leading args. When forwarding a closure-typed param
            // of the current instance, thread that instance's own capture
            // params (frame-local) rather than the original closure's captures.
            if (callTargets_.find(&expression) == callTargets_.end())
            {
              const auto instanceParams = instanceCaptureParamsByLocal_.find(argument.resolvedName->id);
              auto &callCaptureList = callCaptures_[&expression];
              if (instanceParams != instanceCaptureParamsByLocal_.end())
                callCaptureList.insert(callCaptureList.end(), instanceParams->second.begin(),
                                       instanceParams->second.end());
              else
              {
                const auto foundCaptures = closureCapturesByLocal_.find(argument.resolvedName->id);
                if (foundCaptures != closureCapturesByLocal_.end())
                {
                  for (const auto &cap : foundCaptures->second)
                    callCaptureList.push_back(cap.localId);
                }
              }
            }
          }
        }
        if (!isGeneric(signature))
        {
          if (hasSpread && spreadInfo.spreadSlot[index])
            requireType(parameterType, argumentType, argument.span, std::format("call argument {}", index + 1));
          else
            static_cast<void>(
                inferExpected(argument, parameterType, locals, std::format("call argument {}", index + 1)));
          continue;
        }
        if (variadic && index >= fixedParameters)
        {
          auto &pack = substitution.packs[parameterType.value];
          const size_t packPosition = index - fixedParameters;
          if (packPosition < pack.size())
          {
            if (pack[packPosition] != argumentType)
              throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                          interner_.display(pack[packPosition]), interner_.display(argumentType)),
                              argument.span);
          }
          else
          {
            pack.push_back(argumentType);
          }
          continue;
        }
        const auto &expectedDescriptor = interner_.descriptor(parameterType);
        if (hasSpread && spreadInfo.spreadSlot[index])
        {
          unify(parameterType, argumentType, substitution, argument.span);
          continue;
        }
        if (argument.kind == hir::ExpressionKind::EnumLiteral && expectedDescriptor.kind == TypeKind::Enum)
        {
          if (!argument.enumId.has_value() || !argument.variant.has_value() ||
              expectedDescriptor.nominalId != argument.enumId->value)
            throw TypeError("generic enum constructor type mismatch", argument.span);
          const uint32_t variant = *argument.variant;
          if (variant >= expectedDescriptor.elements.size())
            throw TypeError("enum variant is out of range", argument.span);
          if (argument.operands.size() != (expectedDescriptor.variantHasPayload[variant] ? 1u : 0u))
            throw TypeError(
                std::format("enum variant `{}` payload arity mismatch", expectedDescriptor.fieldNames[variant]),
                argument.span);
          if (!argument.operands.empty())
            unify(expectedDescriptor.elements[variant], infer(*argument.operands.front(), locals), substitution,
                  argument.operands.front()->span);
        }
        else
        {
          unify(parameterType, argumentType, substitution, argument.span);
        }
      }
    }
    for (const size_t index : deferredClosureArgs)
    {
      const auto &argument = expression.operands[index + 1];
      const TypeId parameterType = index < fixedParameters
                                       ? signature.parameters[index]
                                       : interner_.descriptor(signature.parameters.back()).element;
      static_cast<void>(
          inferCallClosureArgument(*argument, parameterType, index, locals, substitution, closureTargets));
      // INC-14 S3: a capturing closure literal passed into a higher-order call
      // threads its capture values into the specialized instance as leading
      // args (recorded once, before the call target is committed).
      if (callTargets_.find(&expression) == callTargets_.end() && !argument->closureCaptures.empty())
      {
        std::vector<uint32_t> captureIds;
        for (const auto &cap : argument->closureCaptures)
          captureIds.push_back(cap.local.value);
        auto &callCaptureList = callCaptures_[&expression];
        callCaptureList.insert(callCaptureList.end(), captureIds.begin(), captureIds.end());
      }
    }
    type = specializeReturnType(signature, substitution, packCount);
    if (!signature.genericParameters.empty() && interner_.descriptor(type).kind == TypeKind::TypeParameter)
      throw TypeError(
          std::format("cannot infer generic arguments for function `{}`", expression.operands[0]->text),
          expression.span);
    requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
    bool substitutionConcrete = true;
    for (const auto &[parameter, argument] : substitution.types)
      if (interner_.descriptor(argument).kind == TypeKind::TypeParameter)
        substitutionConcrete = false;
    if (substitutionConcrete && module_ != nullptr && selected.value < module_->functions.size() &&
        module_->functions.at(selected.value).whereClause != nullptr &&
        !evaluateWhereCondition(*module_->functions.at(selected.value).whereClause, substitution, signature))
      throw TypeError(std::format("call to `{}` does not satisfy its where clause",
                                  module_->functions.at(selected.value).name),
                      expression.span);
    const bool externTarget =
        selected.value < module_->functions.size() && module_->functions.at(selected.value).externC;
    if (!hasSpread)
    {
      for (size_t index = 0; index < supplied; ++index)
      {
        const TypeId parameterType = index < fixedParameters
                                        ? signature.parameters[index]
                                        : interner_.descriptor(signature.parameters.back()).element;
        const auto &parameterDescriptor = interner_.descriptor(parameterType);
        if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer &&
            !externTarget)
          trackConsumption(*expression.operands[index + 1], locals);
      }
    }
    else
    {
      for (size_t index = 0; index < supplied; ++index)
      {
        if (!spreadInfo.spreadSlot[index])
          continue;
        const TypeId parameterType = index < fixedParameters
                                        ? signature.parameters[index]
                                        : interner_.descriptor(signature.parameters.back()).element;
        const auto &parameterDescriptor = interner_.descriptor(parameterType);
        if (parameterDescriptor.kind != TypeKind::Reference && parameterDescriptor.kind != TypeKind::RawPointer &&
            !externTarget)
          trackConsumption(*spreadInfo.argumentExpressions[index], locals);
      }
    }
    if (variadic)
    {
      callPackArgCounts_.insert_or_assign(&expression, packCount);
      std::vector<TypeId> packedTypes;
      for (size_t index = fixedParameters; index < supplied; ++index)
        packedTypes.push_back(argumentTypeAt(index));
      callPackTupleTypes_.insert_or_assign(&expression, interner_.internTuple(std::move(packedTypes)));
    }
    selected = instantiateFunction(selected, substitution, packCount, expression.span, closureTargets);
    callTargets_.insert_or_assign(&expression, selected);
    break;
  }
  case hir::ExpressionKind::Index:
  {
    const TypeId receiver = infer(*expression.operands[0], locals);
    const TypeId indexType = infer(*expression.operands[1], locals);
    // D-002 A1.2: place-through-ref — an element access on a scoped reference
    // receiver (`arr[i]` on `array<T> ref`) is equivalent to indexing the
    // dereferenced value; the read/write lowers through the ref object.
    TypeId base = receiver;
    bool throughRef = false;
    if (interner_.descriptor(receiver).kind == TypeKind::Reference)
    {
      base = interner_.descriptor(receiver).element;
      throughRef = true;
    }
    if (interner_.descriptor(indexType).kind == TypeKind::Range)
    {
      if (throughRef)
        throw TypeError("cannot slice through a scoped reference (A1.3 is deferred); dereference the reference first",
                        expression.span);
      const auto &descriptor = interner_.descriptor(base);
      if (descriptor.kind != TypeKind::DynamicArray && descriptor.kind != TypeKind::FixedArray &&
          descriptor.kind != TypeKind::DependentArray)
        throw TypeError(std::format("cannot slice value of type {}", interner_.display(receiver)),
                        expression.span);
      type = interner_.internDynamicArray(descriptor.element);
      break;
    }
    requireType(builtin::I64, infer(*expression.operands[1], locals), expression.operands[1]->span,
                "array index");
    const auto &descriptor = interner_.descriptor(base);
    if (descriptor.kind == TypeKind::Tuple)
    {
      if (expression.text.empty())
        throw TypeError("tuple index must be an integer literal", expression.operands[1]->span);
      uint64_t index{};
      const auto [end, error] =
          std::from_chars(expression.text.data(), expression.text.data() + expression.text.size(), index);
      if (error != std::errc{} || end != expression.text.data() + expression.text.size() ||
          index >= descriptor.elements.size())
        throw TypeError(std::format("tuple index {} is out of range for length {}", expression.text,
                                    descriptor.elements.size()),
                        expression.operands[1]->span);
      type = descriptor.elements[index];
    }
    else
    {
      if (descriptor.kind != TypeKind::DynamicArray && descriptor.kind != TypeKind::FixedArray &&
          descriptor.kind != TypeKind::DependentArray)
        throw TypeError(std::format("cannot index value of type {}", interner_.display(receiver)),
                        expression.span);
      type = descriptor.element;
    }
    break;
  }
  case hir::ExpressionKind::Closure:
  {
    std::vector<TypeId> parameterTypes;
    parameterTypes.reserve(expression.closureParameterIds.size());
    for (size_t index = 0; index < expression.closureParameterIds.size(); ++index)
    {
      if (expression.closureParameterTypes[index] == nullptr)
        throw TypeError(
            std::format("cannot infer closure parameter `{}`; annotate it or the expected closure type",
                        expression.closureParameterNames[index]),
            expression.span);
      const TypeId parameterType =
          interner_.resolveInScope(*expression.closureParameterTypes[index], genericBindings_);
      parameterTypes.push_back(parameterType);
    }
    type = inferClosure(expression, parameterTypes, locals);
    break;
  }
  case hir::ExpressionKind::Member:
  {
    const TypeId receiver = infer(*expression.operands[0], locals);
    // D-002 A1.2: member-through-ref — `s.field` on `S ref` lowers through the
    // ref object like `(*s).field`, so the checker resolves the member on the
    // referenced struct type.
    TypeId base = receiver;
    bool throughRef = false;
    if (interner_.descriptor(receiver).kind == TypeKind::Reference)
    {
      base = interner_.descriptor(receiver).element;
      throughRef = true;
    }
    const auto &descriptor = interner_.descriptor(base);
    if (descriptor.kind != TypeKind::Struct)
      throw TypeError(std::format("cannot access member `{}` on value of type {}", expression.text,
                                  interner_.display(receiver)),
                      expression.span);
    if (const auto root = rootLocalOf(expression); root.has_value())
    {
      const auto fieldNames = descriptor.fieldNames;
      const auto found = std::find(fieldNames.begin(), fieldNames.end(), expression.text);
      if (found != fieldNames.end())
      {
        const uint32_t field = static_cast<uint32_t>(std::distance(fieldNames.begin(), found));
        if (moveState_.isFieldMoved(root->value, field))
          throw TypeError(
              "E-move-field",
              std::format("use of moved field `{}.{}`", expression.operands[0]->text, expression.text),
              expression.span);
      }
    }
    const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.text);
    if (found == descriptor.fieldNames.end())
      throw TypeError(std::format("unknown field `{}` in struct `{}`", expression.text, descriptor.name),
                      expression.span);
    type = descriptor.elements[static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found))];
    break;
  }
  }
  record(expression, type);
  return type;
}

auto Checker::inferCallClosureArgument(const hir::Expression &closure, TypeId parameterType, size_t index,
                                       const LocalTypes &locals, Substitution &substitution,
                                       std::vector<std::pair<size_t, hir::DefId>> &closureTargets) -> TypeId
{
  const TypeId specialized = specialize(parameterType, substitution);
  const auto &closureDescriptor = interner_.descriptor(specialized);
  if (closureDescriptor.kind != TypeKind::Closure)
    throw TypeError(std::format("closure argument is not applicable to parameter of type {}",
                                interner_.display(specialized)),
                    closure.span);
  if (closure.closureParameterIds.size() != closureDescriptor.elements.size())
    throw TypeError(std::format("closure argument count mismatch: expected {}, got {}",
                                closureDescriptor.elements.size(), closure.closureParameterIds.size()),
                    closure.span);
  std::vector<TypeId> closureParameterTypes;
  closureParameterTypes.reserve(closureDescriptor.elements.size());
  for (const auto parameter : closureDescriptor.elements)
    closureParameterTypes.push_back(specialize(parameter, substitution));
  const TypeId closureType = inferClosure(closure, closureParameterTypes, locals);
  record(closure, closureType);
  unify(parameterType, closureType, substitution, closure.span);
  // INC-14 S4: a generic closure has no code function (deferred until the
  // enclosing generic is instantiated); the model call types from the
  // descriptor and the not-concrete instantiation is a model no-op.
  if (!deferredClosures_.contains(&closure))
    closureTargets.emplace_back(index, closureFunctions_.at(&closure));
  return closureType;
}

auto Checker::inferClosure(const hir::Expression &closure, const std::vector<TypeId> &parameterTypes,
                            const LocalTypes &locals) -> TypeId
{
  LocalTypes closureLocals;
  std::vector<TypeId> captureTypes;
  // INC-14 S3: resolve explicit capture list. The closure body references the
  // fresh capture params (closureCaptureParams), typed from the enclosing
  // locals; the original capture locals (closureCaptures) drive call-site
  // threading.
  validateCaptures(closure);
  for (size_t index = 0; index < closure.closureCaptures.size(); ++index)
  {
    const auto &capture = closure.closureCaptures[index];
    const auto found = locals.find(capture.local.value);
    if (found == locals.end())
      throw TypeError(std::format("capture is not in scope"), closure.span);
    const TypeId captureType = found->second;
    captureTypes.push_back(captureType);
    closureLocals.emplace(closure.closureCaptureParams[index].value, captureType);
    recordLocal(closure.closureCaptureParams[index], captureType);
  }
  for (size_t index = 0; index < closure.closureParameterIds.size(); ++index)
  {
    closureLocals.emplace(closure.closureParameterIds[index].value, parameterTypes[index]);
    recordLocal(closure.closureParameterIds[index], parameterTypes[index]);
  }
  const MoveState movesBefore = moveState_;
  checkBlock(*closure.closureBody, closureLocals, {}, TypeId{}, false);
  moveState_ = movesBefore;
  const TypeId resultType = closure.closureBody->tailExpression != nullptr
                                ? expressionTypeIds_.at(closure.closureBody->tailExpression.get())
                                : builtin::Unit;
  const hir::DefId closureId{static_cast<uint32_t>(module_->functions.size() + instances_.size())};
  const std::string closureName = std::format("__closure{}", closureCounter_++);
  // Code function signature: capture params (owned, same type) + user params.
  std::vector<TypeId> codeParamTypes;
  codeParamTypes.reserve(captureTypes.size() + parameterTypes.size());
  for (const auto captureType : captureTypes) codeParamTypes.push_back(captureType);
  for (const auto paramType : parameterTypes) codeParamTypes.push_back(paramType);
  // INC-14 S4: generic closure — a code function whose signature names type
  // parameters cannot be lowered or called. Model-typed only; the concrete
  // code function is synthesized when the enclosing generic is instantiated
  // and the closure re-inferred with concrete types.
  const bool genericClosure = interner_.descriptor(resultType).kind == TypeKind::TypeParameter ||
                              std::any_of(codeParamTypes.begin(), codeParamTypes.end(), [&](TypeId type) {
                                return interner_.descriptor(type).kind == TypeKind::TypeParameter;
                              });
  if (genericClosure)
  {
    deferredClosures_.insert(&closure);
    return interner_.internClosure(parameterTypes, resultType);
  }
  const FunctionTypeIds closureSignature{.parameters = codeParamTypes, .returnType = resultType};
  signatures_.emplace(closureId.value, closureSignature);
  functionTypeIds_.emplace(closureId.value, closureSignature);
  FunctionType displaySignature;
  for (const auto param : codeParamTypes)
    displaySignature.parameters.push_back(interner_.display(param));
  displaySignature.returnType = interner_.display(resultType);
  functionTypes_.emplace(closureId.value, std::move(displaySignature));
  instances_.push_back(hir::makeClosureFunction(closure, closureName, closureId, nextInstanceLocal_));
  checkFunction(instances_.back(), {});
  closureFunctions_.emplace(&closure, closureId);
  // INC-14 S3: move captures consume the original enclosing local at the
  // literal site. Applied AFTER checkFunction because checkFunction resets
  // moveState_ to empty (clobbering any mark made before it); the closure body
  // was already checked against the capture params above.
  for (const auto &capture : closure.closureCaptures)
    if (capture.isMove)
      moveState_.markWhole(capture.local.value);
  // Record capture info for call-site threading.
  closureCaptures_[&closure] = captureTypes;
  closureCaptureTypesByFunction_[closureId.value] = captureTypes;
  return interner_.internClosure(parameterTypes, resultType);
}

} // namespace NG::typecheck::detail
