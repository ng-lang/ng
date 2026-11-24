// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck_const.cpp — const evaluation, patterns, introspection.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

auto Checker::evaluateWhereCondition(const hir::Expression &condition, const Substitution &substitution,
                                      const FunctionTypeIds &signature) -> bool
{
  std::unordered_map<std::string, TypeId> bindings;
  for (size_t index = 0; index < signature.genericParameters.size(); ++index)
  {
    const TypeId parameter = signature.genericParameters[index];
    const auto found = substitution.types.find(parameter.value);
    bindings.emplace(signature.genericParameterNames[index],
                     found != substitution.types.end() ? found->second : parameter);
  }
  const_eval::ConstBindings constBindings;
  for (size_t index = 0; index < signature.constParameters.size(); ++index)
  {
    const auto found = substitution.consts.find(static_cast<uint32_t>(index));
    if (found != substitution.consts.end())
      constBindings.emplace(signature.constParameterNames[index], found->second);
  }
  const auto evaluate = [&](const auto &self, const hir::Expression &expression) -> const_eval::ConstValueId
  {
    switch (expression.kind)
    {
    case hir::ExpressionKind::BooleanLiteral:
      return interner_.constInterner().internBool(expression.text == "true");
    case hir::ExpressionKind::IntegerLiteral:
    {
      const uint64_t magnitude = parseIntegerLiteralMagnitude(expression.text, builtin::I64, expression.span);
      if (magnitude > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        throw TypeError(std::format("integer literal `{}` is out of range", expression.text), expression.span);
      return interner_.constInterner().internInteger(static_cast<int64_t>(magnitude));
    }
    case hir::ExpressionKind::Grouped:
      return self(self, *expression.operands[0]);
    case hir::ExpressionKind::Prefix:
      if (expression.text == "!")
        return interner_.constInterner().internBool(
            !interner_.constInterner().value(self(self, *expression.operands[0])).boolValue);
      throw TypeError(std::format("unsupported where clause operator `{}`", expression.text), expression.span);
    case hir::ExpressionKind::Binary:
    {
      const std::string &op = expression.text;
      if (op == "&&")
      {
        if (!interner_.constInterner().value(self(self, *expression.operands[0])).boolValue)
          return interner_.constInterner().internBool(false);
        return interner_.constInterner().internBool(
            interner_.constInterner().value(self(self, *expression.operands[1])).boolValue);
      }
      if (op == "||")
      {
        if (interner_.constInterner().value(self(self, *expression.operands[0])).boolValue)
          return interner_.constInterner().internBool(true);
        return interner_.constInterner().internBool(
            interner_.constInterner().value(self(self, *expression.operands[1])).boolValue);
      }
      throw TypeError(std::format("unsupported where clause operator `{}`", op), expression.span);
    }
    case hir::ExpressionKind::GenericApplication:
    {
      if (const auto builtin =
              evaluateTraitIntrospection(expression.text, expression.genericArguments, expression.span);
          builtin.has_value())
        return *builtin;
      std::vector<TypeId> typeArguments;
      for (const auto &argument : expression.genericArguments)
      {
        if (argument.kind == syntax::GenericArgumentKind::Type)
          typeArguments.push_back(interner_.resolveInScope(*argument.type, bindings));
        else
          typeArguments.push_back(builtin::Unit);
      }
      for (const auto &argument : typeArguments)
        if (interner_.descriptor(argument).kind == TypeKind::TypeParameter)
          throw TypeError(
              std::format("cannot evaluate where clause of generic function with abstract type parameter `{}`",
                          interner_.display(argument)),
              expression.span);
      if (const auto builtin = evaluateTupleIntrospection(expression.text, typeArguments, expression.span);
          builtin.has_value())
      {
        if (interner_.constInterner().value(*builtin).kind != const_eval::ConstValueKind::Bool)
          throw TypeError(
              std::format("const predicate `{}` must evaluate to bool in predicate position", expression.text),
              expression.span);
        return *builtin;
      }
      const auto *selected = selectConstDeclaration(expression.text, typeArguments, expression.span);
      const const_eval::ConstValueId value = evaluateConstDeclaration(*selected, expression.span);
      if (interner_.constInterner().value(value).kind != const_eval::ConstValueKind::Bool)
        throw TypeError(
            std::format("const declaration `{}` must evaluate to bool in predicate position", expression.text),
            expression.span);
      return value;
    }
    case hir::ExpressionKind::Call:
      return evaluateConstCall(expression, bindings, constBindings);
    case hir::ExpressionKind::TypeTest:
    {
      const auto found = bindings.find(expression.text);
      if (found == bindings.end())
        throw TypeError(std::format("where clause tests unknown type parameter `{}`", expression.text),
                        expression.span);
      const TypeId actual = found->second;
      if (interner_.descriptor(actual).kind == TypeKind::TypeParameter)
        throw TypeError(
            std::format("cannot evaluate where clause of generic function with abstract type parameter `{}`",
                        interner_.display(actual)),
            expression.span);
      const TypeId expected = interner_.resolveInScope(*expression.testedType, bindings);
      return interner_.constInterner().internBool(actual == expected);
    }
    case hir::ExpressionKind::TraitBound:
    {
      const auto found = bindings.find(expression.text);
      if (found == bindings.end())
        throw TypeError(std::format("where clause tests unknown type parameter `{}`", expression.text),
                        expression.span);
      const TypeId actual = found->second;
      if (interner_.descriptor(actual).kind == TypeKind::TypeParameter)
        throw TypeError(
            std::format("cannot evaluate where clause of generic function with abstract type parameter `{}`",
                        interner_.display(actual)),
            expression.span);
      for (const auto &traitName : expression.traitNames)
      {
        if (!traits_.contains(traitName))
          throw TypeError(std::format("unknown trait `{}` in where clause", traitName), expression.span);
        if (!hasImpl(traitName, actual))
          return interner_.constInterner().internBool(false);
      }
      return interner_.constInterner().internBool(true);
    }
    case hir::ExpressionKind::ResolvedName:
      if (expression.resolvedName.has_value() &&
          expression.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
      {
        const auto found = constBindings.find(expression.text);
        if (found == constBindings.end())
          throw TypeError(std::format("unresolved const parameter `{}` in where clause", expression.text),
                          expression.span);
        return found->second;
      }
      throw TypeError(std::format("`{}` is not a compile-time constant in a where clause", expression.text),
                      expression.span);
    default:
      throw TypeError("unsupported where clause constraint", expression.span);
    }
  };
  return interner_.constInterner().value(evaluate(evaluate, condition)).boolValue;
}

auto Checker::evaluateConstCall(const hir::Expression &call,
                                const std::unordered_map<std::string, TypeId> &bindings,
                                const const_eval::ConstBindings &constBindings) -> const_eval::ConstValueId
{
  if (!call.operands.empty() && call.operands[0]->resolvedName.has_value() &&
      call.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function)
  {
    if (call.genericArguments.empty())
    {
      if (const auto recorded = callTargets_.find(&call);
          recorded != callTargets_.end() && recorded->second.value != call.operands[0]->resolvedName->id)
        return interpreter_->evaluateCall(call, {}, constBindings, call.span, recorded->second);
    }
    if (call.genericArguments.empty())
    {
      return interpreter_->evaluateCall(call, {}, constBindings, call.span);
    }
    const hir::DefId target{call.operands[0]->resolvedName->id};
    const auto &function = module_->functions.at(target.value);
    const auto &signature = signatures_.at(target.value);
    if (!signature.genericParameters.empty())
    {
      std::vector<TypeId> typeArguments;
      if (call.genericArguments.size() != signature.explicitParameterOrder.size())
        throw TypeError(std::format("generic argument count mismatch: expected {}, got {}",
                                    signature.explicitParameterOrder.size(), call.genericArguments.size()),
                        call.span);
      for (size_t index = 0; index < call.genericArguments.size(); ++index)
      {
        if (signature.explicitParameterOrder[index] != syntax::GenericParameterKind::Type)
          throw TypeError("const generic call arguments must be types", call.span);
        if (call.genericArguments[index].kind != syntax::GenericArgumentKind::Type)
          throw TypeError("const generic call arguments must be types", call.span);
        typeArguments.push_back(interner_.resolveInScope(*call.genericArguments[index].type, bindings));
      }
      if (typeArguments.size() != signature.genericParameters.size())
        throw TypeError(std::format("const generic call to `{}` has {} type argument(s), expected {}",
                                    function.name, typeArguments.size(), signature.genericParameters.size()),
                        call.span);
      Substitution substitution;
      bool concrete = true;
      for (size_t index = 0; index < typeArguments.size(); ++index)
      {
        substitution.types.emplace(signature.genericParameters[index].value, typeArguments[index]);
        if (interner_.descriptor(typeArguments[index]).kind == TypeKind::TypeParameter)
          concrete = false;
      }
      if (concrete)
      {
        if (function.whereClause != nullptr &&
            !evaluateWhereCondition(*function.whereClause, substitution, signature))
          throw TypeError(std::format("call to `{}` does not satisfy its where clause", function.name),
                          call.span);
        const hir::DefId instance = instantiateFunction(target, substitution, 0, call.span);
        return interpreter_->evaluateCall(call, {}, constBindings, call.span, instance);
      }
    }
  }
  return interpreter_->evaluateCall(call, {}, constBindings, call.span);
}

void Checker::checkConstDeclaration(const hir::ConstDeclaration &declaration)
{
  ConstDeclChecked checked{.declaration = &declaration};
  std::unordered_map<std::string, TypeId> bindings;
  for (size_t index = 0; index < declaration.typeParameters.size(); ++index)
  {
    const TypeId parameter =
        interner_.internTypeParameter(declaration.typeParameters[index], static_cast<uint32_t>(index));
    checked.parameters.push_back(parameter);
    bindings.emplace(declaration.typeParameters[index], parameter);
  }
  for (const auto &pattern : declaration.pattern)
    checked.pattern.push_back(interner_.resolveInScope(*pattern, bindings));
  checked.targetType =
      declaration.targetType != nullptr ? interner_.resolve(*declaration.targetType) : builtin::Bool;
  if (checked.targetType != builtin::Bool && checked.targetType != builtin::I64)
    throw TypeError(std::format("const declaration `{}` must declare bool or i64, got {}", declaration.name,
                                interner_.display(checked.targetType)),
                    declaration.span);
  constDeclarations_[declaration.name].push_back(std::move(checked));
}

auto Checker::matchConstPatternType(TypeId pattern, TypeId actual, PatternMatchState &state) const -> bool
{
  const auto &patternDescriptor = interner_.descriptor(pattern);
  const auto &actualDescriptor = interner_.descriptor(actual);
  if (actualDescriptor.kind == TypeKind::TypeParameter)
    return false;
  switch (patternDescriptor.kind)
  {
  case TypeKind::TypeParameter:
  {
    state.containsParameter = true;
    const uint32_t parameterIndex = *patternDescriptor.nominalId;
    const auto existing = state.bindings.find(parameterIndex);
    if (existing != state.bindings.end())
    {
      if (existing->second != actual)
        return false;
      state.repeated = true;
      return true;
    }
    state.bindings.emplace(parameterIndex, actual);
    return true;
  }
  case TypeKind::Builtin:
  case TypeKind::DependentArray:
  case TypeKind::Opaque:
    return pattern == actual;
  case TypeKind::Reference:
  case TypeKind::Span:
  case TypeKind::RawPointer:
    if (actualDescriptor.kind != patternDescriptor.kind ||
        actualDescriptor.referenceMutable != patternDescriptor.referenceMutable)
      return false;
    return matchConstPatternType(patternDescriptor.element, actualDescriptor.element, state);
  case TypeKind::DynamicArray:
    if (actualDescriptor.kind != TypeKind::DynamicArray)
      return false;
    return matchConstPatternType(patternDescriptor.element, actualDescriptor.element, state);
  case TypeKind::FixedArray:
    if (actualDescriptor.kind != TypeKind::FixedArray || *patternDescriptor.length != *actualDescriptor.length)
      return false;
    return matchConstPatternType(patternDescriptor.element, actualDescriptor.element, state);
  case TypeKind::Tuple:
  {
    if (actualDescriptor.kind != TypeKind::Tuple ||
        patternDescriptor.elements.size() != actualDescriptor.elements.size())
      return false;
    for (size_t index = 0; index < patternDescriptor.elements.size(); ++index)
      if (!matchConstPatternType(patternDescriptor.elements[index], actualDescriptor.elements[index], state))
        return false;
    return true;
  }
  case TypeKind::Struct:
  case TypeKind::Enum:
  {
    if (actualDescriptor.kind != patternDescriptor.kind ||
        *patternDescriptor.nominalId != *actualDescriptor.nominalId)
      return false;
    for (size_t index = 0; index < patternDescriptor.elements.size(); ++index)
      if (!matchConstPatternType(patternDescriptor.elements[index], actualDescriptor.elements[index], state))
        return false;
    return true;
  }
  }
  return false;
}

auto Checker::matchConstPattern(const ConstDeclChecked &candidate, const std::vector<TypeId> &arguments) const
    -> ConstMatch
{
  if (candidate.pattern.size() != arguments.size())
    return {};
  PatternMatchState state;
  bool constructed{};
  for (size_t index = 0; index < arguments.size(); ++index)
  {
    if (!matchConstPatternType(candidate.pattern[index], arguments[index], state))
      return {};
    if (interner_.descriptor(candidate.pattern[index]).kind != TypeKind::TypeParameter)
      constructed = true;
  }
  ConstMatch match{.matched = true, .bindings = std::move(state.bindings)};
  if (!state.containsParameter)
    match.priority = 3;
  else if (constructed || state.repeated)
    match.priority = 2;
  else
    match.priority = 1;
  return match;
}

auto Checker::selectConstDeclaration(std::string_view name, const std::vector<TypeId> &arguments,
                                      syntax::SourceSpan span) const -> const ConstDeclChecked *
{
  const auto found = constDeclarations_.find(std::string{name});
  if (found == constDeclarations_.end())
    throw TypeError(std::format("unknown const declaration `{}`", name), span);
  const ConstDeclChecked *best = nullptr;
  int bestPriority{};
  for (const auto &candidate : found->second)
  {
    ConstMatch match = matchConstPattern(candidate, arguments);
    if (!match.matched)
      continue;
    if (best == nullptr || match.priority > bestPriority)
    {
      best = &candidate;
      bestPriority = match.priority;
    }
    else if (match.priority == bestPriority)
    {
      throw TypeError(std::format("ambiguous const specialization `{}`", name), span);
    }
  }
  if (best == nullptr)
    throw TypeError(std::format("no const specialization of `{}` matches the given type arguments", name), span);
  return best;
}

auto Checker::evaluateConstDeclaration(const ConstDeclChecked &selected, syntax::SourceSpan span)
    -> const_eval::ConstValueId
{
  if (selected.declaration->bodyKind == hir::ConstDeclarationBodyKind::Delete)
    throw TypeError(
        std::format("const declaration `{}` is deleted for these type arguments", selected.declaration->name),
        selected.declaration->span);
  if (selected.declaration->bodyKind == hir::ConstDeclarationBodyKind::Native)
    throw TypeError(std::format("no const native registered for `{}`", selected.declaration->name), span);
  const_eval::ConstEvaluator evaluator{interner_.constInterner()};
  return evaluator.evaluate(*selected.declaration->body, {});
}

auto Checker::evaluateTupleIntrospection(std::string_view name, const std::vector<TypeId> &arguments,
                                          syntax::SourceSpan span) -> std::optional<const_eval::ConstValueId>
{
  if (name == "is_tuple")
  {
    if (arguments.size() != 1)
      throw TypeError("is_tuple<T> expects exactly 1 type argument", span);
    return interner_.constInterner().internBool(interner_.descriptor(arguments[0]).kind == TypeKind::Tuple);
  }
  if (name == "tuple_size")
  {
    if (arguments.size() != 1)
      throw TypeError("tuple_size<T> expects exactly 1 type argument", span);
    const auto &descriptor = interner_.descriptor(arguments[0]);
    if (descriptor.kind != TypeKind::Tuple)
      throw TypeError(std::format("tuple_size<T> expects a tuple type, got {}", interner_.display(arguments[0])),
                      span);
    return interner_.constInterner().internInteger(static_cast<int64_t>(descriptor.elements.size()));
  }
  if (name == "sizeof_pack")
  {
    if (arguments.empty())
      throw TypeError("sizeof_pack requires at least one type argument", span);
    return interner_.constInterner().internInteger(static_cast<int64_t>(arguments.size()));
  }
  return std::nullopt;
}

auto Checker::evaluateTraitIntrospection(std::string_view name,
                                          const std::vector<hir::TypeArgument> &arguments,
                                          syntax::SourceSpan span) -> std::optional<const_eval::ConstValueId>
{
  if (name != "is_trait" && name != "is_abstract")
    return std::nullopt;
  if (arguments.size() != 1 || arguments[0].type == nullptr)
    throw TypeError(std::format("{}<T> expects exactly 1 type argument", name), span);
  const auto &type = *arguments[0].type;
  const bool isTrait = type.kind == hir::TypeKind::Named && traits_.contains(type.name);
  if (isTrait)
    return interner_.constInterner().internBool(true);
  const TypeId resolved = interner_.resolveInScope(type, genericBindings_);
  const auto &descriptor = interner_.descriptor(resolved);
  if (descriptor.kind == TypeKind::TypeParameter)
    throw TypeError(std::format("cannot evaluate const declaration `{}` for abstract type parameter `{}`", name,
                                interner_.display(resolved)),
                    span);
  if (name == "is_trait")
    return interner_.constInterner().internBool(false);
  return interner_.constInterner().internBool(descriptor.kind == TypeKind::Opaque && descriptor.abstractType);
}

auto Checker::evaluateConstApplication(const hir::Expression &expression) -> const_eval::ConstValueId
{
  if (const auto builtin =
          evaluateTraitIntrospection(expression.text, expression.genericArguments, expression.span);
      builtin.has_value())
    return *builtin;
  std::vector<TypeId> typeArguments;
  for (const auto &argument : expression.genericArguments)
  {
    if (argument.kind != syntax::GenericArgumentKind::Type)
      throw TypeError("const arguments on const predicates are not yet supported", argument.span);
    const TypeId resolved = interner_.resolveInScope(*argument.type, genericBindings_);
    if (interner_.descriptor(resolved).kind == TypeKind::TypeParameter)
      throw TypeError(std::format("cannot evaluate const declaration `{}` for abstract type parameter `{}`",
                                  expression.text, interner_.display(resolved)),
                      expression.span);
    typeArguments.push_back(resolved);
  }
  if (const auto builtin = evaluateTupleIntrospection(expression.text, typeArguments, expression.span);
      builtin.has_value())
    return *builtin;
  const auto *selected = selectConstDeclaration(expression.text, typeArguments, expression.span);
  const const_eval::ConstValueId value = evaluateConstDeclaration(*selected, expression.span);
  if (interner_.constInterner().value(value).kind != const_eval::ConstValueKind::Bool)
    throw TypeError(
        std::format("const declaration `{}` must evaluate to bool in predicate position", expression.text),
        expression.span);
  return value;
}

auto Checker::unifyPatternArgument(TypeId pattern, TypeId concrete, Substitution &substitution) const -> bool
{
  if (interner_.descriptor(pattern).kind == TypeKind::TypeParameter)
  {
    const auto existing = substitution.types.find(pattern.value);
    if (existing != substitution.types.end())
      return existing->second == concrete;
    substitution.types.emplace(pattern.value, concrete);
    return true;
  }
  return pattern == concrete;
}

auto Checker::matchImplPattern(const ImplInfo &impl, TypeId concreteType, Substitution &substitution) const -> bool
{
  if (!impl.generic)
    return false;
  const auto &pattern = interner_.descriptor(impl.targetPatternId);
  const auto &concrete = interner_.descriptor(concreteType);
  if (pattern.kind != concrete.kind)
    return false;
  if (pattern.kind == TypeKind::DynamicArray)
    return unifyPatternArgument(pattern.element, concrete.element, substitution);
  if (pattern.kind != TypeKind::Enum && pattern.kind != TypeKind::Struct)
    return false;
  if (*pattern.nominalId != *concrete.nominalId)
    return false;
  if (pattern.typeArguments.size() != concrete.typeArguments.size())
    return false;
  for (size_t index = 0; index < pattern.typeArguments.size(); ++index)
    if (!unifyPatternArgument(pattern.typeArguments[index], concrete.typeArguments[index], substitution))
      return false;
  return true;
}

auto Checker::hasImpl(const std::string &traitName, TypeId target) const -> bool
{
  // Cell clone is compiler-lowered (fresh cell + deep payload), like Box.clone.
  if (traitName == "Clone" && isCell(target))
    return true;
  const auto declared = traits_.find(traitName);
  if (declared != traits_.end() && declared->second.autoTrait)
  {
    const auto kind = interner_.descriptor(target).kind;
    if (traitName == "Fn" || traitName == "FnOnce" || traitName == "FnMut")
      return kind == TypeKind::Closure;
    return kind != TypeKind::TypeParameter && kind != TypeKind::TypeConstructor &&
            kind != TypeKind::TypeApplication;
  }
  for (const auto &impl : impls_)
  {
    if (impl.generic)
    {
      Substitution substitution;
      if (!matchImplPattern(impl, target, substitution))
        continue;
    }
    else if (impl.target != target)
    {
      continue;
    }
    std::vector<const TraitInfo *> closure;
    std::unordered_set<std::string> seen;
    collectSupertraits(impl.traitName, closure, seen);
    for (const auto *trait : closure)
      if (trait->name == traitName)
        return true;
  }
  return false;
}

} // namespace NG::typecheck::detail
