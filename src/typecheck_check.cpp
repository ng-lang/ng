// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck_check.cpp — function/block/statement checking, borrows, moves.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

namespace
{
  /// D-002 A1.2: human-readable rendering of a place expression for diagnostics
  /// (e.g. `p.y`, `items[0]`, `*r`).
  std::string placeName(const hir::Expression &place)
  {
    switch (place.kind)
    {
    case hir::ExpressionKind::ResolvedName:
      return place.text;
    case hir::ExpressionKind::Member:
      return placeName(*place.operands[0]) + "." + place.text;
    case hir::ExpressionKind::Index:
    {
      const std::string index =
          place.operands.size() > 1 ? place.operands[1]->text : "";
      return placeName(*place.operands[0]) + "[" + index + "]";
    }
    case hir::ExpressionKind::Grouped:
      return placeName(*place.operands[0]);
    case hir::ExpressionKind::Prefix:
      return "*" + placeName(*place.operands[0]);
    default:
      return place.text;
    }
  }
} // namespace

auto Checker::isBorrowed(uint32_t local) const -> bool
{
  const auto found = borrowState_.counts.find(local);
  return found != borrowState_.counts.end() &&
         (found->second.shared != 0 || found->second.mutableRefs != 0);
}

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
    returnOriginOrdinals_.insert(function.returnType->originOrdinals.begin(), function.returnType->originOrdinals.end());
    std::string display;
    for (size_t i = 0; i < function.returnType->originOrdinals.size(); ++i)
    {
      const auto &ordinal = function.returnType->originOrdinals[i];
      if (i > 0) display += " | ";
      if (ordinal < function.parameters.size()) display += function.parameters[ordinal].name;
      else display += std::format("<param{}>", ordinal);
    }
    returnOriginDisplay_ = std::move(display);
  }
  inConstGenericFunction_ = !signature.constParameters.empty();
  if (function.whereClause != nullptr && !isGeneric(signature))
  {
    if (!evaluateWhereCondition(*function.whereClause, Substitution{}, signature))
      throw TypeError(std::format("function `{}` does not satisfy its where clause", function.name),
                      function.span);
  }
  checkBlock(function.body, locals, {}, signature.returnType);
  inConstGenericFunction_ = false;
  recordFallthroughDrops(function, locals);
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
    if (!dropTypes_.contains(type.value) || moveState_.isWholeMoved(local))
      continue;
    fallthroughDrops_[function.id.value].push_back(dropEdge(local, type, function.span));
  }
}

auto Checker::lastUseByBlock(const hir::Block &block) -> const std::unordered_map<uint32_t, size_t> &
{
  if (const auto found = lastUseCache_.find(&block); found != lastUseCache_.end())
    return found->second;
  constexpr size_t neverReleased = std::numeric_limits<size_t>::max();
  std::unordered_map<uint32_t, size_t> lastUse;
  const auto visitExpression = [&](const auto &self, const hir::Expression &expression, size_t index) -> void
  {
    if (expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local)
      lastUse[expression.resolvedName->id] = std::max(lastUse[expression.resolvedName->id], index);
    if (expression.kind == hir::ExpressionKind::Closure)
      for (const auto &capture : expression.closureCaptures)
        lastUse[capture.local.value] = std::max(lastUse[capture.local.value], index);
    for (const auto &operand : expression.operands)
      self(self, *operand, index);
  };
  std::function<void(const hir::Block &, size_t)> visitBlock;
  std::function<void(const hir::Statement &, size_t)> visitStatement;
  visitBlock = [&](const hir::Block &nested, size_t index)
  {
    for (const auto &statement : nested.statements)
      visitStatement(statement, index);
    if (nested.tailExpression)
      visitExpression(visitExpression, *nested.tailExpression, neverReleased);
  };
  visitStatement = [&](const hir::Statement &statement, size_t index)
  {
    if (statement.expression)
      visitExpression(visitExpression, *statement.expression, index);
    // INC-17 (M2b): an explicit-capture closure local is a loan bearer. If the
    // closure is never called, its last use is its declaration (closures cannot
    // escape their scope, INC-14), so the carried loan dies there instead of
    // guarding the rest of the block.
    if (statement.kind == hir::StatementKind::Let && statement.local.has_value() && statement.expression &&
        statement.expression->kind == hir::ExpressionKind::Closure)
      lastUse[statement.local->value] = std::max(lastUse[statement.local->value], index);
    if (statement.assignmentTarget)
      visitExpression(visitExpression, *statement.assignmentTarget, index);
    for (const auto &argument : statement.arguments)
      visitExpression(visitExpression, *argument, index);
    if (statement.consequence)
      visitBlock(*statement.consequence, index);
    if (statement.alternative)
      visitBlock(*statement.alternative, index);
    if (statement.body)
      visitBlock(*statement.body, index);
    for (const auto &switchCase : statement.switchCases)
      if (switchCase.body)
        visitBlock(*switchCase.body, index);
  };
  for (size_t index = 0; index < block.statements.size(); ++index)
    visitStatement(block.statements[index], index);
  if (block.tailExpression)
    visitExpression(visitExpression, *block.tailExpression, neverReleased);
  return lastUseCache_.emplace(&block, std::move(lastUse)).first->second;
}

void Checker::releaseLoansUsedUpTo(size_t statementIndex, const std::unordered_map<uint32_t, size_t> &lastUse,
                                    size_t loansBeforeStatement)
{
  constexpr size_t neverReleased = std::numeric_limits<size_t>::max();
  bool changed = false;
  for (size_t loanIndex = 0; loanIndex < borrowState_.loans.size(); ++loanIndex)
  {
    const auto &loan = borrowState_.loans[loanIndex];
    bool release = false;
    const bool untied = std::find(loan.bearers.begin(), loan.bearers.end(), 0) != loan.bearers.end();
    if (untied)
      release = loanIndex >= loansBeforeStatement;
    else
    {
      // INC-17 (M2b): with several bearers (the ref binding + closure locals
      // capturing it), the loan releases only at the statement where the
      // LAST-surviving bearer is used for the last time.
      size_t latest = 0;
      for (const uint32_t bearer : loan.bearers)
      {
        const auto found = lastUse.find(bearer);
        const size_t expiry = found != lastUse.end() ? found->second : neverReleased;
        latest = std::max(latest, expiry);
      }
      release = latest == statementIndex;
    }
    if (!release)
      continue;
    releasedLoans_.insert(loan.site);
    borrowState_.loans.erase(borrowState_.loans.begin() + static_cast<ptrdiff_t>(loanIndex));
    --loanIndex;
    changed = true;
  }
  if (changed)
    borrowState_.rebuildCounts();
}

void Checker::pruneReleasedLoans()
{
  bool changed = false;
  for (size_t index = 0; index < borrowState_.loans.size(); ++index)
  {
    if (!releasedLoans_.contains(borrowState_.loans[index].site))
      continue;
    borrowState_.loans.erase(borrowState_.loans.begin() + static_cast<ptrdiff_t>(index));
    --index;
    changed = true;
  }
  if (changed)
    borrowState_.rebuildCounts();
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
    std::vector<std::pair<uint32_t, uint32_t>> drops;
    for (const auto &[local, type] : locals)
    {
      if (incomingLocals.contains(local))
        continue;
      if (!dropTypes_.contains(type.value) || moveState_.isWholeMoved(local))
        continue;
      drops.push_back(dropEdge(local, type, block.span));
    }
    if (!drops.empty())
      blockDrops_.emplace(&block, std::move(drops));
    bool changed = false;
    for (size_t index = 0; index < borrowState_.loans.size(); ++index)
    {
      auto &loan = borrowState_.loans[index];
      // INC-17 (M2b): a captured closure local is a block-local bearer. It
      // dies with the block, but the loan itself must stay alive while any
      // outer bearer (the ref binding) survives — keep only surviving bearers.
      std::vector<uint32_t> surviving;
      for (const uint32_t bearer : loan.bearers)
        if (bearer == 0 || incomingLocals.contains(bearer))
          surviving.push_back(bearer);
      if (surviving.empty())
      {
        releasedLoans_.insert(loan.site);
        borrowState_.loans.erase(borrowState_.loans.begin() + static_cast<ptrdiff_t>(index));
        --index;
        changed = true;
        continue;
      }
      if (surviving.size() != loan.bearers.size())
      {
        loan.bearers = std::move(surviving);
        changed = true;
      }
    }
    if (changed)
      borrowState_.rebuildCounts();
  }
  mutableBindings_ = std::move(saved);
  borrowState_ = std::move(borrowsSaved);
  pruneReleasedLoans();
}

void Checker::checkStatement(const hir::Statement &statement, LocalTypes &locals, LoopTypes &loops, TypeId returnType)
{
  switch (statement.kind)
  {
  case hir::StatementKind::Let:
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
    if (statement.expression->kind == hir::ExpressionKind::Closure)
    {
      const auto foundCode = closureFunctions_.find(statement.expression.get());
      if (foundCode != closureFunctions_.end() && statement.local.has_value())
      {
        closureLocals_.emplace(statement.local->value, foundCode->second);
        // INC-14 S3: record capture info for call-site threading.
        const auto foundCaptures = closureCaptures_.find(statement.expression.get());
        if (foundCaptures != closureCaptures_.end())
        {
          std::vector<CaptureLocalInfo> captureInfos;
          const auto &closureExpr = dynamic_cast<const hir::Expression &>(*statement.expression);
          for (size_t i = 0; i < closureExpr.closureCaptures.size(); ++i)
          {
            captureInfos.push_back(CaptureLocalInfo{.localId = closureExpr.closureCaptures[i].local.value,
                                                     .isMove = closureExpr.closureCaptures[i].isMove,
                                                     .type = foundCaptures->second[i]});
          }
          closureCapturesByLocal_.emplace(statement.local->value, std::move(captureInfos));
          // INC-17 (M2b): a closure capturing a live scoped reference becomes
          // an anonymous borrower of that referent. Append the closure local
          // to the captured ref binding's loan bearers, so the loan survives
          // until every bearer (the ref binding and every closure capturing
          // it) is dead. Move/assign of the referent while the closure is
          // alive is then rejected by the usual E-borrow-conflict gates.
          for (auto &loan : borrowState_.loans)
          {
            bool capturesLoan = false;
            for (const auto &capture : closureExpr.closureCaptures)
              if (std::find(loan.bearers.begin(), loan.bearers.end(), capture.local.value) != loan.bearers.end())
              {
                capturesLoan = true;
                break;
              }
            if (!capturesLoan)
              continue;
            if (std::find(loan.bearers.begin(), loan.bearers.end(), statement.local->value) == loan.bearers.end())
            {
              loan.bearers.push_back(statement.local->value);
            }
          }
        }
      }
      else if (statement.local.has_value())
      {
        // INC-14 S4: generic closure literal. Model-typed only; the concrete
        // code function is synthesized when the enclosing generic function is
        // instantiated. Calls through this local defer via the placeholder path.
        deferredClosureLocals_.insert(statement.local->value);
      }
    }
    if (interner_.descriptor(type).kind != TypeKind::TraitReference)
      trackConsumption(*statement.expression, locals);
    if (!statement.destructuredLocals.empty() || statement.restLocal.has_value())
    {
      const auto &tuple = interner_.descriptor(type);
      if (tuple.kind != TypeKind::Tuple)
        throw TypeError(std::format("cannot destructure value of type {}", interner_.display(type)),
                        statement.expression->span);
      if (statement.restLocal.has_value())
      {
        if (tuple.elements.size() < statement.destructuredLocals.size())
          throw TypeError(std::format("tuple destructuring length mismatch: pattern binds {}, tuple has {}",
                                      statement.destructuredLocals.size(), tuple.elements.size()),
                          statement.span);
      }
      else if (tuple.elements.size() != statement.destructuredLocals.size())
      {
        throw TypeError(std::format("tuple destructuring length mismatch: expected {}, got {}",
                                    tuple.elements.size(), statement.destructuredLocals.size()),
                        statement.span);
      }
      for (size_t index = 0; index < statement.destructuredLocals.size(); ++index)
      {
        locals.emplace(statement.destructuredLocals[index].value, tuple.elements[index]);
        recordLocal(statement.destructuredLocals[index], tuple.elements[index]);
        if (statement.mutableBinding)
          mutableBindings_.insert(statement.destructuredLocals[index].value);
      }
      if (statement.restLocal.has_value())
      {
        std::vector<TypeId> restElements;
        restElements.reserve(tuple.elements.size() - statement.destructuredLocals.size());
        for (size_t index = statement.destructuredLocals.size(); index < tuple.elements.size(); ++index)
          restElements.push_back(tuple.elements[index]);
        const TypeId restType = interner_.internTuple(std::move(restElements));
        locals.emplace(statement.restLocal->value, restType);
        recordLocal(*statement.restLocal, restType);
        if (statement.mutableBinding)
          mutableBindings_.insert(statement.restLocal->value);
      }
    }
    else
    {
      locals.emplace(statement.local->value, type);
      recordLocal(*statement.local, type);
      if (statement.mutableBinding)
        mutableBindings_.insert(statement.local->value);
    }
    if (statement.local.has_value() && statement.expression->kind == hir::ExpressionKind::Prefix &&
        (statement.expression->text == "ref" || statement.expression->text == "ref mut"))
    {
      for (auto &loan : borrowState_.loans)
      {
        if (loan.site != statement.expression.get())
          continue;
        auto untied = std::find(loan.bearers.begin(), loan.bearers.end(), 0);
        if (untied != loan.bearers.end())
          *untied = statement.local->value;
        else if (std::find(loan.bearers.begin(), loan.bearers.end(), statement.local->value) == loan.bearers.end())
          loan.bearers.push_back(statement.local->value);
        break;
      }
    }
    return;
  }
  case hir::StatementKind::Assign:
    if (statement.assignmentTarget != nullptr)
    {
      if (statement.assignmentTarget->kind == hir::ExpressionKind::ResolvedName &&
          statement.assignmentTarget->resolvedName->kind == hir::ResolvedNameKind::Local &&
          interner_.descriptor(locals.at(statement.assignmentTarget->resolvedName->id)).kind == TypeKind::Reference)
        throw TypeError("cannot reassign a scoped reference binding (A1.2; references are non-escaping views)",
                        statement.span);
      if (const auto root = rootLocalOf(*statement.assignmentTarget); root.has_value() && isBorrowed(root->value))
        throw TypeError("E-borrow-conflict",
                        std::format("cannot assign to `{}` while it is borrowed (D-002 A1.2)",
                                    placeName(*statement.assignmentTarget)),
                        statement.span);
      const MoveState beforeAssignment = moveState_;
      const auto reviveTarget = [&]()
      {
        if (const auto root = rootLocalOf(*statement.assignmentTarget); root.has_value())
        {
          if (statement.assignmentTarget->kind == hir::ExpressionKind::Member)
          {
            const auto &descriptor = interner_.descriptor(locals.at(root->value));
            const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(),
                                         statement.assignmentTarget->text);
            if (found != descriptor.fieldNames.end())
              moveState_.reinitializeField(
                  root->value, static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found)));
          }
          else
          {
            moveState_.reinitialize(root->value);
          }
        }
      };
      reviveTarget();
      requireMutableDeref(*statement.assignmentTarget, locals);
      const TypeId target = infer(*statement.assignmentTarget, locals);
      moveState_ = beforeAssignment;
      static_cast<void>(inferExpected(*statement.expression, target, locals, "assignment value"));
      trackConsumption(*statement.expression, locals);
      reviveTarget();
    }
    else
    {
      if (interner_.descriptor(locals.at(statement.local->value)).kind == TypeKind::Reference)
        throw TypeError("cannot reassign a scoped reference binding (A1.2; references are non-escaping views)",
                        statement.span);
      if (isBorrowed(statement.local->value))
        throw TypeError(
            "E-borrow-conflict",
            std::format("cannot assign to this local (of type {}) while it is borrowed (D-002 A1.2)",
                        interner_.display(locals.at(statement.local->value))),
            statement.span);
      static_cast<void>(
          inferExpected(*statement.expression, locals.at(statement.local->value), locals, "assignment value"));
      trackConsumption(*statement.expression, locals);
      moveState_.reinitialize(statement.local->value);
    }
    return;
  case hir::StatementKind::Return:
    if (statement.expression != nullptr)
    {
      const TypeId returned = inferExpected(*statement.expression, returnType, locals, "return value");
      if (interner_.descriptor(returned).kind == TypeKind::Reference)
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
    }
    else
      requireType(returnType, builtin::Unit, statement.span, "return value");
    for (const auto &[local, type] : locals)
    {
      if (!dropTypes_.contains(type.value) || moveState_.isWholeMoved(local))
        continue;
      returnDrops_[&statement].push_back(dropEdge(local, type, statement.span));
    }
    return;
  case hir::StatementKind::If:
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
    return;
  }
  case hir::StatementKind::ConstIf:
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
                      std::format("const if condition references abstract const parameter `{}`", node.text),
                      node.span);
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
        selected = evaluator.evaluateBool(*statement.expression,
                                          [this](const hir::Expression &node)
                                          {
                                            if (node.kind == hir::ExpressionKind::GenericApplication)
                                              return evaluateConstApplication(node);
                                            if (node.kind == hir::ExpressionKind::Call)
                                              return evaluateConstCall(node, genericBindings_, {});
                                            throw const_eval::ConstEvalError(
                                                "const if condition is not a compile-time constant expression",
                                                node.span);
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
    loopTypes.emplace(statement.loop->value, types);
    const MoveState before = moveState_;
    const BorrowState borrowsBefore = borrowState_;
    checkBlock(*statement.body, std::move(loopLocals), std::move(loopTypes), returnType, true);
    moveState_ = before.mergedWith(moveState_);
    borrowState_ = borrowsBefore.mergedWith(borrowState_);
    return;
  }
  case hir::StatementKind::Next:
    checkNext(statement, locals, loops);
    return;
  case hir::StatementKind::While:
  {
    requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "while condition");
    const MoveState before = moveState_;
    const BorrowState borrowsBefore = borrowState_;
    checkBlock(*statement.body, locals, loops, returnType, true);
    moveState_ = before.mergedWith(moveState_);
    borrowState_ = borrowsBefore.mergedWith(borrowState_);
    return;
  }
  case hir::StatementKind::Continue:
  case hir::StatementKind::Break:
    // Terminators with a unit value; control leaves the enclosing loop.
    return;
  case hir::StatementKind::Switch:
    checkSwitch(statement, locals, loops, returnType);
    return;
  case hir::StatementKind::Expression:
    static_cast<void>(infer(*statement.expression, locals));
    return;
  }
}

void Checker::checkNext(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops)
{
  const std::vector<TypeId> *expected = statement.nextTarget->kind == hir::NextTargetKind::Loop
                                            ? &loops.at(statement.nextTarget->id)
                                            : &signatures_.at(statement.nextTarget->id).parameters;
  if (statement.arguments.size() != expected->size())
    throw TypeError(std::format("next argument count mismatch: expected {}, got {}", expected->size(),
                                statement.arguments.size()),
                    statement.span);
  for (size_t index = 0; index < expected->size(); ++index)
    static_cast<void>(inferExpected(*statement.arguments[index], (*expected)[index], locals,
                                    std::format("next argument {}", index + 1)));
}

void Checker::checkLiteralSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                                  const LoopTypes &loops, TypeId returnType)
{
  if (!isIntegerBuiltin(scrutinee) && scrutinee != builtin::Bool && scrutinee != builtin::String)
    throw TypeError(std::format("literal switch patterns require an integer, bool, or string value, got {}",
                                interner_.display(scrutinee)),
                    statement.expression->span);
  std::unordered_set<std::string> seen;
  const MoveState beforeSwitch = moveState_;
  const BorrowState borrowsBeforeSwitch = borrowState_;
  MoveState merged;
  BorrowState borrowsMerged;
  bool anyBranch = false;
  const auto checkBranch = [&](const hir::Block &branch, const LocalTypes &branchLocals)
  {
    const MoveState beforeBranch = moveState_;
    const BorrowState borrowsBeforeBranch = borrowState_;
    checkBlock(branch, branchLocals, loops, returnType, true);
    merged = merged.mergedWith(moveState_);
    borrowsMerged = borrowsMerged.mergedWith(borrowState_);
    moveState_ = beforeBranch;
    borrowState_ = borrowsBeforeBranch;
    anyBranch = true;
  };
  for (const auto &switchCase : statement.switchCases)
  {
    if (switchCase.literalTexts.empty())
      throw TypeError("cannot mix literal and variant patterns in one switch", switchCase.span);
    for (const auto &text : switchCase.literalTexts)
    {
      if (!seen.insert(text).second)
        throw TypeError(std::format("duplicate literal `{}` in switch", text), switchCase.span);
      if (isIntegerBuiltin(scrutinee))
      {
        if (text.empty() || (text[0] != '-' && !std::isdigit(static_cast<unsigned char>(text[0]))))
          throw TypeError(
              std::format("literal `{}` does not match switch type {}", text, interner_.display(scrutinee)),
              switchCase.span);
        if (isUnsignedIntegerBuiltin(scrutinee))
        {
          const uint64_t magnitude = parseIntegerLiteralMagnitude(text, scrutinee, switchCase.span);
          checkIntegerLiteralMagnitude(magnitude, text, scrutinee, switchCase.span);
        }
        else
        {
          int64_t value{};
          const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
          if (error != std::errc{} || end != text.data() + text.size())
            throw TypeError(std::format("integer literal `{}` is out of range for type {}", text,
                                        interner_.display(scrutinee)),
                            switchCase.span);
          checkIntegerLiteral(value, text, scrutinee, switchCase.span);
        }
      }
      else if (scrutinee == builtin::Bool)
      {
        if (text != "true" && text != "false")
          throw TypeError(std::format("literal `{}` does not match switch type bool", text), switchCase.span);
      }
    }
    checkBranch(*switchCase.body, locals);
  }
  if (statement.alternative != nullptr)
    checkBranch(*statement.alternative, locals);
  if (anyBranch)
  {
    moveState_ = beforeSwitch.mergedWith(merged);
    borrowState_ = borrowsBeforeSwitch.mergedWith(borrowsMerged);
  }
}

void Checker::checkSwitch(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops,
                           TypeId returnType)
{
  const TypeId scrutinee = infer(*statement.expression, locals);
  if (!statement.switchCases.empty() && !statement.switchCases.front().literalTexts.empty())
  {
    checkLiteralSwitch(statement, scrutinee, locals, loops, returnType);
    return;
  }
  const auto &descriptor = interner_.descriptor(scrutinee);
  if (descriptor.kind != TypeKind::Enum)
    throw TypeError(std::format("switch value must be an enum type, got {}", interner_.display(scrutinee)),
                    statement.expression->span);
  std::vector<bool> covered(descriptor.fieldNames.size());
  const MoveState beforeSwitch = moveState_;
  const BorrowState borrowsBeforeSwitch = borrowState_;
  MoveState merged;
  BorrowState borrowsMerged;
  bool anyBranch = false;
  const auto checkBranch = [&](const hir::Block &branch, const LocalTypes &branchLocals)
  {
    const MoveState beforeBranch = moveState_;
    const BorrowState borrowsBeforeBranch = borrowState_;
    checkBlock(branch, branchLocals, loops, returnType, true);
    merged = merged.mergedWith(moveState_);
    borrowsMerged = borrowsMerged.mergedWith(borrowState_);
    moveState_ = beforeBranch;
    borrowState_ = borrowsBeforeBranch;
    anyBranch = true;
  };
  for (const auto &switchCase : statement.switchCases)
  {
    if (!switchCase.literalTexts.empty())
      throw TypeError("cannot mix literal and variant patterns in one switch", switchCase.span);
    const auto found =
        std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), switchCase.variantName);
    if (found == descriptor.fieldNames.end())
      throw TypeError(std::format("unknown variant `{}` for enum `{}`", switchCase.variantName, descriptor.name),
                      switchCase.span);
    const size_t variant = static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found));
    if (covered[variant])
      throw TypeError(std::format("duplicate variant `{}` in switch", switchCase.variantName), switchCase.span);
    covered[variant] = true;
    if (!switchCase.binding.has_value())
    {
      checkBranch(*switchCase.body, locals);
      continue;
    }
    if (!descriptor.variantHasPayload[variant])
      throw TypeError(std::format("variant `{}` has no payload to bind", switchCase.variantName),
                      switchCase.span);
    LocalTypes caseLocals = locals;
    const TypeId payloadType = descriptor.elements[variant];
    if (!switchCase.bindings.empty())
    {
      const auto &payloadDescriptor = interner_.descriptor(payloadType);
      if (payloadDescriptor.kind != TypeKind::Tuple ||
          payloadDescriptor.elements.size() != switchCase.bindings.size() + 1)
        throw TypeError(
            std::format("variant `{}` payload bindings do not match its tuple payload", switchCase.variantName),
            switchCase.span);
      caseLocals.emplace(switchCase.binding->value, payloadDescriptor.elements.front());
      recordLocal(*switchCase.binding, payloadDescriptor.elements.front());
      for (size_t index = 0; index < switchCase.bindings.size(); ++index)
      {
        caseLocals.emplace(switchCase.bindings[index].value, payloadDescriptor.elements[index + 1]);
        recordLocal(switchCase.bindings[index], payloadDescriptor.elements[index + 1]);
      }
    }
    else
    {
      caseLocals.emplace(switchCase.binding->value, payloadType);
      recordLocal(*switchCase.binding, payloadType);
    }
    checkBranch(*switchCase.body, caseLocals);
  }
  if (statement.alternative != nullptr)
    checkBranch(*statement.alternative, locals);
  if (anyBranch)
  {
    moveState_ = beforeSwitch.mergedWith(merged);
    borrowState_ = borrowsBeforeSwitch.mergedWith(borrowsMerged);
  }
  if (statement.alternative == nullptr)
  {
    if (const auto missing = std::find(covered.begin(), covered.end(), false); missing != covered.end())
    {
      const size_t variant = static_cast<size_t>(std::distance(covered.begin(), missing));
      throw TypeError(
          std::format("switch is not exhaustive: missing variant `{}`", descriptor.fieldNames[variant]),
          statement.span);
    }
  }
}

auto Checker::isPlace(const hir::Expression &expression) -> bool
{
  switch (expression.kind)
  {
  case hir::ExpressionKind::ResolvedName:
    return expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local;
  case hir::ExpressionKind::Index:
  case hir::ExpressionKind::Member:
    return isPlace(*expression.operands[0]);
  case hir::ExpressionKind::Grouped:
    return isPlace(*expression.operands[0]);
  default:
    return false;
  }
}

void Checker::requireMutableRoot(const hir::Expression &expression, syntax::SourceSpan span) const
{
  const hir::Expression *root = &expression;
  while (true)
  {
    if (root->kind == hir::ExpressionKind::Index || root->kind == hir::ExpressionKind::Member ||
        root->kind == hir::ExpressionKind::Grouped)
      root = root->operands[0].get();
    else
      break;
  }
  if (root->kind != hir::ExpressionKind::ResolvedName || root->resolvedName->kind != hir::ResolvedNameKind::Local)
    return;
  if (!mutableBindings_.contains(root->resolvedName->id))
    throw TypeError("cannot create a mutable reference to an immutable binding", span);
}

void Checker::requireMutableDeref(const hir::Expression &expression, const LocalTypes &locals)
{
  switch (expression.kind)
  {
  case hir::ExpressionKind::Index:
  case hir::ExpressionKind::Member:
  case hir::ExpressionKind::Grouped:
    requireMutableDeref(*expression.operands[0], locals);
    return;
  case hir::ExpressionKind::ResolvedName:
    // A1.2 place-through-ref: the assignment writes through a reference when
    // the root binding is itself a scoped reference (`s.field`, `arr[i]`), so
    // the ref must be mutable — mirroring the `*r` path below.
    if (expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local)
    {
      const auto &descriptor = interner_.descriptor(locals.at(expression.resolvedName->id));
      if (descriptor.kind == TypeKind::Reference && !descriptor.referenceMutable)
        throw TypeError("cannot assign through an immutable reference", expression.span);
    }
    return;
  case hir::ExpressionKind::Prefix:
    if (expression.text == "*")
    {
      const TypeId operand = infer(*expression.operands[0], locals);
      const auto &descriptor = interner_.descriptor(operand);
      if (descriptor.kind != TypeKind::Reference)
        throw TypeError(std::format("cannot assign through value of type {}", interner_.display(operand)),
                        expression.span);
      if (!descriptor.referenceMutable)
        throw TypeError("cannot assign through an immutable reference", expression.span);
    }
    return;
  default:
    return;
  }
}

auto Checker::isAffine(TypeId type) const -> bool
{
  const auto &descriptor = interner_.descriptor(type);
  if (descriptor.kind == TypeKind::Struct || descriptor.kind == TypeKind::Enum)
    // D-002 rule 1: Copy-derived types are copyable, so a "move" of one is a
    // copy and must not consume the binding or block later use.
    return !derivedCopyTypes_.contains(type.value);
  if (descriptor.kind == TypeKind::Tuple)
    return std::any_of(descriptor.elements.begin(), descriptor.elements.end(),
                       [this](TypeId element) { return isAffine(element); });
  return false;
}

auto Checker::rootLocalOf(const hir::Expression &expression) const -> std::optional<hir::LocalId>
{
  const hir::Expression *root = &expression;
  while (true)
  {
    if (root->kind == hir::ExpressionKind::Index || root->kind == hir::ExpressionKind::Member ||
        root->kind == hir::ExpressionKind::Grouped)
      root = root->operands[0].get();
    else
      break;
  }
  if (root->kind == hir::ExpressionKind::ResolvedName && root->resolvedName.has_value() &&
      root->resolvedName->kind == hir::ResolvedNameKind::Local)
    return hir::LocalId{root->resolvedName->id};
  return std::nullopt;
}

auto Checker::originRootLocalOf(const hir::Expression &expression) -> std::optional<hir::LocalId>
{
  const hir::Expression *root = &expression;
  while (true)
  {
    if (root->kind == hir::ExpressionKind::Index || root->kind == hir::ExpressionKind::Member ||
        root->kind == hir::ExpressionKind::Grouped)
    {
      root = root->operands[0].get();
      continue;
    }
    if (root->kind == hir::ExpressionKind::Prefix &&
        (root->text == "*" || root->text == "ref" || root->text == "ref mut"))
    {
      root = root->operands[0].get();
      continue;
    }
    break;
  }
  if (root->kind == hir::ExpressionKind::ResolvedName && root->resolvedName.has_value() &&
      root->resolvedName->kind == hir::ResolvedNameKind::Local)
    return hir::LocalId{root->resolvedName->id};
  return std::nullopt;
}

auto Checker::returnOriginSatisfied(const hir::Expression &expression) const -> bool
{
  if (returnOriginOrdinals_.empty()) return false;
  const auto root = originRootLocalOf(expression);
  if (!root.has_value()) return false;
  const auto ordinal = paramOrdinalOf_.find(root->value);
  if (ordinal == paramOrdinalOf_.end()) return false;
  return returnOriginOrdinals_.contains(ordinal->second);
}

void Checker::trackConsumption(const hir::Expression &expression, const LocalTypes &locals)
{
  if (expression.kind == hir::ExpressionKind::Prefix && expression.text == "clone")
    return;
  if (expression.kind == hir::ExpressionKind::Prefix && expression.text == "move")
  {
    const auto root = rootLocalOf(*expression.operands[0]);
    if (!root.has_value())
      return;
    const auto &operand = *expression.operands[0];
    if (isBorrowed(root->value))
      throw TypeError("E-borrow-conflict",
                      std::format("cannot move `{}` while it is borrowed (D-002 A1.2)", placeName(operand)),
                      operand.span);
    if (operand.kind == hir::ExpressionKind::Member)
    {
      const auto &descriptor = interner_.descriptor(locals.at(root->value));
      if ((descriptor.kind == TypeKind::Struct || descriptor.kind == TypeKind::Enum) &&
          !descriptor.fieldNames.empty())
      {
        const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), operand.text);
        if (found != descriptor.fieldNames.end())
        {
          moveState_.markField(root->value,
                                static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found)));
          return;
        }
      }
    }
    moveState_.markWhole(root->value);
    return;
  }
  if (expression.kind == hir::ExpressionKind::Grouped)
  {
    trackConsumption(*expression.operands[0], locals);
    return;
  }
  if (expression.kind == hir::ExpressionKind::Member)
  {
    const auto root = rootLocalOf(expression);
    if (!root.has_value())
      return;
    const auto fieldType = locals.at(root->value);
    if (!isAffine(fieldType))
      return;
    const auto &descriptor = interner_.descriptor(fieldType);
    const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.text);
    if (found == descriptor.fieldNames.end())
      return;
    const uint32_t field = static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found));
    if (isBorrowed(root->value))
      throw TypeError("E-borrow-conflict",
                      std::format("cannot move `{}` while it is borrowed (D-002 A1.2)", placeName(expression)),
                      expression.span);
    if (moveState_.hasMovedField(root->value))
      throw TypeError("E-move-partial",
                      std::format("use of partially moved value `{}`", expression.operands[0]->text),
                      expression.span);
    moveState_.markField(root->value, field);
    return;
  }
  if (expression.kind == hir::ExpressionKind::ResolvedName &&
      expression.resolvedName->kind == hir::ResolvedNameKind::Local)
  {
    const uint32_t local = expression.resolvedName->id;
    if (!isAffine(locals.at(local)))
      return;
    if (isBorrowed(local))
      throw TypeError("E-borrow-conflict",
                      std::format("cannot move `{}` while it is borrowed (D-002 A1.2)", expression.text),
                      expression.span);
    if (moveState_.hasMovedField(local))
      throw TypeError("E-move-partial",
                      std::format("use of partially moved value `{}`", expression.text), expression.span);
    moveState_.markWhole(local);
  }
}

void Checker::validateCaptures(const hir::Expression &closure) const
{
  std::unordered_set<uint32_t> inScope;
  for (const auto parameter : closure.closureParameterIds)
    inScope.insert(parameter.value);
  // Explicitly captured names are shadowed by fresh closure-scope params.
  for (const auto param : closure.closureCaptureParams)
    inScope.insert(param.value);
  std::function<void(const hir::Block &)> collectBlock;
  std::function<void(const hir::Expression &)> visitExpression;
  std::unordered_set<uint32_t> referenced;
  const auto declare = [&](hir::LocalId local)
  {
    inScope.insert(local.value);
  };
  visitExpression = [&](const hir::Expression &expression)
  {
    if (expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local)
      referenced.insert(expression.resolvedName->id);
    for (const auto &operand : expression.operands)
      visitExpression(*operand);
  };
  collectBlock = [&](const hir::Block &block)
  {
    for (const auto &statement : block.statements)
    {
      if (statement.local.has_value())
        declare(*statement.local);
      for (const auto local : statement.destructuredLocals)
        declare(local);
      if (statement.restLocal.has_value())
        declare(*statement.restLocal);
      for (const auto local : statement.loopBindings)
        declare(local);
      for (const auto &switchCase : statement.switchCases)
      {
        if (switchCase.binding.has_value())
          declare(*switchCase.binding);
        for (const auto binding : switchCase.bindings)
          declare(binding);
      }
      if (statement.expression != nullptr)
        visitExpression(*statement.expression);
      if (statement.assignmentTarget != nullptr)
        visitExpression(*statement.assignmentTarget);
      for (const auto &argument : statement.arguments)
        visitExpression(*argument);
      if (statement.consequence != nullptr)
        collectBlock(*statement.consequence);
      if (statement.alternative != nullptr)
        collectBlock(*statement.alternative);
      if (statement.body != nullptr)
        collectBlock(*statement.body);
      for (const auto &switchCase : statement.switchCases)
        if (switchCase.body != nullptr)
          collectBlock(*switchCase.body);
    }
    if (block.tailExpression != nullptr)
      visitExpression(*block.tailExpression);
  };
  const auto firstCapture = [&]() -> std::optional<uint32_t>
  {
    for (const auto local : referenced)
      if (!inScope.contains(local))
        return local;
    return std::nullopt;
  };
  if (closure.closureBody != nullptr)
    collectBlock(*closure.closureBody);
  if (const auto capture = firstCapture())
    throw TypeError(std::format("closure body references local {} which is not listed in the capture list",
                                capture.value()),
                    closure.span);
}

} // namespace NG::typecheck::detail
