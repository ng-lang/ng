// AI-generated code; reviewed for this repository's vNext rewrite.
// typecheck_switch.cpp — switch checking: enum variants (with payload
// bindings and exhaustiveness), scalar literal-or patterns, union
// member/literal patterns (INC-23 DEC-D), and tuple patterns (INC-33).
// Branch move/borrow states join through BranchJoin.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

  void Checker::BranchJoin::check(const hir::Block &branch, const LocalTypes &locals)
  {
    const MoveState beforeBranch = checker.moveState_;
    const BorrowState borrowsBeforeBranch = checker.borrowState_;
    checker.checkBlock(branch, locals, loops, returnType, true);
    merged = merged.mergedWith(checker.moveState_);
    borrowsMerged = borrowsMerged.mergedWith(checker.borrowState_);
    checker.moveState_ = beforeBranch;
    checker.borrowState_ = borrowsBeforeBranch;
    anyBranch = true;
  }

  void Checker::BranchJoin::finish()
  {
    if (!anyBranch)
      return;
    checker.moveState_ = before.mergedWith(merged);
    checker.borrowState_ = borrowsBefore.mergedWith(borrowsMerged);
  }

  auto Checker::beginBranches(const LoopTypes &loops, TypeId returnType, MoveState before) -> BranchJoin
  {
    return BranchJoin{.checker = *this,
                      .loops = loops,
                      .returnType = returnType,
                      .before = std::move(before),
                      .borrowsBefore = borrowState_};
  }

  [[nodiscard]] auto isTuplePattern(const hir::SwitchCase &switchCase) -> bool
  {
    return switchCase.variantName.empty() && switchCase.memberType == nullptr && switchCase.literalTexts.empty() &&
           switchCase.binding.has_value();
  }

  void Checker::checkSwitch(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops,
                            TypeId returnType)
  {
    const TypeId scrutinee = infer(*statement.expression, locals);
    const auto kind = interner_.descriptor(scrutinee).kind;
    if (kind == TypeKind::Union)
      checkUnionSwitch(statement, scrutinee, locals, loops, returnType);
    else if (kind == TypeKind::Tuple)
      checkTupleSwitch(statement, scrutinee, locals, loops, returnType);
    else if (!statement.switchCases.empty() && !statement.switchCases.front().literalTexts.empty())
      checkLiteralSwitch(statement, scrutinee, locals, loops, returnType);
    else
      checkEnumSwitch(statement, scrutinee, locals, loops, returnType);
  }

  void Checker::checkEnumSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                                const LoopTypes &loops, TypeId returnType)
  {
    const auto &descriptor = interner_.descriptor(scrutinee);
    if (descriptor.kind != TypeKind::Enum)
      throw TypeError(std::format("switch value must be an enum type, got {}", interner_.display(scrutinee)),
                      statement.expression->span);
    std::vector<bool> covered(descriptor.fieldNames.size());
    MoveState beforeSwitch = moveState_;
    if (isAffine(scrutinee))
      consumeSwitchScrutinee(*statement.expression, locals, beforeSwitch);
    BranchJoin branches = beginBranches(loops, returnType, std::move(beforeSwitch));
    for (const auto &switchCase : statement.switchCases)
    {
      if (!switchCase.literalTexts.empty())
        throw TypeError("cannot mix literal and variant patterns in one switch", switchCase.span);
      if (isTuplePattern(switchCase))
        throw TypeError("tuple patterns require a tuple switch value", switchCase.span);
      const auto found = std::ranges::find(descriptor.fieldNames, switchCase.variantName);
      if (found == descriptor.fieldNames.end())
        throw TypeError(std::format("unknown variant `{}` for enum `{}`", switchCase.variantName, descriptor.name),
                        switchCase.span);
      const auto variant = static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found));
      if (covered[variant])
        throw TypeError(std::format("duplicate variant `{}` in switch", switchCase.variantName), switchCase.span);
      covered[variant] = true;
      if (!switchCase.binding.has_value())
      {
        branches.check(*switchCase.body, locals);
        continue;
      }
      if (!descriptor.variantHasPayload[variant])
        throw TypeError(std::format("variant `{}` has no payload to bind", switchCase.variantName), switchCase.span);
      LocalTypes caseLocals = locals;
      const auto bind = [&](hir::LocalId local, TypeId type)
      {
        caseLocals.emplace(local.value, type);
        recordLocal(local, type);
      };
      const TypeId payloadType = descriptor.elements[variant];
      if (switchCase.bindings.empty())
      {
        bind(*switchCase.binding, payloadType);
      }
      else
      {
        // `case Cell(value, rest)` binds each field of a tuple payload.
        const auto &payload = interner_.descriptor(payloadType);
        if (payload.kind != TypeKind::Tuple || payload.elements.size() != switchCase.bindings.size() + 1)
          throw TypeError(
              std::format("variant `{}` payload bindings do not match its tuple payload", switchCase.variantName),
              switchCase.span);
        bind(*switchCase.binding, payload.elements.front());
        for (size_t index = 0; index < switchCase.bindings.size(); ++index)
          bind(switchCase.bindings[index], payload.elements[index + 1]);
      }
      branches.check(*switchCase.body, caseLocals);
    }
    if (statement.alternative != nullptr)
      branches.check(*statement.alternative, locals);
    branches.finish();
    if (statement.alternative != nullptr)
      return;
    if (const auto missing = std::ranges::find(covered, false); missing != covered.end())
      throw TypeError(std::format("switch is not exhaustive: missing variant `{}`",
                                  descriptor.fieldNames[static_cast<size_t>(std::distance(covered.begin(), missing))]),
                      statement.span);
  }

  void Checker::checkSwitchLiteral(std::string_view text, TypeId scrutinee, syntax::SourceSpan span) const
  {
    if (scrutinee == builtin::Bool)
    {
      if (text != "true" && text != "false")
        throw TypeError(std::format("literal `{}` does not match switch type bool", text), span);
      return;
    }
    if (!isIntegerBuiltin(scrutinee))
      return;
    if (text.empty() || (text[0] != '-' && !std::isdigit(static_cast<unsigned char>(text[0]))))
      throw TypeError(std::format("literal `{}` does not match switch type {}", text, interner_.display(scrutinee)),
                      span);
    if (isUnsignedIntegerBuiltin(scrutinee))
    {
      checkIntegerLiteralMagnitude(parseIntegerLiteralMagnitude(text, scrutinee, span), text, scrutinee, span);
      return;
    }
    int64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
      throw TypeError(
          std::format("integer literal `{}` is out of range for type {}", text, interner_.display(scrutinee)), span);
    checkIntegerLiteral(value, text, scrutinee, span);
  }

  void Checker::checkLiteralSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                                   const LoopTypes &loops, TypeId returnType)
  {
    if (!isIntegerBuiltin(scrutinee) && scrutinee != builtin::Bool && scrutinee != builtin::String)
      throw TypeError(std::format("literal switch patterns require an integer, bool, or string value, got {}",
                                  interner_.display(scrutinee)),
                      statement.expression->span);
    std::unordered_set<std::string> seen;
    BranchJoin branches = beginBranches(loops, returnType, moveState_);
    for (const auto &switchCase : statement.switchCases)
    {
      if (switchCase.literalTexts.empty())
        throw TypeError("cannot mix literal and variant patterns in one switch", switchCase.span);
      for (const auto &text : switchCase.literalTexts)
      {
        if (!seen.insert(text).second)
          throw TypeError(std::format("duplicate literal `{}` in switch", text), switchCase.span);
        checkSwitchLiteral(text, scrutinee, switchCase.span);
      }
      branches.check(*switchCase.body, locals);
    }
    if (statement.alternative != nullptr)
      branches.check(*statement.alternative, locals);
    branches.finish();
  }

  void Checker::checkUnionSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                                 const LoopTypes &loops, TypeId returnType)
  {
    const auto &descriptor = interner_.descriptor(scrutinee);
    std::vector<bool> covered(descriptor.elements.size());
    BranchJoin branches = beginBranches(loops, returnType, moveState_);
    for (const auto &switchCase : statement.switchCases)
    {
      if (!switchCase.variantName.empty())
        throw TypeError(
            std::format("union switches match member types or literals, not variants (`{}`)", switchCase.variantName),
            switchCase.span);
      if (!switchCase.literalTexts.empty())
      {
        for (const auto &text : switchCase.literalTexts)
          if (memberIndexForUnionLiteral(descriptor, unionLiteralFamily(text)) < 0)
            throw TypeError(std::format("literal `{}` does not match any member type of union {}", text,
                                        interner_.display(scrutinee)),
                            switchCase.span);
        branches.check(*switchCase.body, locals);
        continue;
      }
      if (switchCase.memberType == nullptr || !switchCase.binding.has_value())
        throw TypeError("union member pattern requires a `(name: Type)` binding", switchCase.span);
      const TypeId memberType = interner_.resolveInScope(*switchCase.memberType, genericBindings_);
      const auto found = std::ranges::find(descriptor.elements, memberType);
      if (found == descriptor.elements.end())
        throw TypeError(std::format("`{}` is not a member type of union {}", interner_.display(memberType),
                                    interner_.display(scrutinee)),
                        switchCase.span);
      const auto member = static_cast<size_t>(std::distance(descriptor.elements.begin(), found));
      if (covered[member])
        throw TypeError(
            std::format("duplicate member pattern for type `{}` in union switch", interner_.display(memberType)),
            switchCase.span);
      covered[member] = true;
      LocalTypes caseLocals = locals;
      caseLocals.emplace(switchCase.binding->value, memberType);
      recordLocal(*switchCase.binding, memberType);
      branches.check(*switchCase.body, caseLocals);
    }
    if (statement.alternative != nullptr)
      branches.check(*statement.alternative, locals);
    branches.finish();
    if (statement.alternative != nullptr)
      return;
    if (const auto missing = std::ranges::find(covered, false); missing != covered.end())
    {
      const auto memberName =
          interner_.display(descriptor.elements[static_cast<size_t>(std::distance(covered.begin(), missing))]);
      throw TypeError(std::format("switch is not exhaustive: missing member type `{}` of union {} "
                                  "(member-literal patterns match only their literal value, so add an `otherwise` or a "
                                  "type pattern for `{}`)",
                                  memberName, interner_.display(scrutinee), memberName),
                      statement.span);
    }
  }

  void Checker::checkTupleSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                                 const LoopTypes &loops, TypeId returnType)
  {
    const auto &descriptor = interner_.descriptor(scrutinee);
    bool covered = false;
    BranchJoin branches = beginBranches(loops, returnType, moveState_);
    for (const auto &switchCase : statement.switchCases)
    {
      if (!isTuplePattern(switchCase))
        throw TypeError("cannot mix tuple and other patterns in one switch", switchCase.span);
      const size_t arity = 1 + switchCase.bindings.size();
      if (arity != descriptor.elements.size())
        throw TypeError(std::format("tuple pattern binds {} element{}, but the scrutinee has {}", arity,
                                    arity == 1 ? "" : "s", descriptor.elements.size()),
                        switchCase.span);
      if (covered)
        throw TypeError("duplicate tuple pattern in switch", switchCase.span);
      covered = true;
      LocalTypes caseLocals = locals;
      const auto bind = [&](hir::LocalId local, TypeId type)
      {
        caseLocals.emplace(local.value, type);
        recordLocal(local, type);
      };
      bind(*switchCase.binding, descriptor.elements.front());
      for (size_t index = 0; index < switchCase.bindings.size(); ++index)
        bind(switchCase.bindings[index], descriptor.elements[index + 1]);
      branches.check(*switchCase.body, caseLocals);
    }
    if (statement.alternative != nullptr)
      branches.check(*statement.alternative, locals);
    branches.finish();
    if (statement.alternative != nullptr)
      return;
    if (!covered)
      throw TypeError("switch is not exhaustive: missing a tuple pattern or `otherwise`", statement.span);
  }

} // namespace NG::typecheck::detail
