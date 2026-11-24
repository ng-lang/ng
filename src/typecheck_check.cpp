// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck_check.cpp — function, block, and statement checking.
// Switches live in typecheck_switch.cpp; places, moves, and loans in
// typecheck_ownership.cpp.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

  void Checker::checkFunction(const hir::Function &function, const const_eval::ConstBindings &constBindings)
  {
    const hir::DefId previousFunction = currentFunctionId_;
    currentFunctionId_ = function.id;
    const const_eval::ConstBindings previousBindings = std::move(activeConstBindings_);
    activeConstBindings_ = constBindings;
    const auto previousGenericBindings = std::move(genericBindings_);
    const bool previousConstGeneric = inConstGenericFunction_;
    // INC-17 (M2b): closure literals are checked as synthesized functions,
    // re-entering checkFunction mid-statement of the enclosing body. Save the
    // enclosing function's transient statement/borrow/move state so the closure
    // check cannot wipe live loans (a closure capturing a ref is an anonymous
    // borrower) or clobber the enclosing move/mutable context; restore at exit.
    const MutableBindings savedMutableBindings = std::move(mutableBindings_);
    MoveState savedMoveState = std::move(moveState_);
    BorrowState savedBorrowState = std::move(borrowState_);
    const auto savedBorrowExpressions = std::move(borrowExpressions_);
    const auto savedReleasedLoans = std::move(releasedLoans_);
    const auto savedLastUseCache = std::move(lastUseCache_);
    const auto savedParameterLocals = std::move(parameterLocals_);
    const auto savedParamOrdinalOf = std::move(paramOrdinalOf_);
    const auto savedReturnOriginOrdinals = std::move(returnOriginOrdinals_);
    const auto savedReturnOriginDisplay = std::move(returnOriginDisplay_);
    LocalTypes locals;
    mutableBindings_.clear();
    moveState_ = MoveState{};
    borrowState_ = BorrowState{};
    releasedLoans_.clear();
    lastUseCache_.clear();
    const auto &signature = signatures_.at(function.id.value);
    if (interner_.descriptor(signature.returnType).kind == TypeKind::Closure)
      throw TypeError(
          std::format("function `{}` returns a closure, which cannot escape its scope (non-escaping closures, INC-14)",
                      function.name),
          function.span);
    if (!signature.packParameters.empty())
      placeholderFunctions_.insert(function.id.value);
    genericBindings_.clear();
    for (size_t index = 0; index < function.genericParameters.size(); ++index)
      genericBindings_.emplace(function.genericParameters[index], signature.genericParameters[index]);
    for (size_t index = 0; index < function.constructorParameters.size(); ++index)
      genericBindings_.emplace(function.constructorParameters[index], signature.constructorParameters[index]);
    for (size_t index = 0; index < function.variadicConstructorParameters.size(); ++index)
      genericBindings_.emplace(function.variadicConstructorParameters[index],
                               signature.constructorParameters[function.constructorParameters.size() + index]);
    for (size_t index = 0; index < function.parameters.size(); ++index)
    {
      locals.emplace(function.parameters[index].local.value, signature.parameters[index]);
      recordLocal(function.parameters[index].local, signature.parameters[index]);
    }
    parameterLocals_.clear();
    paramOrdinalOf_.clear();
    for (size_t index = 0; index < function.parameters.size(); ++index)
    {
      parameterLocals_.insert(function.parameters[index].local.value);
      paramOrdinalOf_.emplace(function.parameters[index].local.value, static_cast<uint32_t>(index));
    }
    returnOriginOrdinals_.clear();
    returnOriginDisplay_.clear();
    if (function.returnType != nullptr && function.returnType->kind == hir::TypeKind::ScopedReference &&
        !function.returnType->originOrdinals.empty())
    {
      returnOriginOrdinals_.insert(function.returnType->originOrdinals.begin(),
                                   function.returnType->originOrdinals.end());
      std::string display;
      for (size_t i = 0; i < function.returnType->originOrdinals.size(); ++i)
      {
        const auto &ordinal = function.returnType->originOrdinals[i];
        if (i > 0)
          display += " | ";
        if (ordinal < function.parameters.size())
          display += function.parameters[ordinal].name;
        else
          display += std::format("<param{}>", ordinal);
      }
      returnOriginDisplay_ = std::move(display);
    }
    inConstGenericFunction_ = !signature.constParameters.empty();
    if (function.whereClause != nullptr && !isGeneric(signature))
    {
      if (!evaluateWhereCondition(*function.whereClause, Substitution{}, signature))
        throw TypeError(std::format("function `{}` does not satisfy its where clause", function.name), function.span);
    }
    checkBlock(function.body, locals, {}, signature.returnType);
    inConstGenericFunction_ = false;
    recordFallthroughDrops(function, locals);
    applyGrowthDecisions(function, locals);
    mutableBindings_ = std::move(savedMutableBindings);
    moveState_ = std::move(savedMoveState);
    borrowState_ = std::move(savedBorrowState);
    borrowExpressions_ = std::move(savedBorrowExpressions);
    releasedLoans_ = std::move(savedReleasedLoans);
    lastUseCache_ = std::move(savedLastUseCache);
    parameterLocals_ = std::move(savedParameterLocals);
    paramOrdinalOf_ = std::move(savedParamOrdinalOf);
    returnOriginOrdinals_ = std::move(savedReturnOriginOrdinals);
    returnOriginDisplay_ = std::move(savedReturnOriginDisplay);
    genericBindings_ = std::move(previousGenericBindings);
    inConstGenericFunction_ = previousConstGeneric;
    activeConstBindings_ = previousBindings;
    currentFunctionId_ = previousFunction;
  }

  void Checker::recordFallthroughDrops(const hir::Function &function, const LocalTypes &locals)
  {
    for (const auto &[local, type] : locals)
    {
      if (!hasDrop(type) || moveState_.isWholeMoved(local))
        continue;
      fallthroughDrops_[function.id.value].push_back(dropEdge(local, type, function.span));
    }
  }

  void Checker::checkBlock(const hir::Block &block, LocalTypes locals, LoopTypes loops, TypeId returnType,
                           bool nestedScope)
  {
    std::unordered_set<uint32_t> incomingLocals;
    for (const auto &[local, type] : locals)
      incomingLocals.insert(local);
    const MutableBindings saved = mutableBindings_;
    const BorrowState borrowsSaved = borrowState_;
    const std::unordered_map<uint32_t, size_t> lastUse = lastUseByBlock(block);
    for (size_t index = 0; index < block.statements.size(); ++index)
    {
      const size_t loansBefore = borrowState_.loans.size();
      checkStatement(block.statements[index], locals, loops, returnType);
      releaseLoansUsedUpTo(index, lastUse, loansBefore);
    }
    if (block.tailExpression != nullptr)
      static_cast<void>(infer(*block.tailExpression, locals));
    if (nestedScope)
    {
      // Scope-local values with a Drop impl that were not moved out drop at
      // the block end; loans borne only by scope locals end with them.
      std::vector<std::pair<uint32_t, uint32_t>> drops;
      for (const auto &[local, type] : locals)
        if (!incomingLocals.contains(local) && hasDrop(type) && !moveState_.isWholeMoved(local))
          drops.push_back(dropEdge(local, type, block.span));
      if (!drops.empty())
        blockDrops_.emplace(&block, std::move(drops));
      releaseScopeBearers(incomingLocals);
    }
    mutableBindings_ = saved;
    borrowState_ = borrowsSaved;
    pruneReleasedLoans();
  }

  void Checker::checkStatement(const hir::Statement &statement, LocalTypes &locals, LoopTypes &loops, TypeId returnType)
  {
    switch (statement.kind)
    {
    case hir::StatementKind::Let:
      checkLet(statement, locals);
      return;
    case hir::StatementKind::Assign:
      checkAssign(statement, locals);
      return;
    case hir::StatementKind::Return:
      checkReturn(statement, locals, returnType);
      return;
    case hir::StatementKind::If:
      checkIf(statement, locals, loops, returnType);
      return;
    case hir::StatementKind::ConstIf:
      checkConstIf(statement, locals, loops, returnType);
      return;
    case hir::StatementKind::Loop:
    {
      std::vector<TypeId> types;
      for (const auto &initializer : statement.arguments)
        types.push_back(infer(*initializer, locals));
      LocalTypes loopLocals = locals;
      for (size_t index = 0; index < statement.loopBindings.size(); ++index)
      {
        loopLocals.emplace(statement.loopBindings[index].value, types[index]);
        recordLocal(statement.loopBindings[index], types[index]);
        mutableBindings_.insert(statement.loopBindings[index].value);
      }
      LoopTypes loopTypes = loops;
      loopTypes.emplace(statement.loop->value, std::move(types));
      checkLoopBody(*statement.body, std::move(loopLocals), std::move(loopTypes), returnType);
      return;
    }
    case hir::StatementKind::While:
      requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "while condition");
      checkLoopBody(*statement.body, locals, loops, returnType);
      return;
    case hir::StatementKind::Next:
      checkNext(statement, locals, loops);
      return;
    case hir::StatementKind::Continue:
    case hir::StatementKind::Break:
      // Terminators with a unit value; control leaves the enclosing loop.
      return;
    case hir::StatementKind::Switch:
      checkSwitch(statement, locals, loops, returnType);
      return;
    case hir::StatementKind::Expression:
      static_cast<void>(infer(*statement.expression, locals));
      discardStatementValue(statement, locals);
      return;
    }
  }

  /// `move s;` (or a bare `s;`) consumes the local like any other by-value
  /// use; the discarded value is dropped right after the statement, since no
  /// binding owns it any more.
  void Checker::discardStatementValue(const hir::Statement &statement, const LocalTypes &locals)
  {
    const hir::Expression *consumed = statement.expression.get();
    while (consumed->kind == hir::ExpressionKind::Grouped)
      consumed = consumed->operands[0].get();
    const bool explicitMove = consumed->kind == hir::ExpressionKind::Prefix && consumed->text == "move";
    const hir::Expression &place = explicitMove ? *consumed->operands[0] : *consumed;
    if (place.kind != hir::ExpressionKind::ResolvedName || !place.resolvedName.has_value() ||
        place.resolvedName->kind != hir::ResolvedNameKind::Local)
      return;
    const uint32_t local = place.resolvedName->id;
    const TypeId type = locals.at(local);
    if (!explicitMove && !isAffine(type))
      return;
    const bool wasMoved = moveState_.isWholeMoved(local);
    trackConsumption(*consumed, locals);
    if (!wasMoved && moveState_.isWholeMoved(local) && hasDrop(type))
      returnDrops_[&statement].push_back(dropEdge(local, type, statement.span));
  }

  void Checker::checkLet(const hir::Statement &statement, LocalTypes &locals)
  {
    const TypeId bindingType = statement.bindingType != nullptr
                                   ? interner_.resolveInScope(*statement.bindingType, genericBindings_)
                                   : TypeId{};
    if (bindingType.value != 0 && interner_.descriptor(bindingType).kind == TypeKind::Trait)
      throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`", interner_.display(bindingType),
                                  interner_.display(bindingType)),
                      statement.span);
    const TypeId type = statement.bindingType != nullptr
                            ? inferExpected(*statement.expression, bindingType, locals, "let initializer")
                            : infer(*statement.expression, locals);
    if (statement.expression->kind == hir::ExpressionKind::Closure && statement.local.has_value())
      bindClosureLocal(statement);
    // Trait views borrow their referent rather than consuming it.
    if (interner_.descriptor(type).kind != TypeKind::TraitReference)
      trackConsumption(*statement.expression, locals);
    bindLetLocals(statement, type, locals);
    if (statement.local.has_value() && statement.expression != nullptr)
    {
      if (const auto coercion = traitViewCoercions_.find(statement.expression.get());
          coercion != traitViewCoercions_.end())
        traitViewOrigins_[statement.local->value] = coercion->second.second;
      else if (statement.expression->resolvedName.has_value() &&
               statement.expression->resolvedName->kind == hir::ResolvedNameKind::Local)
        if (const auto origin = traitViewOrigins_.find(statement.expression->resolvedName->id);
            origin != traitViewOrigins_.end())
          traitViewOrigins_[statement.local->value] = origin->second;
    }
    // `let r = ref x;` / `let v = span xs;`: the binding becomes the bearer of
    // the loan created by its initializer.
    const auto &initializer = *statement.expression;
    if (statement.local.has_value() && initializer.kind == hir::ExpressionKind::Prefix &&
        (initializer.text == "ref" || initializer.text == "ref mut" || initializer.text == "span"))
    {
      const auto loan = std::ranges::find_if(borrowState_.loans,
                                             [&](const auto &candidate) { return candidate.site == &initializer; });
      if (loan != borrowState_.loans.end())
      {
        if (const auto untied = std::ranges::find(loan->bearers, 0U); untied != loan->bearers.end())
          *untied = statement.local->value;
        else if (std::ranges::find(loan->bearers, statement.local->value) == loan->bearers.end())
          loan->bearers.push_back(statement.local->value);
      }
    }
  }

  void Checker::bindLetLocals(const hir::Statement &statement, TypeId type, LocalTypes &locals)
  {
    const auto bind = [&](hir::LocalId local, TypeId localType)
    {
      locals.emplace(local.value, localType);
      recordLocal(local, localType);
      if (statement.mutableBinding)
        mutableBindings_.insert(local.value);
    };
    if (statement.destructuredLocals.empty() && !statement.restLocal.has_value())
    {
      bind(*statement.local, type);
      return;
    }
    const auto &tuple = interner_.descriptor(type);
    if (tuple.kind != TypeKind::Tuple)
      throw TypeError(std::format("cannot destructure value of type {}", interner_.display(type)),
                      statement.expression->span);
    const size_t bound = statement.destructuredLocals.size();
    if (statement.restLocal.has_value() && tuple.elements.size() < bound)
      throw TypeError(std::format("tuple destructuring length mismatch: pattern binds {}, tuple has {}", bound,
                                  tuple.elements.size()),
                      statement.span);
    if (!statement.restLocal.has_value() && tuple.elements.size() != bound)
      throw TypeError(
          std::format("tuple destructuring length mismatch: expected {}, got {}", tuple.elements.size(), bound),
          statement.span);
    for (size_t index = 0; index < bound; ++index)
      bind(statement.destructuredLocals[index], tuple.elements[index]);
    if (statement.restLocal.has_value())
      bind(*statement.restLocal,
           interner_.internTuple(
               std::vector<TypeId>(tuple.elements.begin() + static_cast<std::ptrdiff_t>(bound), tuple.elements.end())));
  }

  void Checker::bindClosureLocal(const hir::Statement &statement)
  {
    const auto &closure = *statement.expression;
    const uint32_t closureLocal = statement.local->value;
    const auto code = closureFunctions_.find(&closure);
    if (code == closureFunctions_.end())
    {
      // INC-14 S4: generic closure literal. Model-typed only; the concrete code
      // function is synthesized when the enclosing generic function is
      // instantiated. Calls through this local defer via the placeholder path.
      deferredClosureLocals_.insert(closureLocal);
      return;
    }
    closureLocals_.emplace(closureLocal, code->second);
    const auto captureTypes = closureCaptures_.find(&closure);
    if (captureTypes == closureCaptures_.end())
      return;
    // INC-14 S3: capture info for call-site threading.
    std::vector<CaptureLocalInfo> captures;
    for (size_t index = 0; index < closure.closureCaptures.size(); ++index)
      captures.push_back(CaptureLocalInfo{.localId = closure.closureCaptures[index].local.value,
                                          .isMove = closure.closureCaptures[index].isMove,
                                          .type = captureTypes->second[index]});
    closureCapturesByLocal_.emplace(closureLocal, std::move(captures));
    // INC-17 (M2b): a closure capturing a live scoped reference becomes an
    // anonymous borrower of that referent: the loan survives until every bearer
    // (the ref binding and every closure capturing it) is dead, so moving or
    // assigning the referent while the closure lives is rejected.
    for (auto &loan : borrowState_.loans)
    {
      const bool capturesLoan =
          std::ranges::any_of(closure.closureCaptures, [&](const auto &capture)
                              { return std::ranges::find(loan.bearers, capture.local.value) != loan.bearers.end(); });
      if (capturesLoan && std::ranges::find(loan.bearers, closureLocal) == loan.bearers.end())
        loan.bearers.push_back(closureLocal);
    }
  }

  void Checker::rejectViewReassignment(TypeId type, syntax::SourceSpan span) const
  {
    const auto kind = interner_.descriptor(type).kind;
    if (kind == TypeKind::Reference)
      throw TypeError("cannot reassign a scoped reference binding (A1.2; references are non-escaping views)", span);
    if (kind == TypeKind::Span)
      throw TypeError("cannot reassign a span view binding (INC-21; views are non-escaping)", span);
  }

  void Checker::checkAssign(const hir::Statement &statement, const LocalTypes &locals)
  {
    if (statement.assignmentTarget == nullptr)
    {
      const TypeId localType = locals.at(statement.local->value);
      rejectViewReassignment(localType, statement.span);
      if (isBorrowed(statement.local->value))
        throw TypeError("E-borrow-conflict",
                        std::format("cannot assign to this local (of type {}) while it is borrowed (D-002 A1.2)",
                                    interner_.display(localType)),
                        statement.span);
      static_cast<void>(inferExpected(*statement.expression, localType, locals, "assignment value"));
      trackConsumption(*statement.expression, locals);
      moveState_.reinitialize(statement.local->value);
      return;
    }
    const auto &target = *statement.assignmentTarget;
    if (target.kind == hir::ExpressionKind::ResolvedName && target.resolvedName->kind == hir::ResolvedNameKind::Local)
      rejectViewReassignment(locals.at(target.resolvedName->id), statement.span);
    if (const auto root = rootLocalOf(target); root.has_value())
      requireAssignable(root->value, target);
    // The target is typed as a live place: a (partially) moved root is revived
    // for the read of the target type, then restored before the value is
    // checked, and finally revived again by the write itself.
    const MoveState beforeAssignment = moveState_;
    const auto reviveTarget = [&]
    {
      const auto root = rootLocalOf(target);
      if (!root.has_value())
        return;
      if (target.kind != hir::ExpressionKind::Member)
        moveState_.reinitialize(root->value);
      else if (const auto field = memberField(target, locals.at(root->value)); field.has_value())
        moveState_.reinitializeField(root->value, *field);
    };
    reviveTarget();
    requireMutableDeref(target, locals);
    const TypeId targetType = infer(target, locals);
    moveState_ = beforeAssignment;
    static_cast<void>(inferExpected(*statement.expression, targetType, locals, "assignment value"));
    trackConsumption(*statement.expression, locals);
    reviveTarget();
  }

  void Checker::checkReturn(const hir::Statement &statement, const LocalTypes &locals, TypeId returnType)
  {
    if (statement.expression == nullptr)
      requireType(returnType, builtin::Unit, statement.span, "return value");
    else
    {
      const TypeId returned = inferExpected(*statement.expression, returnType, locals, "return value");
      const auto kind = interner_.descriptor(returned).kind;
      if (kind == TypeKind::Reference)
      {
        if (returnOriginOrdinals_.empty())
          throw TypeError("references cannot be returned from a function; declare the origin set on the "
                          "return type (`T ref(a | b)`) (D-002 A1.1)",
                          statement.expression->span);
        if (!returnOriginSatisfied(*statement.expression))
          throw TypeError("returned reference does not satisfy the declared origin(s) `" + returnOriginDisplay_ +
                              "` of the return type (D-002 A1.1)",
                          statement.expression->span);
      }
      if (kind == TypeKind::Span)
        throw TypeError("span views cannot be returned from a function (INC-21; views are scoped, non-escaping)",
                        statement.expression->span);
    }
    for (const auto &[local, type] : locals)
      if (hasDrop(type) && !moveState_.isWholeMoved(local))
        returnDrops_[&statement].push_back(dropEdge(local, type, statement.span));
  }

  void Checker::checkIf(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops,
                        TypeId returnType)
  {
    requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "if condition");
    const MoveState before = moveState_;
    const BorrowState borrowsBefore = borrowState_;
    checkBlock(*statement.consequence, locals, loops, returnType, true);
    const MoveState afterConsequence = moveState_;
    const BorrowState borrowsAfterConsequence = borrowState_;
    moveState_ = before;
    borrowState_ = borrowsBefore;
    if (statement.alternative != nullptr)
      checkBlock(*statement.alternative, locals, loops, returnType, true);
    moveState_ = afterConsequence.mergedWith(moveState_);
    borrowState_ = borrowsAfterConsequence.mergedWith(borrowState_);
  }

  /// `const if` selects one branch at check time; generic functions whose
  /// condition depends on unbound const/type parameters become placeholders
  /// re-checked per instance.
  void Checker::checkConstIf(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops,
                             TypeId returnType)
  {
    bool selected{};
    if (inConstGenericFunction_)
    {
      try
      {
        const_eval::ConstEvaluator evaluator{interner_.constInterner()};
        selected = evaluator.evaluateBool(
            *statement.expression,
            [this](const hir::Expression &node)
            {
              if (node.kind == hir::ExpressionKind::ResolvedName && node.resolvedName.has_value() &&
                  node.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
              {
                const auto found = activeConstBindings_.find(node.text);
                if (found == activeConstBindings_.end())
                  throw const_eval::ConstEvalError(
                      std::format("const if condition references abstract const parameter `{}`", node.text), node.span);
                return found->second;
              }
              if (node.kind == hir::ExpressionKind::GenericApplication)
                return evaluateConstApplication(node);
              if (node.kind == hir::ExpressionKind::Call)
                return evaluateConstCall(node, genericBindings_, activeConstBindings_);
              throw const_eval::ConstEvalError("const if condition is not a compile-time constant expression",
                                               node.span);
            });
      }
      catch (const const_eval::ConstEvalError &error)
      {
        if (activeConstBindings_.empty())
        {
          placeholderFunctions_.insert(currentFunctionId_.value);
          return;
        }
        throw TypeError(error.what(), error.span);
      }
    }
    else
    {
      requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span,
                  "const if condition");
      try
      {
        const_eval::ConstEvaluator evaluator{interner_.constInterner()};
        selected =
            evaluator.evaluateBool(*statement.expression,
                                   [this](const hir::Expression &node)
                                   {
                                     if (node.kind == hir::ExpressionKind::GenericApplication)
                                       return evaluateConstApplication(node);
                                     if (node.kind == hir::ExpressionKind::Call)
                                       return evaluateConstCall(node, genericBindings_, {});
                                     throw const_eval::ConstEvalError(
                                         "const if condition is not a compile-time constant expression", node.span);
                                   });
      }
      catch (const const_eval::ConstEvalError &error)
      {
        if (isGeneric(signatures_.at(currentFunctionId_.value)))
        {
          placeholderFunctions_.insert(currentFunctionId_.value);
          return;
        }
        throw TypeError(error.what(), error.span);
      }
    }
    constIfSelections_.emplace(&statement, selected);
    if (selected)
      checkBlock(*statement.consequence, locals, loops, returnType, true);
    else if (statement.alternative != nullptr)
      checkBlock(*statement.alternative, locals, loops, returnType, true);
    return;
  }

  void Checker::checkLoopBody(const hir::Block &body, LocalTypes locals, LoopTypes loops, TypeId returnType)
  {
    const MoveState before = moveState_;
    const BorrowState borrowsBefore = borrowState_;
    checkBlock(body, std::move(locals), std::move(loops), returnType, true);
    moveState_ = before.mergedWith(moveState_);
    borrowState_ = borrowsBefore.mergedWith(borrowState_);
  }

  void Checker::checkNext(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops)
  {
    const std::vector<TypeId> &expected = statement.nextTarget->kind == hir::NextTargetKind::Loop
                                              ? loops.at(statement.nextTarget->id)
                                              : signatures_.at(statement.nextTarget->id).parameters;
    if (statement.arguments.size() != expected.size())
      throw TypeError(
          std::format("next argument count mismatch: expected {}, got {}", expected.size(), statement.arguments.size()),
          statement.span);
    for (size_t index = 0; index < expected.size(); ++index)
      static_cast<void>(inferExpected(*statement.arguments[index], expected[index], locals,
                                      std::format("next argument {}", index + 1)));
  }

} // namespace NG::typecheck::detail
