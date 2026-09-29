// AI-generated code; reviewed for this repository's vNext rewrite.
// typecheck_ownership.cpp — places and ownership: NLL-style loan release
// (last use per block, multi-bearer loans), mutability of places, affine
// consumption (whole/field moves), return-origin contracts, and closure
// capture validation.
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
        return placeName(*place.operands[0]) + "[" + (place.operands.size() > 1 ? place.operands[1]->text : "") + "]";
      case hir::ExpressionKind::Grouped:
        return placeName(*place.operands[0]);
      case hir::ExpressionKind::Prefix:
        return "*" + placeName(*place.operands[0]);
      default:
        return place.text;
      }
    }

    [[nodiscard]] auto isLocalName(const hir::Expression &expression) -> bool
    {
      return expression.kind == hir::ExpressionKind::ResolvedName && expression.resolvedName.has_value() &&
             expression.resolvedName->kind == hir::ResolvedNameKind::Local;
    }

    /// Walks a place through Index/Member/Grouped projections (and the prefix
    /// operators accepted by `throughPrefix`) to its root expression.
    template <typename ThroughPrefix>
    [[nodiscard]] auto placeRoot(const hir::Expression &expression, ThroughPrefix throughPrefix)
        -> const hir::Expression *
    {
      const hir::Expression *root = &expression;
      while (root->kind == hir::ExpressionKind::Index || root->kind == hir::ExpressionKind::Member ||
             root->kind == hir::ExpressionKind::Grouped ||
             (root->kind == hir::ExpressionKind::Prefix && throughPrefix(root->text)))
        root = root->operands[0].get();
      return root;
    }

    /// Visits every block reachable from a statement's nested bodies.
    template <typename Visit>
    void forEachNestedBlock(const hir::Statement &statement, Visit visit)
    {
      for (const auto *nested : {statement.consequence.get(), statement.alternative.get(), statement.body.get()})
        if (nested != nullptr)
          visit(*nested);
      for (const auto &switchCase : statement.switchCases)
        if (switchCase.body != nullptr)
          visit(*switchCase.body);
    }
  } // namespace

  // ---- Loan liveness -----------------------------------------------------------

  auto Checker::isBorrowed(uint32_t local) const -> bool
  {
    const auto found = borrowState_.counts.find(local);
    return found != borrowState_.counts.end() && (found->second.shared != 0 || found->second.mutableRefs != 0);
  }

  auto Checker::lastUseByBlock(const hir::Block &block) -> const std::unordered_map<uint32_t, size_t> &
  {
    if (const auto found = lastUseCache_.find(&block); found != lastUseCache_.end())
      return found->second;
    constexpr size_t neverReleased = std::numeric_limits<size_t>::max();
    std::unordered_map<uint32_t, size_t> lastUse;
    const auto use = [&](uint32_t local, size_t index) { lastUse[local] = std::max(lastUse[local], index); };
    const auto visitExpression = [&](const auto &self, const hir::Expression &expression, size_t index) -> void
    {
      if (isLocalName(expression))
        use(expression.resolvedName->id, index);
      if (expression.kind == hir::ExpressionKind::Closure)
        for (const auto &capture : expression.closureCaptures)
          use(capture.local.value, index);
      for (const auto &operand : expression.operands)
        self(self, *operand, index);
    };
    std::function<void(const hir::Block &, size_t)> visitBlock;
    const auto visitStatement = [&](const hir::Statement &statement, size_t index)
    {
      if (statement.expression)
        visitExpression(visitExpression, *statement.expression, index);
      // INC-17 (M2b): an explicit-capture closure local is a loan bearer. If the
      // closure is never called, its last use is its declaration (closures cannot
      // escape their scope, INC-14), so the carried loan dies there instead of
      // guarding the rest of the block.
      if (statement.kind == hir::StatementKind::Let && statement.local.has_value() && statement.expression &&
          statement.expression->kind == hir::ExpressionKind::Closure)
        use(statement.local->value, index);
      if (statement.assignmentTarget)
        visitExpression(visitExpression, *statement.assignmentTarget, index);
      for (const auto &argument : statement.arguments)
        visitExpression(visitExpression, *argument, index);
      forEachNestedBlock(statement, [&](const hir::Block &nested) { visitBlock(nested, index); });
    };
    visitBlock = [&](const hir::Block &nested, size_t index)
    {
      for (const auto &statement : nested.statements)
        visitStatement(statement, index);
      if (nested.tailExpression)
        visitExpression(visitExpression, *nested.tailExpression, neverReleased);
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
    size_t loanIndex = 0;
    std::erase_if(borrowState_.loans,
                  [&](const BorrowState::Loan &loan)
                  {
                    const size_t position = loanIndex++;
                    bool release = false;
                    // Untied (expression-level) loans end with their statement.
                    if (std::ranges::find(loan.bearers, 0U) != loan.bearers.end())
                      release = position >= loansBeforeStatement;
                    else
                    {
                      // INC-17 (M2b): with several bearers (the ref binding +
                      // closure locals capturing it), the loan releases only at
                      // the last use of the LAST-surviving bearer.
                      size_t latest = 0;
                      for (const uint32_t bearer : loan.bearers)
                      {
                        const auto found = lastUse.find(bearer);
                        latest = std::max(latest, found != lastUse.end() ? found->second : neverReleased);
                      }
                      release = latest == statementIndex;
                    }
                    if (release)
                      releasedLoans_.insert(loan.site);
                    return release;
                  });
    borrowState_.rebuildCounts();
  }

  void Checker::releaseScopeBearers(const std::unordered_set<uint32_t> &outerLocals)
  {
    std::erase_if(borrowState_.loans,
                  [&](BorrowState::Loan &loan)
                  {
                    // INC-17 (M2b): a captured closure local is a block-local
                    // bearer. It dies with the block, but the loan stays alive
                    // while any outer bearer (the ref binding) survives.
                    std::erase_if(loan.bearers,
                                  [&](uint32_t bearer) { return bearer != 0 && !outerLocals.contains(bearer); });
                    if (!loan.bearers.empty())
                      return false;
                    releasedLoans_.insert(loan.site);
                    return true;
                  });
    borrowState_.rebuildCounts();
  }

  void Checker::pruneReleasedLoans()
  {
    std::erase_if(borrowState_.loans,
                  [&](const BorrowState::Loan &loan) { return releasedLoans_.contains(loan.site); });
    borrowState_.rebuildCounts();
  }

  // ---- Places ------------------------------------------------------------------

  auto Checker::isPlace(const hir::Expression &expression) -> bool
  {
    switch (expression.kind)
    {
    case hir::ExpressionKind::ResolvedName:
      return isLocalName(expression);
    case hir::ExpressionKind::Index:
    case hir::ExpressionKind::Member:
    case hir::ExpressionKind::Grouped:
      return isPlace(*expression.operands[0]);
    default:
      return false;
    }
  }

  void Checker::requireMutableRoot(const hir::Expression &expression, syntax::SourceSpan span) const
  {
    const auto *root = placeRoot(expression, [](std::string_view) { return false; });
    if (isLocalName(*root) && !mutableBindings_.contains(root->resolvedName->id))
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
      if (isLocalName(expression))
      {
        const auto &descriptor = interner_.descriptor(locals.at(expression.resolvedName->id));
        if (descriptor.kind == TypeKind::Reference && !descriptor.referenceMutable)
          throw TypeError("cannot assign through an immutable reference", expression.span);
        if (descriptor.kind == TypeKind::Span)
          throw TypeError("cannot assign through a read-only span view (INC-21)", expression.span);
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
      return std::ranges::any_of(descriptor.elements, [this](TypeId element) { return isAffine(element); });
    return false;
  }

  auto Checker::isBox(TypeId type) const -> bool
  {
    const auto &descriptor = interner_.descriptor(type);
    return descriptor.kind == TypeKind::Struct && descriptor.name == "Box" && descriptor.fieldNames.size() == 1 &&
           descriptor.fieldNames.front() == "handle" && descriptor.typeArguments.size() == 1;
  }

  auto Checker::rootLocalOf(const hir::Expression &expression) const -> std::optional<hir::LocalId>
  {
    // INC-21 S2 deref write-back: a `*p` assignment target writes through the
    // reference into its root local, so the loan tracking keys on that root.
    const auto *root = placeRoot(expression, [](std::string_view op) { return op == "*"; });
    if (isLocalName(*root))
      return hir::LocalId{root->resolvedName->id};
    return std::nullopt;
  }

  auto Checker::originRootLocalOf(const hir::Expression &expression) -> std::optional<hir::LocalId>
  {
    const auto *root =
        placeRoot(expression, [](std::string_view op) { return op == "*" || op == "ref" || op == "ref mut"; });
    if (isLocalName(*root))
      return hir::LocalId{root->resolvedName->id};
    return std::nullopt;
  }

  auto Checker::returnOriginSatisfied(const hir::Expression &expression) const -> bool
  {
    if (returnOriginOrdinals_.empty())
      return false;
    const auto root = originRootLocalOf(expression);
    if (!root.has_value())
      return false;
    const auto ordinal = paramOrdinalOf_.find(root->value);
    return ordinal != paramOrdinalOf_.end() && returnOriginOrdinals_.contains(ordinal->second);
  }

  // ---- Moves -------------------------------------------------------------------

  void Checker::requireNotBorrowed(uint32_t root, const hir::Expression &place) const
  {
    if (isBorrowed(root))
      throw TypeError("E-borrow-conflict",
                      std::format("cannot move `{}` while it is borrowed (D-002 A1.2)", placeName(place)), place.span);
  }

  void Checker::requireAssignable(uint32_t root, const hir::Expression &place) const
  {
    if (isBorrowed(root))
      throw TypeError("E-borrow-conflict",
                      std::format("cannot assign to `{}` while it is borrowed (D-002 A1.2)", placeName(place)),
                      place.span);
  }

  auto Checker::memberField(const hir::Expression &member, TypeId rootType) const -> std::optional<uint32_t>
  {
    const auto &descriptor = interner_.descriptor(rootType);
    if (descriptor.kind != TypeKind::Struct && descriptor.kind != TypeKind::Enum)
      return std::nullopt;
    const auto found = std::ranges::find(descriptor.fieldNames, member.text);
    if (found == descriptor.fieldNames.end())
      return std::nullopt;
    return static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found));
  }

  void Checker::trackConsumption(const hir::Expression &expression, const LocalTypes &locals)
  {
    switch (expression.kind)
    {
    case hir::ExpressionKind::Prefix:
    {
      // `clone` copies; an explicit `move` consumes even non-affine values.
      if (expression.text != "move")
        return;
      const auto &operand = *expression.operands[0];
      const auto root = rootLocalOf(operand);
      if (!root.has_value() || moveState_.movedBy(operand))
        return;
      requireNotBorrowed(root->value, operand);
      const auto field =
          operand.kind == hir::ExpressionKind::Member ? memberField(operand, locals.at(root->value)) : std::nullopt;
      if (field.has_value())
        moveState_.markField(root->value, *field);
      else
        moveState_.markWhole(root->value);
      moveState_.sites.insert(&operand);
      return;
    }
    case hir::ExpressionKind::Grouped:
      trackConsumption(*expression.operands[0], locals);
      return;
    case hir::ExpressionKind::Member:
    {
      const auto root = rootLocalOf(expression);
      if (!root.has_value() || !isAffine(locals.at(root->value)) || moveState_.movedBy(expression))
        return;
      const auto field = memberField(expression, locals.at(root->value));
      if (!field.has_value())
        return;
      requireNotBorrowed(root->value, expression);
      if (moveState_.hasMovedField(root->value))
        throw TypeError("E-move-partial",
                        std::format("use of partially moved value `{}`", expression.operands[0]->text),
                        expression.span);
      moveState_.markField(root->value, *field);
      moveState_.sites.insert(&expression);
      return;
    }
    case hir::ExpressionKind::ResolvedName:
    {
      if (expression.resolvedName->kind != hir::ResolvedNameKind::Local)
        return;
      const uint32_t local = expression.resolvedName->id;
      if (!isAffine(locals.at(local)) || moveState_.movedBy(expression))
        return;
      requireNotBorrowed(local, expression);
      if (moveState_.hasMovedField(local))
        throw TypeError("E-move-partial", std::format("use of partially moved value `{}`", expression.text),
                        expression.span);
      moveState_.markWhole(local);
      moveState_.sites.insert(&expression);
      return;
    }
    default:
      return;
    }
  }

  void Checker::consumeSwitchScrutinee(const hir::Expression &expression, const LocalTypes &locals, MoveState &moves)
  {
    if (expression.kind == hir::ExpressionKind::Grouped)
    {
      consumeSwitchScrutinee(*expression.operands[0], locals, moves);
      return;
    }
    if (expression.kind == hir::ExpressionKind::ResolvedName &&
        expression.resolvedName->kind == hir::ResolvedNameKind::Local)
    {
      requireNotBorrowed(expression.resolvedName->id, expression);
      moves.markWhole(expression.resolvedName->id);
      return;
    }
    if (expression.kind != hir::ExpressionKind::Member)
      return;
    const auto root = rootLocalOf(expression);
    if (!root.has_value())
      return;
    const auto field = memberField(expression, locals.at(root->value));
    if (!field.has_value())
      return;
    requireNotBorrowed(root->value, expression);
    moves.markField(root->value, *field);
  }

  // ---- Closure captures --------------------------------------------------------

  void Checker::validateCaptures(const hir::Expression &closure) const
  {
    // Explicitly captured names are shadowed by fresh closure-scope params.
    std::unordered_set<uint32_t> inScope;
    for (const auto parameter : closure.closureParameterIds)
      inScope.insert(parameter.value);
    for (const auto parameter : closure.closureCaptureParams)
      inScope.insert(parameter.value);
    std::unordered_set<uint32_t> referenced;
    const auto visitExpression = [&](const auto &self, const hir::Expression &expression) -> void
    {
      if (isLocalName(expression))
        referenced.insert(expression.resolvedName->id);
      for (const auto &operand : expression.operands)
        self(self, *operand);
    };
    const auto visitBlock = [&](const auto &self, const hir::Block &block) -> void
    {
      for (const auto &statement : block.statements)
      {
        if (statement.local.has_value())
          inScope.insert(statement.local->value);
        for (const auto local : statement.destructuredLocals)
          inScope.insert(local.value);
        if (statement.restLocal.has_value())
          inScope.insert(statement.restLocal->value);
        for (const auto local : statement.loopBindings)
          inScope.insert(local.value);
        for (const auto &switchCase : statement.switchCases)
        {
          if (switchCase.binding.has_value())
            inScope.insert(switchCase.binding->value);
          for (const auto binding : switchCase.bindings)
            inScope.insert(binding.value);
        }
        if (statement.expression != nullptr)
          visitExpression(visitExpression, *statement.expression);
        if (statement.assignmentTarget != nullptr)
          visitExpression(visitExpression, *statement.assignmentTarget);
        for (const auto &argument : statement.arguments)
          visitExpression(visitExpression, *argument);
        forEachNestedBlock(statement, [&](const hir::Block &nested) { self(self, nested); });
      }
      if (block.tailExpression != nullptr)
        visitExpression(visitExpression, *block.tailExpression);
    };
    if (closure.closureBody != nullptr)
      visitBlock(visitBlock, *closure.closureBody);
    for (const auto local : referenced)
      if (!inScope.contains(local))
        throw TypeError(std::format("closure body references local {} which is not listed in the capture list", local),
                        closure.span);
  }

} // namespace NG::typecheck::detail
