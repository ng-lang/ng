// AI-generated code; reviewed for this repository's vNext rewrite.
// typecheck_expected.cpp — checking-mode inference: an expression checked
// against an expected type (literal adoption, union/trait-view coercion,
// expected aggregates, closures, and return-type-seeded generic calls).
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

  auto Checker::inferExpected(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                              std::string_view context) -> TypeId
  {
    const auto &descriptor = interner_.descriptor(expected);
    const auto adopt = [&]
    {
      record(expression, expected);
      return expected;
    };
    // A union-typed local narrows to the expected member type.
    if (descriptor.kind != TypeKind::Union && expression.kind == hir::ExpressionKind::ResolvedName)
    {
      const auto &value = interner_.descriptor(infer(expression, locals));
      if (value.kind == TypeKind::Union && std::ranges::find(value.elements, expected) != value.elements.end())
        return adopt();
    }
    if (descriptor.kind == TypeKind::Union)
      return inferExpectedUnion(expression, expected, locals);
    if (descriptor.kind == TypeKind::TraitReference)
      return inferExpectedTraitView(expression, expected, locals, context);

    switch (expression.kind)
    {
    case hir::ExpressionKind::IntegerLiteral:
    case hir::ExpressionKind::FloatLiteral:
    case hir::ExpressionKind::Prefix:
      if (const auto adopted = adoptNumericLiteral(expression, expected, locals); adopted.has_value())
        return *adopted;
      break;
    case hir::ExpressionKind::ArrayLiteral:
      if (descriptor.kind == TypeKind::Enum)
      {
        if (const auto list = inferExpectedList(expression, expected, locals); list.has_value())
          return *list;
      }
      else if (isArrayKind(descriptor.kind))
      {
        if (descriptor.kind == TypeKind::FixedArray && expression.operands.size() != *descriptor.length)
          throw TypeError(std::format("fixed array length mismatch: expected {}, got {}", *descriptor.length,
                                      expression.operands.size()),
                          expression.span);
        for (const auto &element : expression.operands)
          static_cast<void>(inferExpected(*element, descriptor.element, locals, "array element"));
        return adopt();
      }
      break;
    case hir::ExpressionKind::Binary:
      if (expression.text == ".." && descriptor.kind == TypeKind::Range)
      {
        static_cast<void>(inferExpected(*expression.operands[0], descriptor.element, locals, "range start"));
        static_cast<void>(inferExpected(*expression.operands[1], descriptor.element, locals, "range end"));
        return adopt();
      }
      break;
    case hir::ExpressionKind::EnumLiteral:
      if (descriptor.kind == TypeKind::Enum)
        return inferExpectedEnum(expression, expected, locals);
      break;
    case hir::ExpressionKind::Call:
      if (const auto call = inferExpectedCall(expression, expected, locals, context); call.has_value())
        return *call;
      break;
    case hir::ExpressionKind::StructLiteral:
      if (descriptor.kind == TypeKind::Struct)
        return inferExpectedStruct(expression, expected, locals);
      break;
    case hir::ExpressionKind::TupleLiteral:
      if (descriptor.kind == TypeKind::Tuple)
        return inferExpectedTuple(expression, expected, locals);
      break;
    case hir::ExpressionKind::Closure:
      if (descriptor.kind == TypeKind::Closure)
        return inferExpectedClosure(expression, expected, locals, context);
      break;
    default:
      break;
    }
    const TypeId actual = infer(expression, locals);
    requireType(expected, actual, expression.span, context);
    return actual;
  }

  auto Checker::adoptNumericLiteral(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
      -> std::optional<TypeId>
  {
    const auto requireSuffix = [&](const hir::Expression &literal, std::string_view family)
    {
      if (!literal.numericSuffix.empty() && typeFromNumericSuffix(literal.numericSuffix) != expected)
        throw TypeError(std::format("{} literal suffix `{}` conflicts with expected type {}", family,
                                    literal.numericSuffix, interner_.display(expected)),
                        expression.span);
    };
    const bool integerExpected = isIntegerBuiltin(expected);
    const bool floatExpected = isFloatBuiltin(expected);
    switch (expression.kind)
    {
    case hir::ExpressionKind::IntegerLiteral:
      if (integerExpected)
      {
        requireSuffix(expression, "integer");
        checkIntegerLiteralMagnitude(parseIntegerLiteralMagnitude(expression.text, expected, expression.span),
                                     expression.text, expected, expression.span);
        break;
      }
      // DEC-A: an unsuffixed integer literal adopts a float context.
      if (floatExpected && expression.numericSuffix.empty())
        break;
      return std::nullopt;
    case hir::ExpressionKind::FloatLiteral:
      if (!floatExpected)
        return std::nullopt;
      requireSuffix(expression, "float");
      break;
    case hir::ExpressionKind::Prefix:
    {
      if (expression.text != "-" && expression.text != "+")
        return std::nullopt;
      const auto &literal = *expression.operands[0];
      if (floatExpected && literal.kind == hir::ExpressionKind::FloatLiteral)
      {
        static_cast<void>(inferExpected(literal, expected, locals, "float literal"));
        break;
      }
      if (!integerExpected || literal.kind != hir::ExpressionKind::IntegerLiteral)
        return std::nullopt;
      const std::string text = std::format("{}{}", expression.text, literal.text);
      requireSuffix(literal, "integer");
      record(literal, expected);
      const uint64_t magnitude = parseIntegerLiteralMagnitude(literal.text, expected, expression.span);
      if (expression.text == "+")
      {
        checkIntegerLiteralMagnitude(magnitude, text, expected, expression.span);
        break;
      }
      if (!isSignedIntegerBuiltin(expected))
        throw TypeError(
            std::format("integer literal `-{}` is out of range for type {}", literal.text, interner_.display(expected)),
            expression.span);
      constexpr uint64_t MaxMagnitude = 1ULL << 63;
      if (magnitude > MaxMagnitude)
        throw TypeError(
            std::format("integer literal `{}` is out of range for type {}", text, interner_.display(expected)),
            expression.span);
      checkIntegerLiteral(magnitude == MaxMagnitude ? std::numeric_limits<int64_t>::min()
                                                    : -static_cast<int64_t>(magnitude),
                          text, expected, expression.span);
      break;
    }
    default:
      return std::nullopt;
    }
    record(expression, expected);
    return expected;
  }

  /// `[a, b, c]` against a recursive `List<T>`-shaped enum (a Nil variant plus
  /// a Cons variant); other enums fall back to synthesis.
  auto Checker::inferExpectedList(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
      -> std::optional<TypeId>
  {
    const auto &descriptor = interner_.descriptor(expected);
    const bool hasNil = std::ranges::find(descriptor.variantHasPayload, false) != descriptor.variantHasPayload.end();
    const auto element = listElementType(expected);
    if (!hasNil || !element.has_value())
      return std::nullopt;
    for (const auto &candidate : expression.operands)
    {
      if (isSpread(*candidate))
        throw TypeError("list collection literals do not support spreads", candidate->span);
      static_cast<void>(inferExpected(*candidate, *element, locals, "list element"));
    }
    record(expression, expected);
    return expected;
  }

  auto Checker::inferExpectedUnion(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
      -> TypeId
  {
    const TypeId valueType = infer(expression, locals);
    const auto &members = interner_.descriptor(expected).elements;
    if (valueType != expected && std::ranges::find(members, valueType) == members.end())
      throw TypeError(std::format("value of type {} is not a member of union {}", interner_.display(valueType),
                                  interner_.display(expected)),
                      expression.span);
    record(expression, expected);
    return expected;
  }

  /// A value or reference of a concrete type implementing the trait coerces
  /// to a `ref<Trait>` view (recorded for lowering as MakeTraitView).
  auto Checker::inferExpectedTraitView(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                       std::string_view context) -> TypeId
  {
    const std::string traitName = interner_.descriptor(expected).name;
    const TypeId valueType = infer(expression, locals);
    const auto &value = interner_.descriptor(valueType);
    if (value.kind == TypeKind::TraitReference)
    {
      requireType(expected, valueType, expression.span, context);
      record(expression, expected);
      return expected;
    }
    const TypeId concrete = value.kind == TypeKind::Reference ? value.element : valueType;
    const auto concreteKind = interner_.descriptor(concrete).kind;
    if (concreteKind == TypeKind::TraitReference || concreteKind == TypeKind::Trait)
      throw TypeError(
          std::format("cannot build a `{}` view from {}", interner_.display(expected), interner_.display(valueType)),
          expression.span);
    if (!hasImpl(traitName, concrete) || !ensureTraitViewTable(traitName, concrete))
      throw TypeError(
          std::format("value of type {} does not implement trait `{}`", interner_.display(concrete), traitName),
          expression.span);
    traitViewCoercions_.insert_or_assign(&expression, std::pair<std::string, TypeId>{traitName, concrete});
    record(expression, expected);
    return expected;
  }

  auto Checker::inferExpectedEnum(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                  bool consumePayloads) -> TypeId
  {
    const auto &descriptor = interner_.descriptor(expected);
    if (!expression.enumId.has_value() || descriptor.nominalId != expression.enumId->value)
      throw TypeError(std::format("enum constructor type mismatch: expected {}, got {}", interner_.display(expected),
                                  expression.text),
                      expression.span);
    const uint32_t variant = expression.variant.value();
    if (variant >= descriptor.elements.size())
      throw TypeError("enum variant is out of range", expression.span);
    // Multi-field variants take one argument per tuple field.
    std::vector<TypeId> payloads;
    if (descriptor.variantHasPayload[variant])
    {
      const auto &payload = interner_.descriptor(descriptor.elements[variant]);
      if (payload.kind == TypeKind::Tuple)
        payloads = payload.elements;
      else
        payloads = {descriptor.elements[variant]};
    }
    if (expression.operands.size() != payloads.size())
      throw TypeError(std::format("enum variant `{}` expects {} payload values, got {}", descriptor.fieldNames[variant],
                                  payloads.size(), expression.operands.size()),
                      expression.span);
    for (size_t index = 0; index < payloads.size(); ++index)
    {
      static_cast<void>(inferExpected(*expression.operands[index], payloads[index], locals, "variant payload"));
      if (consumePayloads)
        trackConsumption(*expression.operands[index], locals);
    }
    record(expression, expected);
    return expected;
  }

  auto Checker::inferExpectedStruct(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
      -> TypeId
  {
    const auto &descriptor = interner_.descriptor(expected);
    if (!expression.structId.has_value() || descriptor.nominalId != expression.structId->value)
      throw TypeError(std::format("struct literal type mismatch: expected {}, got {}", interner_.display(expected),
                                  expression.text),
                      expression.span);
    std::vector<bool> seen(descriptor.fieldNames.size());
    for (size_t index = 0; index < expression.operands.size(); ++index)
    {
      const auto &name = expression.memberNames[index];
      const auto found = std::ranges::find(descriptor.fieldNames, name);
      if (found == descriptor.fieldNames.end())
        throw TypeError(std::format("unknown field `{}` in struct `{}`", name, descriptor.name), expression.span);
      const auto field = static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found));
      if (seen[field])
        throw TypeError(std::format("duplicate field `{}` in struct literal", name), expression.span);
      seen[field] = true;
      const TypeId fieldType = inferExpected(*expression.operands[index], descriptor.elements[field], locals,
                                             std::format("field `{}`", name));
      trackConsumption(*expression.operands[index], locals);
      rejectScopedView(fieldType, expression.operands[index]->span, "struct fields");
    }
    if (std::ranges::find(seen, false) != seen.end())
      throw TypeError(std::format("missing field in struct `{}`", descriptor.name), expression.span);
    record(expression, expected);
    return expected;
  }

  /// Tuple literals against an expected tuple: element-wise checks, with
  /// spreads splicing tuple operands (or absorbing type-pack slots).
  auto Checker::inferExpectedTuple(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
      -> TypeId
  {
    const auto &slots = interner_.descriptor(expected).elements;
    const auto lengthMismatch = [&](size_t got)
    {
      return TypeError(std::format("tuple length mismatch: expected {}, got {}", slots.size(), got), expression.span);
    };
    size_t slot = 0;
    for (const auto &element : expression.operands)
    {
      if (!isSpread(*element))
      {
        if (slot >= slots.size())
          throw lengthMismatch(expression.operands.size());
        static_cast<void>(inferExpected(*element, slots[slot], locals, std::format("tuple element {}", slot + 1)));
        ++slot;
        continue;
      }
      const TypeId operand = infer(*element->operands[0], locals);
      const auto &spread = interner_.descriptor(operand);
      if (slot < slots.size() && interner_.descriptor(slots[slot]).kind == TypeKind::TypePack)
      {
        ++slot;
        continue;
      }
      if (spread.kind == TypeKind::TypePack)
      {
        slot = slots.size();
        continue;
      }
      if (spread.kind != TypeKind::Tuple)
        throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)), element->span);
      if (slot + spread.elements.size() > slots.size())
        throw TypeError("tuple splice exceeds the expected tuple length", element->span);
      for (const auto spliced : spread.elements)
        requireType(slots[slot++], spliced, element->span, "tuple splice element");
    }
    if (slot != slots.size())
      throw lengthMismatch(slot);
    record(expression, expected);
    return expected;
  }

  /// Unannotated closure parameters take their types from the expected
  /// closure type (INC-14 S2).
  auto Checker::inferExpectedClosure(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                     std::string_view context) -> TypeId
  {
    const auto &descriptor = interner_.descriptor(expected);
    if (expression.closureParameterIds.size() != descriptor.elements.size())
      throw TypeError(std::format("closure arity mismatch: expected {}, got {}", descriptor.elements.size(),
                                  expression.closureParameterIds.size()),
                      expression.span);
    std::vector<TypeId> parameterTypes;
    parameterTypes.reserve(descriptor.elements.size());
    for (size_t index = 0; index < descriptor.elements.size(); ++index)
    {
      const auto &annotation = expression.closureParameterTypes[index];
      parameterTypes.push_back(annotation == nullptr ? descriptor.elements[index]
                                                     : interner_.resolveInScope(*annotation, genericBindings_));
    }
    const TypeId actual = inferClosure(expression, parameterTypes, locals);
    requireType(expected, actual, expression.span, context);
    record(expression, actual);
    return actual;
  }

  auto Checker::inferExpectedCall(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                  std::string_view context) -> std::optional<TypeId>
  {
    if (expression.operands.empty() || !expression.operands[0]->resolvedName.has_value() ||
        expression.operands[0]->resolvedName->kind != hir::ResolvedNameKind::Function)
      return std::nullopt;
    const hir::DefId selected = selectOverload(expression, locals, false);
    if (const FoldInfo fold = analyzeFoldCall(expression, locals); fold.fold)
    {
      const TypeId result = inferFoldCall(expression, fold, selected, locals);
      requireType(expected, result, expression.span, context);
      record(expression, result);
      return result;
    }
    // Tuple-spread arguments and non-generic callees check in synthesis mode.
    const bool anySpread =
        std::ranges::any_of(expression.operands, [](const auto &operand) { return isSpread(*operand); });
    if (anySpread || !isGeneric(signatures_.at(selected.value)))
      return std::nullopt;
    return inferExpectedGenericCall(expression, selected, expected, locals, context);
  }

  /// A generic call under an expected type: the expected type seeds the
  /// substitution through the return type (unless explicit generic arguments
  /// are given), then arguments unify against their parameters.
  auto Checker::inferExpectedGenericCall(const hir::Expression &expression, hir::DefId selected, TypeId expected,
                                         const LocalTypes &locals, std::string_view context) -> TypeId
  {
    const auto &signature = signatures_.at(selected.value);
    const size_t supplied = expression.operands.size() - 1;
    const bool variadic = !signature.packParameters.empty();
    const size_t fixedParameters = fixedParameterCount(signature);
    if (variadic ? supplied < fixedParameters : supplied != signature.parameters.size())
      throw TypeError(
          std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied),
          expression.span);
    const size_t packCount = variadic ? supplied - fixedParameters : 0;
    const auto argumentAt = [&](size_t index) -> const hir::Expression & { return *expression.operands[index + 1]; };
    Substitution substitution;
    std::vector<std::pair<size_t, hir::DefId>> closureTargets;
    std::vector<size_t> enumLiteralArguments;
    if (!expression.genericArguments.empty())
    {
      substitution = explicitSubstitution(expression, signature);
      for (size_t index = 0; index < supplied; ++index)
        static_cast<void>(inferExpected(argumentAt(index),
                                        specialize(callParameterType(signature, index), substitution), locals,
                                        std::format("call argument {}", index + 1)));
    }
    else
    {
      unify(signature.returnType, expected, substitution, expression.span);
      for (size_t index = 0; index < supplied; ++index)
      {
        const auto &argument = argumentAt(index);
        const TypeId parameterType = callParameterType(signature, index);
        const bool fixedSlot = index < fixedParameters;
        if (fixedSlot && argument.kind == hir::ExpressionKind::Closure)
        {
          static_cast<void>(
              inferCallClosureArgument(argument, parameterType, index, locals, substitution, closureTargets));
          std::vector<uint32_t> captures;
          for (const auto &capture : argument.closureCaptures)
            captures.push_back(capture.local.value);
          threadCallCaptures(expression, captures);
          continue;
        }
        // INC-14 S3/S4: a bound closure local forwarded into a higher-order
        // call specializes the callee with the forwarding closure.
        if (fixedSlot && interner_.descriptor(parameterType).kind == TypeKind::Closure &&
            argument.resolvedName.has_value() && argument.resolvedName->kind == hir::ResolvedNameKind::Local &&
            closureLocals_.contains(argument.resolvedName->id) &&
            bindForwardedClosure(expression, argument, infer(argument, locals), index, closureTargets))
          continue;
        if (variadic && !fixedSlot)
        {
          bindPackArgument(substitution, parameterType, index - fixedParameters, infer(argument, locals),
                           argument.span);
          continue;
        }
        if (argument.kind == hir::ExpressionKind::EnumLiteral &&
            interner_.descriptor(parameterType).kind == TypeKind::Enum)
          enumLiteralArguments.push_back(index);
        unifyGenericArgument(argument, parameterType, substitution, locals);
      }
    }
    const TypeId specialized = specializeReturnType(signature, substitution, packCount);
    // Generic enum constructor arguments unify through their payloads only;
    // record their specialized type so lowering can build the enum value.
    for (const size_t index : enumLiteralArguments)
      record(argumentAt(index), specialize(callParameterType(signature, index), substitution));
    requireConstArguments(signature, substitution, expression.operands[0]->text, expression.span);
    if (isConcrete(substitution))
      requireWhereClause(selected, substitution, signature, expression.span);
    trackCallConsumption(expression, selected, signature, supplied, nullptr, locals);
    if (variadic)
      recordVariadicPack(expression, packCount, fixedParameters, supplied,
                         [&](size_t index) { return infer(argumentAt(index), locals); });
    const hir::DefId instance = instantiateFunction(selected, substitution, packCount, expression.span, closureTargets);
    requireType(expected, specialized, expression.span, context);
    record(expression, specialized);
    callTargets_.insert_or_assign(&expression, instance);
    return specialized;
  }

} // namespace NG::typecheck::detail
