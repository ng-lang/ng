// AI-generated code; reviewed for this repository's vNext rewrite.
// typecheck_call.cpp — call checking: overload selection, folds, spreads,
// variadic packs, generic substitution, closure arguments/targets/captures,
// method dispatch (bounds, trait views, derives, impls), and closure literals.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

  // ---- Shared call helpers -----------------------------------------------------

  auto Checker::fixedParameterCount(const FunctionTypeIds &signature) -> size_t
  {
    return signature.parameters.size() - (signature.packParameters.empty() ? 0 : 1);
  }

  auto Checker::callParameterType(const FunctionTypeIds &signature, size_t index) const -> TypeId
  {
    return index < fixedParameterCount(signature) ? signature.parameters[index]
                                                  : interner_.descriptor(signature.parameters.back()).element;
  }

  auto Checker::closureCaptureLocals(uint32_t closureLocal) const -> std::vector<uint32_t>
  {
    // Instance frames thread their own capture params; original frames thread
    // the closure local's captured locals.
    if (const auto instance = instanceCaptureParamsByLocal_.find(closureLocal);
        instance != instanceCaptureParamsByLocal_.end())
      return instance->second;
    std::vector<uint32_t> captures;
    if (const auto found = closureCapturesByLocal_.find(closureLocal); found != closureCapturesByLocal_.end())
      for (const auto &capture : found->second)
        captures.push_back(capture.localId);
    return captures;
  }

  void Checker::threadCallCaptures(const hir::Expression &call, const std::vector<uint32_t> &captures)
  {
    if (callTargets_.contains(&call) || captures.empty())
      return;
    auto &list = callCaptures_[&call];
    list.insert(list.end(), captures.begin(), captures.end());
  }

  auto Checker::bindForwardedClosure(const hir::Expression &call, const hir::Expression &argument, TypeId argumentType,
                                     size_t index, std::vector<std::pair<size_t, hir::DefId>> &closureTargets) -> bool
  {
    if (interner_.descriptor(argumentType).kind != TypeKind::Closure)
      return false;
    const auto bound = closureLocals_.find(argument.resolvedName->id);
    if (bound == closureLocals_.end())
      return false;
    closureTargets.emplace_back(index, bound->second);
    // INC-14 S3/S4: a capturing closure passed into a higher-order function
    // threads its capture values into the specialized instance.
    if (!callTargets_.contains(&call))
    {
      auto &list = callCaptures_[&call];
      const auto captures = closureCaptureLocals(argument.resolvedName->id);
      list.insert(list.end(), captures.begin(), captures.end());
    }
    return true;
  }

  void Checker::bindPackArgument(Substitution &substitution, TypeId packParameter, size_t position, TypeId argumentType,
                                 syntax::SourceSpan span) const
  {
    auto &pack = substitution.packs[packParameter.value];
    if (position >= pack.size())
    {
      pack.push_back(argumentType);
      return;
    }
    if (pack[position] != argumentType)
      throw TypeError(std::format("generic argument type mismatch: expected {}, got {}",
                                  interner_.display(pack[position]), interner_.display(argumentType)),
                      span);
  }

  void Checker::unifyGenericArgument(const hir::Expression &argument, TypeId parameterType, Substitution &substitution,
                                     const LocalTypes &locals)
  {
    const auto &parameter = interner_.descriptor(parameterType);
    if (argument.kind != hir::ExpressionKind::EnumLiteral || parameter.kind != TypeKind::Enum)
    {
      unify(parameterType, infer(argument, locals), substitution, argument.span);
      return;
    }
    if (!argument.enumId.has_value() || !argument.variant.has_value() || parameter.nominalId != argument.enumId->value)
      throw TypeError("generic enum constructor type mismatch", argument.span);
    const uint32_t variant = *argument.variant;
    if (variant >= parameter.elements.size())
      throw TypeError("enum variant is out of range", argument.span);
    if (argument.operands.size() != (parameter.variantHasPayload[variant] ? 1U : 0U))
      throw TypeError(std::format("enum variant `{}` payload arity mismatch", parameter.fieldNames[variant]),
                      argument.span);
    if (!argument.operands.empty())
      unify(parameter.elements[variant], infer(*argument.operands.front(), locals), substitution,
            argument.operands.front()->span);
  }

  void Checker::requireWhereClause(hir::DefId target, const Substitution &substitution,
                                   const FunctionTypeIds &signature, syntax::SourceSpan span)
  {
    if (module_ == nullptr || target.value >= module_->functions.size())
      return;
    const auto &function = module_->functions.at(target.value);
    // Signature bounds (`<T: Trait>`) are checked per concrete binding like
    // `where T: Trait`; bindings still abstract in a generic caller are
    // checked when that caller is instantiated.
    for (const auto &[parameterName, traitNames] : function.traitBounds)
    {
      const auto position = std::ranges::find(signature.genericParameterNames, parameterName);
      if (position == signature.genericParameterNames.end())
        continue;
      const TypeId parameter =
          signature
              .genericParameters[static_cast<size_t>(std::distance(signature.genericParameterNames.begin(), position))];
      const auto bound = substitution.types.find(parameter.value);
      if (bound == substitution.types.end() || interner_.descriptor(bound->second).kind == TypeKind::TypeParameter)
        continue;
      for (const auto &traitName : traitNames)
        if (!hasImpl(traitName, bound->second))
          throw TypeError(std::format("call to `{}` requires `{}: {}`, but {} does not implement `{}`", function.name,
                                      parameterName, traitName, interner_.display(bound->second), traitName),
                          span);
    }
    if (function.whereClause != nullptr && !evaluateWhereCondition(*function.whereClause, substitution, signature))
      throw TypeError(std::format("call to `{}` does not satisfy its where clause", function.name), span);
  }

  auto Checker::isConcrete(const Substitution &substitution) const -> bool
  {
    return std::ranges::none_of(substitution.types, [&](const auto &binding)
                                { return interner_.descriptor(binding.second).kind == TypeKind::TypeParameter; });
  }

  void Checker::trackCallConsumption(const hir::Expression &call, hir::DefId target, const FunctionTypeIds &signature,
                                     size_t supplied, const CallSpreadInfo *spread, const LocalTypes &locals)
  {
    if (target.value < module_->functions.size() && module_->functions.at(target.value).externC)
      return;
    for (size_t index = 0; index < supplied; ++index)
    {
      if (spread != nullptr && !spread->spreadSlot[index])
        continue;
      const auto kind = interner_.descriptor(callParameterType(signature, index)).kind;
      if (kind == TypeKind::Reference || kind == TypeKind::RawPointer)
        continue;
      trackConsumption(spread != nullptr ? *spread->argumentExpressions[index] : *call.operands[index + 1], locals);
    }
  }

  void Checker::recordVariadicPack(const hir::Expression &call, size_t packCount, size_t fixedParameters,
                                   size_t supplied, const std::function<TypeId(size_t)> &argumentType)
  {
    callPackArgCounts_.insert_or_assign(&call, packCount);
    std::vector<TypeId> packed;
    for (size_t index = fixedParameters; index < supplied; ++index)
      packed.push_back(argumentType(index));
    callPackTupleTypes_.insert_or_assign(&call, interner_.internTuple(std::move(packed)));
  }

  auto Checker::selectOverload(const hir::Expression &call, const LocalTypes &locals, bool requireMatch) -> hir::DefId
  {
    const auto &callee = *call.operands[0];
    hir::DefId selected{callee.resolvedName->id};
    if (callee.functionCandidates.size() <= 1)
      return selected;
    int bestScore{-1};
    // Argument types do not depend on the candidate: infer each once, outside
    // the candidate trial, so an error inside an argument (INC-14 S6:
    // `print([twice(xs)...])`) surfaces instead of reading as "no candidate
    // matched". Closure literals and generic enum constructors need a
    // parameter type and are matched per candidate.
    std::vector<std::optional<TypeId>> argumentTypes(call.operands.size() - 1);
    const auto argumentType = [&](size_t index) -> TypeId
    {
      if (!argumentTypes[index].has_value())
        argumentTypes[index] = infer(*call.operands[index + 1], locals);
      return *argumentTypes[index];
    };
    for (const auto candidate : callee.functionCandidates)
    {
      const auto &signature = signatures_.at(candidate.value);
      if (signature.parameters.size() != call.operands.size() - 1)
        continue;
      const bool generic = isGeneric(signature);
      if (!generic)
      {
        bool matches = true;
        for (size_t index = 0; matches && index < signature.parameters.size(); ++index)
        {
          const auto &argument = *call.operands[index + 1];
          if (isGenericEnumLiteral(argument))
          {
            const auto &parameter = interner_.descriptor(signature.parameters[index]);
            matches = parameter.kind == TypeKind::Enum && parameter.nominalId == argument.enumId->value;
            continue;
          }
          if (argument.kind == hir::ExpressionKind::Closure)
          {
            matches = interner_.descriptor(signature.parameters[index]).kind == TypeKind::Closure;
            continue;
          }
          const TypeId actual = argumentType(index);
          try
          {
            requireType(signature.parameters[index], actual, argument.span, "specialization argument");
          }
          catch (const TypeError &)
          {
            matches = false;
          }
        }
        if (!matches)
          continue;
      }
      int score = generic ? 0 : 100;
      for (const auto parameter : signature.parameters)
        score += specificity(parameter);
      if (score > bestScore)
      {
        bestScore = score;
        selected = candidate;
      }
      else if (score == bestScore && candidate != selected)
      {
        throw TypeError("ambiguous function specialization", call.span);
      }
    }
    if (requireMatch && bestScore < 0)
      throw TypeError("no matching function specialization", call.span);
    return selected;
  }

  // ---- Spreads and folds -------------------------------------------------------

  auto Checker::analyzeCallSpread(const hir::Expression &expression, size_t firstArgument, const LocalTypes &locals,
                                  CallSpreadInfo &info) -> bool
  {
    bool anySpread = false;
    for (size_t index = firstArgument; index < expression.operands.size(); ++index)
    {
      const auto &argument = *expression.operands[index];
      if (!isSpread(argument))
      {
        // INC-14 S2c: closure literal arguments cannot be inferred standalone;
        // the call checks them against the closure parameter in a deferred
        // pass, leaving their slot empty. Generic enum constructors likewise
        // take their type from the parameter (or unification). An empty array
        // literal `[]` is the same shape: the parameter type supplies its
        // element type, so it is checked by the later inferExpected pass.
        const bool deferred = argument.kind == hir::ExpressionKind::Closure || isGenericEnumLiteral(argument) ||
                              (argument.kind == hir::ExpressionKind::ArrayLiteral && argument.operands.empty());
        info.expandedTypes.push_back(deferred ? TypeId{} : infer(argument, locals));
        info.spreadSlot.push_back(false);
        info.argumentExpressions.push_back(&argument);
        continue;
      }
      const TypeId operand = infer(*argument.operands[0], locals);
      const auto &descriptor = interner_.descriptor(operand);
      if (descriptor.kind != TypeKind::Tuple)
        throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)), argument.span);
      info.spreadPositions.push_back(index - firstArgument);
      info.expandedTypes.insert(info.expandedTypes.end(), descriptor.elements.begin(), descriptor.elements.end());
      info.spreadSlot.insert(info.spreadSlot.end(), descriptor.elements.size(), true);
      info.argumentExpressions.insert(info.argumentExpressions.end(), descriptor.elements.size(),
                                      argument.operands[0].get());
      anySpread = true;
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
      if (!isSpread(argument))
        continue;
      const TypeId operand = infer(*argument.operands[0], locals);
      const auto &descriptor = interner_.descriptor(operand);
      if (!isArrayKind(descriptor.kind) && descriptor.kind != TypeKind::Range)
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

  /// `f(acc, xs...)` / `f(xs..., acc)`: the element and accumulator unify with
  /// the two parameters, and the result must feed back as the accumulator.
  auto Checker::inferFoldCall(const hir::Expression &call, const FoldInfo &fold, hir::DefId target,
                              const LocalTypes &locals) -> TypeId
  {
    if (call.operands.size() != 3)
      throw TypeError("fold calls require exactly one accumulator argument", call.span);
    const auto &signature = signatures_.at(target.value);
    if (signature.parameters.size() != 2)
      throw TypeError("fold function must take exactly two arguments", call.span);
    Substitution substitution;
    unify(signature.parameters[fold.spreadPosition], fold.elementType, substitution, call.span);
    const TypeId accumulator = infer(*call.operands[fold.accumulatorPosition + 1], locals);
    unify(signature.parameters[fold.accumulatorPosition], accumulator, substitution, call.span);
    const TypeId result = specializeReturnType(signature, substitution, 0);
    unify(signature.parameters[fold.accumulatorPosition], result, substitution, call.span);
    requireConstArguments(signature, substitution, call.operands[0]->text, call.span);
    requireWhereClause(target, substitution, signature, call.span);
    callFoldSpreadPositions_.insert_or_assign(&call, std::vector<size_t>{fold.spreadPosition});
    callFoldAccumulatorPositions_.insert_or_assign(&call, std::vector<size_t>{fold.accumulatorPosition});
    callTargets_.insert_or_assign(&call, target);
    return specializeReturnType(signature, substitution, 0);
  }

  auto Checker::explicitSubstitution(const hir::Expression &expression, const FunctionTypeIds &signature)
      -> Substitution
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
        const bool constructor = descriptor.typeArguments.empty() && descriptor.nominalId.has_value() &&
                                 ((descriptor.kind == TypeKind::Struct &&
                                   interner_.structGenericArity(hir::StructId{*descriptor.nominalId}) != 0) ||
                                  (descriptor.kind == TypeKind::Enum &&
                                   interner_.enumGenericArity(hir::EnumId{*descriptor.nominalId}) != 0));
        if (constructor)
          throw TypeError(std::format("generic argument {} is a type constructor, not a type", index + 1),
                          argument.span);
        substitution.types.emplace(signature.genericParameters[typeIndex++].value, resolved);
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
            *interner_.descriptor(signature.constructorParameters[constructorIndex++]).nominalId, templateType);
        break;
      }
      case syntax::GenericParameterKind::Const:
      {
        if (argument.kind != syntax::GenericArgumentKind::ConstExpr)
          throw TypeError(std::format("generic argument {} must be a const expression", index + 1), argument.span);
        substitution.consts.emplace(
            static_cast<uint32_t>(constIndex++),
            const_eval::ConstEvaluator{interner_.constInterner()}.evaluate(*argument.constExpr, {}));
        break;
      }
      case syntax::GenericParameterKind::Pack:
        break;
      }
    }
    return substitution;
  }

  // ---- Synthesis-mode calls ----------------------------------------------------

  auto Checker::inferCall(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    const auto &callee = *expression.operands[0];
    if (callee.resolvedName.has_value() && callee.resolvedName->kind == hir::ResolvedNameKind::Local)
    {
      const TypeId calleeType = infer(callee, locals);
      if (interner_.descriptor(calleeType).kind == TypeKind::Closure)
        return inferClosureCall(expression, calleeType, locals);
    }
    if (!callee.resolvedName.has_value() || callee.resolvedName->kind != hir::ResolvedNameKind::Function)
      throw TypeError("call target is not a function", callee.span);
    if (const FoldInfo fold = analyzeFoldCall(expression, locals); fold.fold)
      return inferFoldCall(expression, fold, hir::DefId{callee.resolvedName->id}, locals);

    hir::DefId selected = selectOverload(expression, locals, true);
    const auto &signature = signatures_.at(selected.value);
    CallSpreadInfo spread;
    const bool hasSpread = analyzeCallSpread(expression, 1, locals, spread);
    const size_t supplied = hasSpread ? spread.expandedCount : expression.operands.size() - 1;
    const bool variadic = !signature.packParameters.empty();
    const size_t fixedParameters = fixedParameterCount(signature);
    if (variadic ? supplied < fixedParameters : supplied != signature.parameters.size())
      throw TypeError(
          std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied),
          expression.span);
    const size_t packCount = variadic ? supplied - fixedParameters : 0;
    if (hasSpread)
      callSpreadPositions_.insert_or_assign(&expression, spread.spreadPositions);

    // INC-17: reuse the argument types precomputed by analyzeCallSpread instead
    // of re-inferring; re-reading a nested call's by-value affine locals would
    // raise a spurious E-move-used. Closure-literal slots are inferred on demand.
    const auto argumentTypeAt = [&](size_t index) -> TypeId
    {
      // Closure literals and generic enum constructors have no precomputed
      // type; synthesis-only slots infer them on demand (which reports the
      // missing type context for an unconstrained generic constructor).
      if (!hasSpread && (expression.operands[index + 1]->kind == hir::ExpressionKind::Closure ||
                         isGenericEnumLiteral(*expression.operands[index + 1])))
        return infer(*expression.operands[index + 1], locals);
      return spread.expandedTypes[index];
    };
    const auto spreadSlot = [&](size_t index) { return hasSpread && spread.spreadSlot[index]; };
    const auto argumentAt = [&](size_t index) -> const hir::Expression &
    { return hasSpread ? *spread.argumentExpressions[index] : *expression.operands[index + 1]; };

    Substitution substitution;
    // INC-14 S2c: closure literal arguments are deferred until the rest of the
    // substitution is bound; literal and variable closure arguments record
    // their code function so instantiateFunction re-checks the callee with the
    // concrete closure binding.
    std::vector<size_t> deferredClosureArguments;
    std::vector<size_t> enumLiteralArguments;
    std::vector<std::pair<size_t, hir::DefId>> closureTargets;
    if (!expression.genericArguments.empty())
    {
      substitution = explicitSubstitution(expression, signature);
      for (size_t index = 0; index < supplied; ++index)
      {
        const TypeId parameterType = specialize(callParameterType(signature, index), substitution);
        if (spreadSlot(index))
          requireType(parameterType, argumentTypeAt(index), argumentAt(index).span,
                      std::format("call argument {}", index + 1));
        else
          static_cast<void>(
              inferExpected(argumentAt(index), parameterType, locals, std::format("call argument {}", index + 1)));
      }
    }
    else
    {
      const bool generic = isGeneric(signature);
      for (size_t index = 0; index < supplied; ++index)
      {
        const auto &argument = argumentAt(index);
        const TypeId parameterType = callParameterType(signature, index);
        const bool fixedSlot = !hasSpread && index < fixedParameters;
        if (fixedSlot && argument.kind == hir::ExpressionKind::Closure)
        {
          deferredClosureArguments.push_back(index);
          continue;
        }
        if (!generic && !spreadSlot(index))
        {
          // Checking mode: the parameter type drives literals, unions, and
          // generic enum constructors (`get(Result.Ok(3))`).
          if (fixedSlot && argument.resolvedName.has_value() &&
              argument.resolvedName->kind == hir::ResolvedNameKind::Local)
            static_cast<void>(bindForwardedClosure(expression, argument, argumentTypeAt(index), index, closureTargets));
          static_cast<void>(inferExpected(argument, parameterType, locals, std::format("call argument {}", index + 1)));
          continue;
        }
        if (!spreadSlot(index) && !(variadic && index >= fixedParameters) &&
            argument.kind == hir::ExpressionKind::EnumLiteral &&
            interner_.descriptor(parameterType).kind == TypeKind::Enum)
        {
          unifyGenericArgument(argument, parameterType, substitution, locals);
          enumLiteralArguments.push_back(index);
          continue;
        }
        const TypeId argumentType = argumentTypeAt(index);
        if (fixedSlot && argument.resolvedName.has_value() &&
            argument.resolvedName->kind == hir::ResolvedNameKind::Local)
          static_cast<void>(bindForwardedClosure(expression, argument, argumentType, index, closureTargets));
        if (!generic)
        {
          requireType(parameterType, argumentType, argument.span, std::format("call argument {}", index + 1));
          continue;
        }
        if (variadic && index >= fixedParameters)
          bindPackArgument(substitution, parameterType, index - fixedParameters, argumentType, argument.span);
        else
          unify(parameterType, argumentType, substitution, argument.span);
      }
    }
    for (const size_t index : deferredClosureArguments)
    {
      const auto &argument = *expression.operands[index + 1];
      static_cast<void>(inferCallClosureArgument(argument, callParameterType(signature, index), index, locals,
                                                 substitution, closureTargets));
      std::vector<uint32_t> captures;
      for (const auto &capture : argument.closureCaptures)
        captures.push_back(capture.local.value);
      threadCallCaptures(expression, captures);
    }

    const TypeId result = specializeReturnType(signature, substitution, packCount);
    if (!signature.genericParameters.empty() && interner_.descriptor(result).kind == TypeKind::TypeParameter)
      throw TypeError(std::format("cannot infer generic arguments for function `{}`", callee.text), expression.span);
    // Generic enum constructor arguments unify through their payloads only;
    // record their specialized type so lowering can build the enum value.
    for (const size_t index : enumLiteralArguments)
      record(argumentAt(index), specialize(callParameterType(signature, index), substitution));
    requireConstArguments(signature, substitution, callee.text, expression.span);
    if (isConcrete(substitution))
      requireWhereClause(selected, substitution, signature, expression.span);
    trackCallConsumption(expression, selected, signature, supplied, hasSpread ? &spread : nullptr, locals);
    if (variadic)
      recordVariadicPack(expression, packCount, fixedParameters, supplied, argumentTypeAt);
    selected = instantiateFunction(selected, substitution, packCount, expression.span, closureTargets);
    callTargets_.insert_or_assign(&expression, selected);
    return result;
  }

  auto Checker::inferClosureCall(const hir::Expression &expression, TypeId calleeType, const LocalTypes &locals)
      -> TypeId
  {
    const uint32_t closureLocal = expression.operands[0]->resolvedName->id;
    const auto &closure = interner_.descriptor(calleeType);
    const auto checkArguments = [&]
    {
      if (closure.elements.size() != expression.operands.size() - 1)
        throw TypeError(std::format("closure argument count mismatch: expected {}, got {}", closure.elements.size(),
                                    expression.operands.size() - 1),
                        expression.span);
      for (size_t index = 0; index < closure.elements.size(); ++index)
        static_cast<void>(inferExpected(*expression.operands[index + 1], closure.elements[index], locals,
                                        std::format("closure argument {}", index + 1)));
    };
    const auto code = closureLocals_.find(closureLocal);
    if (code == closureLocals_.end())
    {
      // INC-14 S2c/S4: templates call closure-typed parameters or generic
      // closure locals with no concrete binding yet; the model types the call
      // from the descriptor and defers codegen to the specialized instance.
      if (!placeholderFunctions_.contains(currentFunctionId_.value) ||
          (!parameterLocals_.contains(closureLocal) && !deferredClosureLocals_.contains(closureLocal)))
        throw TypeError(std::format("closure value `{}` was not created directly by a closure literal",
                                    expression.operands[0]->text),
                        expression.span);
      checkArguments();
      return closure.element;
    }
    checkArguments();
    callTargets_.insert_or_assign(&expression, code->second);
    // INC-14 S3: capture values thread into the call site.
    if (auto captures = closureCaptureLocals(closureLocal); !captures.empty())
      callCaptures_.emplace(&expression, std::move(captures));
    return closure.element;
  }

  // ---- Method calls ------------------------------------------------------------

  auto Checker::inferMethodCall(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    const auto &receiverNode = *expression.operands[0]->operands[0];
    const bool traitName =
        receiverNode.kind == hir::ExpressionKind::ResolvedName && !receiverNode.resolvedName.has_value();
    if (traitName && !traits_.contains(receiverNode.text))
      throw TypeError(std::format("unknown trait `{}` in qualified call", receiverNode.text), receiverNode.span);
    // `Trait.method(receiver, ...)` qualifies the call with a trait name.
    const bool qualified = traitName;
    const hir::Expression &receiver = qualified ? *expression.operands[1] : receiverNode;
    const size_t firstArgument = qualified ? 2 : 1;
    const TypeId receiverType = infer(receiver, locals);
    const bool passReferenceThrough = interner_.descriptor(receiverType).kind == TypeKind::Reference;
    const TypeId dispatchType = passReferenceThrough ? interner_.descriptor(receiverType).element : receiverType;

    TypeId result;
    const auto dispatchKind = interner_.descriptor(dispatchType).kind;
    if (dispatchKind == TypeKind::TypeParameter)
      result = inferBoundMethodCall(expression, dispatchType);
    else if (dispatchKind == TypeKind::TraitReference)
      result = inferTraitViewMethodCall(expression, receiver, dispatchType, firstArgument, locals);
    else if (const auto derived = inferDerivedMethodCall(expression, dispatchType, firstArgument); derived.has_value())
      result = *derived;
    else
      return inferImplMethodCall(expression, receiver, receiverType, dispatchType, passReferenceThrough, firstArgument,
                                 locals);
    record(expression, result);
    return result;
  }

  /// A method on a type parameter resolves through the current function's trait
  /// bounds (or the trait owning the default method being checked); codegen
  /// defers to the monomorphized instance.
  auto Checker::inferBoundMethodCall(const hir::Expression &expression, TypeId dispatchType) -> TypeId
  {
    const std::string parameterName = interner_.descriptor(dispatchType).name;
    const auto provides = [&](const std::string &trait) -> const TraitInfo *
    {
      const auto found = traits_.find(trait);
      return found != traits_.end() && found->second.methodParameters.contains(expression.text) ? &found->second
                                                                                                : nullptr;
    };
    const TraitInfo *owning = nullptr;
    if (currentFunctionId_.value < module_->functions.size())
      for (const auto &[boundParameter, boundTraits] : module_->functions.at(currentFunctionId_.value).traitBounds)
      {
        if (boundParameter != parameterName)
          continue;
        for (const auto &trait : boundTraits)
          if ((owning = provides(trait)) != nullptr)
            break;
        if (owning != nullptr)
          break;
      }
    if (owning == nullptr)
      if (const auto owner = defaultMethodTraits_.find(currentFunctionId_.value); owner != defaultMethodTraits_.end())
        owning = provides(owner->second);
    if (owning == nullptr)
      throw TypeError(
          std::format("no trait bound provides method `{}` for type parameter `{}`", expression.text, parameterName),
          expression.span);
    deferredMethodFunctions_.insert(currentFunctionId_.value);
    return interner_.specialize(owning->methodReturns.at(expression.text),
                                {{owning->selfParameter.value, dispatchType}});
  }

  /// Dynamic dispatch through a `ref<Trait>` view. Methods that mention Self
  /// in the return type (INC-35) specialize when the view's concrete origin
  /// is known; otherwise they are rejected.
  auto Checker::inferTraitViewMethodCall(const hir::Expression &expression, const hir::Expression &receiver,
                                         TypeId dispatchType, size_t firstArgument, const LocalTypes &locals) -> TypeId
  {
    const std::string traitName = interner_.descriptor(dispatchType).name;
    const auto &trait = traits_.at(traitName);
    const auto method = std::ranges::find(trait.methodOrder, expression.text);
    if (method == trait.methodOrder.end())
      throw TypeError(std::format("trait `{}` has no method `{}`", traitName, expression.text), expression.span);
    TypeId returnType = trait.methodReturns.at(expression.text);
    if (typeContains(returnType, trait.selfParameter))
    {
      std::optional<TypeId> concrete;
      if (receiver.resolvedName.has_value() && receiver.resolvedName->kind == hir::ResolvedNameKind::Local)
        if (const auto origin = traitViewOrigins_.find(receiver.resolvedName->id); origin != traitViewOrigins_.end())
          concrete = origin->second;
      if (!concrete.has_value())
        throw TypeError("cannot call a method returning Self on a trait view without a known concrete type",
                        expression.span);
      returnType = interner_.specialize(returnType, {{trait.selfParameter.value, *concrete}});
    }
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
    traitViewCalls_.insert_or_assign(
        &expression, std::pair<std::string, size_t>{
                       traitName, static_cast<size_t>(std::distance(trait.methodOrder.begin(), method))});
    return returnType;
  }

  /// Derived `clone` (incl. Box deep clone) and `show`: argument-free methods
  /// borrowing the receiver shared.
  auto Checker::inferDerivedMethodCall(const hir::Expression &expression, TypeId dispatchType, size_t firstArgument)
      -> std::optional<TypeId>
  {
    const bool boxClone = expression.text == "clone" && (isBox(dispatchType) || isCell(dispatchType));
    const bool clone = boxClone || (expression.text == "clone" && derivedCloneTypes_.contains(dispatchType.value));
    const bool show = expression.text == "show" && derivedShowTypes_.contains(dispatchType.value);
    if (!clone && !show)
      return std::nullopt;
    if (expression.operands.size() != firstArgument)
      throw TypeError(std::format("derived {} takes no arguments", expression.text), expression.span);
    methodReceiverMutable_.insert_or_assign(&expression, false);
    methodReceiverRefTypes_.insert_or_assign(&expression, interner_.internReference(dispatchType, false));
    if (show)
    {
      derivedShowCalls_.insert_or_assign(&expression, dispatchType);
      return builtin::String;
    }
    derivedCloneCalls_.insert_or_assign(&expression, dispatchType);
    if (boxClone)
      boxCloneCalls_.insert_or_assign(&expression, dispatchType);
    return dispatchType;
  }

  auto Checker::findImplMethod(std::string_view name, TypeId dispatchType, Substitution &matched) const
      -> std::optional<hir::DefId>
  {
    for (const auto &impl : impls_)
    {
      if (impl.generic || impl.target != dispatchType)
        continue;
      if (const auto found = impl.methods.find(std::string{name}); found != impl.methods.end())
        return found->second;
    }
    for (const auto &impl : impls_)
    {
      if (!impl.generic)
        continue;
      Substitution substitution;
      if (!matchImplPattern(impl, dispatchType, substitution))
        continue;
      if (const auto found = impl.methods.find(std::string{name}); found != impl.methods.end())
      {
        matched = std::move(substitution);
        return found->second;
      }
    }
    return std::nullopt;
  }

  /// Static dispatch to a concrete (or generic, instantiated) impl method: the
  /// receiver is borrowed per the method's `Self ref` / `Self ref mut`, or
  /// passed through when it already is a reference.
  auto Checker::inferImplMethodCall(const hir::Expression &expression, const hir::Expression &receiver,
                                    TypeId receiverType, TypeId dispatchType, bool passReferenceThrough,
                                    size_t firstArgument, const LocalTypes &locals) -> TypeId
  {
    Substitution matched;
    const auto selected = findImplMethod(expression.text, dispatchType, matched);
    if (!selected.has_value())
      throw TypeError(
          std::format("no method `{}` for value of type {}", expression.text, interner_.display(dispatchType)),
          expression.span);
    hir::DefId target = *selected;
    if (const auto &signature = signatures_.at(selected->value); isGeneric(signature))
    {
      for (const auto parameter : signature.genericParameters)
        matched.types.try_emplace(parameter.value, dispatchType);
      target = instantiateFunction(*selected, matched, 0, expression.span);
    }
    const auto &signature = signatures_.at(target.value);
    const size_t supplied = expression.operands.size() - firstArgument;
    if (supplied != signature.parameters.size() - 1)
      throw TypeError(
          std::format("method argument count mismatch: expected {}, got {}", signature.parameters.size() - 1, supplied),
          expression.span);
    const TypeId receiverParameter = signature.parameters.front();
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
      const auto kind = interner_.descriptor(signature.parameters[index + 1]).kind;
      if (kind != TypeKind::Reference && kind != TypeKind::RawPointer)
        trackConsumption(*expression.operands[firstArgument + index], locals);
    }
    methodReceiverMutable_.insert_or_assign(&expression, receiverDescriptor.referenceMutable);
    methodReceiverRefTypes_.insert_or_assign(&expression, receiverParameter);
    for (size_t index = 0; index < supplied; ++index)
      static_cast<void>(inferExpected(*expression.operands[firstArgument + index], signature.parameters[index + 1],
                                      locals, std::format("method argument {}", index + 1)));
    record(expression, signature.returnType);
    callTargets_.insert_or_assign(&expression, target);
    return signature.returnType;
  }

  // ---- Closures ----------------------------------------------------------------

  auto Checker::inferCallClosureArgument(const hir::Expression &closure, TypeId parameterType, size_t index,
                                         const LocalTypes &locals, Substitution &substitution,
                                         std::vector<std::pair<size_t, hir::DefId>> &closureTargets) -> TypeId
  {
    const TypeId specialized = specialize(parameterType, substitution);
    const auto &descriptor = interner_.descriptor(specialized);
    if (descriptor.kind != TypeKind::Closure)
      throw TypeError(
          std::format("closure argument is not applicable to parameter of type {}", interner_.display(specialized)),
          closure.span);
    if (closure.closureParameterIds.size() != descriptor.elements.size())
      throw TypeError(std::format("closure argument count mismatch: expected {}, got {}", descriptor.elements.size(),
                                  closure.closureParameterIds.size()),
                      closure.span);
    std::vector<TypeId> parameterTypes;
    parameterTypes.reserve(descriptor.elements.size());
    for (const auto parameter : descriptor.elements)
      parameterTypes.push_back(specialize(parameter, substitution));
    const TypeId closureType = inferClosure(closure, parameterTypes, locals);
    record(closure, closureType);
    unify(parameterType, closureType, substitution, closure.span);
    // INC-14 S4: a generic closure has no code function yet (deferred until the
    // enclosing generic is instantiated).
    if (!deferredClosures_.contains(&closure))
      closureTargets.emplace_back(index, closureFunctions_.at(&closure));
    return closureType;
  }

  auto Checker::inferClosure(const hir::Expression &closure, const std::vector<TypeId> &parameterTypes,
                             const LocalTypes &locals) -> TypeId
  {
    // INC-14 S3: the body references fresh capture params typed from the
    // enclosing locals; the original capture locals drive call-site threading.
    validateCaptures(closure);
    LocalTypes closureLocals;
    std::vector<TypeId> captureTypes;
    for (size_t index = 0; index < closure.closureCaptures.size(); ++index)
    {
      const auto found = locals.find(closure.closureCaptures[index].local.value);
      if (found == locals.end())
        throw TypeError("capture is not in scope", closure.span);
      captureTypes.push_back(found->second);
      closureLocals.emplace(closure.closureCaptureParams[index].value, found->second);
      recordLocal(closure.closureCaptureParams[index], found->second);
    }
    // B5: a copy capture of an affine value is threaded into every call, so it
    // must be deep-copied per call — otherwise the first callee consumes the
    // captured value and later calls see a dead handle. Require `Clone`.
    for (size_t index = 0; index < closure.closureCaptures.size(); ++index)
    {
      if (closure.closureCaptures[index].isMove)
        continue;
      const TypeId type = captureTypes[index];
      if (!isAffine(type) || hasImpl("Clone", type))
        continue;
      throw TypeError(std::format("cannot copy-capture affine value of type `{}` that does not implement Clone; "
                                  "use `[move ...]`, `[ref ...]`, or `derive(Clone)` on the type",
                                  interner_.display(type)),
                      closure.span);
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
    // Code function signature: capture params (owned) then user params.
    std::vector<TypeId> codeParameterTypes = captureTypes;
    codeParameterTypes.insert(codeParameterTypes.end(), parameterTypes.begin(), parameterTypes.end());
    // INC-14 S4: a code function whose signature names type parameters cannot
    // be lowered; the concrete one is synthesized when the enclosing generic is
    // instantiated and the closure re-inferred.
    const auto isTypeParameter = [&](TypeId type)
    { return interner_.descriptor(type).kind == TypeKind::TypeParameter; };
    if (isTypeParameter(resultType) || std::ranges::any_of(codeParameterTypes, isTypeParameter))
    {
      deferredClosures_.insert(&closure);
      return interner_.internClosure(parameterTypes, resultType);
    }
    const hir::DefId closureId{static_cast<uint32_t>(module_->functions.size() + instances_.size())};
    const FunctionTypeIds closureSignature{.parameters = codeParameterTypes, .returnType = resultType};
    signatures_.emplace(closureId.value, closureSignature);
    functionTypeIds_.emplace(closureId.value, closureSignature);
    FunctionType displaySignature;
    for (const auto parameter : codeParameterTypes)
      displaySignature.parameters.push_back(interner_.display(parameter));
    displaySignature.returnType = interner_.display(resultType);
    functionTypes_.emplace(closureId.value, std::move(displaySignature));
    instances_.push_back(hir::makeClosureFunction(closure, std::format("__closure{}", closureCounter_++), closureId,
                                                  nextInstanceLocal_));
    checkFunction(instances_.back(), {});
    closureFunctions_.emplace(&closure, closureId);
    // INC-14 S3: move captures consume the enclosing local at the literal
    // site, after checkFunction (which resets moveState_).
    for (const auto &capture : closure.closureCaptures)
      if (capture.isMove)
        moveState_.markWhole(capture.local.value);
    closureCaptures_[&closure] = captureTypes;
    closureCaptureTypesByFunction_[closureId.value] = captureTypes;
    return interner_.internClosure(parameterTypes, resultType);
  }

} // namespace NG::typecheck::detail
