// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4 / INC-21 S2: growth decisions — a conservative, flow-insensitive
// escape analysis over array-typed locals plus classification of `pushBack`
// call sites. A uniquely owned receiver may grow in place (amortized O(1) on
// the array native); a possibly shared receiver takes the copy-on-write path.
// Over-marking a local shared is always sound: it only ever forces the copying
// fallback.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

void Checker::applyGrowthDecisions(const hir::Function &function, const LocalTypes &locals)
{
  (void)locals;
  const auto signature = signatures_.find(function.id.value);
  if (signature == signatures_.end() || isGeneric(signature->second))
    return;

  const auto containerType = [&](TypeId type) -> bool
  {
    return interner_.descriptor(type).kind == TypeKind::DynamicArray;
  };
  const auto localIsContainer = [&](uint32_t local) -> bool
  {
    // `localTypeIds_` carries every local (params, lets, destructured, loop
    // and case bindings); the syntactic `locals` map reaching this point holds
    // only parameters because checkBlock takes it by value.
    const auto found = localTypeIds_.find(local);
    return found != localTypeIds_.end() && containerType(found->second);
  };
  const auto expressionIsContainer = [&](const hir::Expression &expression) -> bool
  {
    const auto found = expressionTypeIds_.find(&expression);
    return found != expressionTypeIds_.end() && containerType(found->second);
  };
  // A binding initialized from a fresh literal owns its buffer outright. Enum
  // literals with operands (`Cons(value, box)`) may reuse a shared tail chain,
  // so only the nullary `Nil` form counts as fresh.
  const auto freshContainer = [](const hir::Expression &expression) -> bool
  {
    if (expression.kind == hir::ExpressionKind::ArrayLiteral)
      return true;
    if (expression.kind == hir::ExpressionKind::EnumLiteral)
      return expression.operands.empty();
    return false;
  };

  std::unordered_set<uint32_t> shared;
  // Parameters start shared: the caller owns the binding and may have copies.
  for (const auto &parameter : function.parameters)
    if (localIsContainer(parameter.local.value))
      shared.insert(parameter.local.value);

  const auto markIfContainerLocal = [&](const hir::Expression &expression)
  {
    if (expression.resolvedName.has_value() &&
        expression.resolvedName->kind == hir::ResolvedNameKind::Local &&
        localIsContainer(expression.resolvedName->id))
      shared.insert(expression.resolvedName->id);
  };
  const auto markBinding = [&](std::optional<hir::LocalId> local)
  {
    if (local.has_value() && localIsContainer(local->value))
      shared.insert(local->value);
  };

  std::function<void(const hir::Expression &, bool)> walkExpression;
  std::function<void(const hir::Block &)> walkBlock;
  std::function<void(const hir::Statement &)> walkStatement;
  // The may-be-shared set must be complete before any call is classified
  // (flow-insensitive), so the walk runs twice: pass 1 collects escapes, pass 2
  // stamps decisions against the frozen set.
  bool decide = false;

  const auto classifyPushBack = [&](const hir::Expression &call)
  {
    if (call.operands.size() != 3)
      return;
    const auto &callee = *call.operands[0];
    if (!callee.resolvedName.has_value() || callee.resolvedName->kind != hir::ResolvedNameKind::Function ||
        callee.text != "pushBack")
      return;
    const auto &receiver = *call.operands[1];
    const auto receiverType = expressionTypeIds_.find(&receiver);
    if (receiverType == expressionTypeIds_.end())
      return;
    const auto &reference = interner_.descriptor(receiverType->second);
    if (reference.kind != TypeKind::Reference)
      return;
    if (interner_.descriptor(reference.element).kind != TypeKind::DynamicArray)
      return;
    const auto root = originRootLocalOf(receiver);
    const bool unique = root.has_value() && !shared.contains(root->value);
    inPlaceMutationCalls_.insert_or_assign(&call, unique);
  };

  walkExpression = [&](const hir::Expression &expression, bool value)
  {
    switch (expression.kind)
    {
    case hir::ExpressionKind::ResolvedName:
      if (value)
        markIfContainerLocal(expression);
      return;
    case hir::ExpressionKind::Prefix:
      if (expression.text == "ref" || expression.text == "ref mut" || expression.text == "span")
      {
        // Borrows and spans alias the buffer without copying it.
        if (!expression.operands.empty())
          walkExpression(*expression.operands[0], false);
        return;
      }
      // `move`, `clone`, deref, and unary operators read the operand value.
      for (const auto &operand : expression.operands)
        walkExpression(*operand, true);
      return;
    case hir::ExpressionKind::Member:
    case hir::ExpressionKind::Index:
      // Place reads do not duplicate the root buffer; index expressions do
      // read value operands.
      if (!expression.operands.empty())
        walkExpression(*expression.operands[0], false);
      for (size_t index = 1; index < expression.operands.size(); ++index)
        walkExpression(*expression.operands[index], true);
      return;
    case hir::ExpressionKind::Call:
      if (expression.methodCall)
      {
        // Method dispatch borrows the receiver; the root read is a place.
        if (!expression.operands.empty())
          walkExpression(*expression.operands[0], false);
      }
      else if (!expression.operands.empty())
      {
        const auto &callee = *expression.operands[0];
        if (callee.resolvedName.has_value() && callee.resolvedName->kind == hir::ResolvedNameKind::Local)
          walkExpression(callee, true);
      }
      for (size_t index = 1; index < expression.operands.size(); ++index)
        walkExpression(*expression.operands[index], true);
      if (decide)
        classifyPushBack(expression);
      return;
    case hir::ExpressionKind::Grouped:
      if (!expression.operands.empty())
        walkExpression(*expression.operands[0], value);
      return;
    case hir::ExpressionKind::Closure:
      // Captured enclosing locals are copied into the closure value.
      for (const auto &capture : expression.closureCaptures)
        if (localIsContainer(capture.local.value))
          shared.insert(capture.local.value);
      return;
    case hir::ExpressionKind::TypeTest:
    case hir::ExpressionKind::TraitBound:
      return;
    default:
      for (const auto &operand : expression.operands)
        walkExpression(*operand, true);
      return;
    }
  };

  walkStatement = [&](const hir::Statement &statement)
  {
    switch (statement.kind)
    {
    case hir::StatementKind::Let:
      if (statement.expression != nullptr)
      {
        walkExpression(*statement.expression, true);
        if (!freshContainer(*statement.expression))
        {
          markBinding(statement.local);
          for (const auto local : statement.destructuredLocals)
            if (localIsContainer(local.value))
              shared.insert(local.value);
          markBinding(statement.restLocal);
        }
      }
      return;
    case hir::StatementKind::Assign:
      if (statement.assignmentTarget != nullptr)
      {
        walkExpression(*statement.assignmentTarget, false);
        const auto root = originRootLocalOf(*statement.assignmentTarget);
        if (root.has_value() && statement.expression != nullptr && expressionIsContainer(*statement.expression))
          shared.insert(root->value);
      }
      if (statement.expression != nullptr)
        walkExpression(*statement.expression, true);
      return;
    case hir::StatementKind::Return:
    case hir::StatementKind::Expression:
      if (statement.expression != nullptr)
        walkExpression(*statement.expression, true);
      return;
    case hir::StatementKind::If:
    case hir::StatementKind::ConstIf:
      if (statement.expression != nullptr)
        walkExpression(*statement.expression, true);
      if (statement.consequence != nullptr)
        walkBlock(*statement.consequence);
      if (statement.alternative != nullptr)
        walkBlock(*statement.alternative);
      return;
    case hir::StatementKind::Loop:
      for (const auto local : statement.loopBindings)
        if (localIsContainer(local.value))
          shared.insert(local.value);
      if (statement.body != nullptr)
        walkBlock(*statement.body);
      if (statement.expression != nullptr)
        walkExpression(*statement.expression, true);
      return;
    case hir::StatementKind::While:
      if (statement.expression != nullptr)
        walkExpression(*statement.expression, true);
      if (statement.body != nullptr)
        walkBlock(*statement.body);
      return;
    case hir::StatementKind::Switch:
      if (statement.expression != nullptr)
        walkExpression(*statement.expression, true);
      for (const auto &switchCase : statement.switchCases)
      {
        markBinding(switchCase.binding);
        for (const auto local : switchCase.bindings)
          if (localIsContainer(local.value))
            shared.insert(local.value);
        if (switchCase.body != nullptr)
          walkBlock(*switchCase.body);
      }
      return;
    case hir::StatementKind::Next:
      for (const auto &argument : statement.arguments)
        walkExpression(*argument, true);
      return;
    case hir::StatementKind::Continue:
    case hir::StatementKind::Break:
      return;
    }
  };

  walkBlock = [&](const hir::Block &block)
  {
    for (const auto &statement : block.statements)
      walkStatement(statement);
    if (block.tailExpression != nullptr)
      walkExpression(*block.tailExpression, true);
  };

  walkBlock(function.body);
  decide = true;
  walkBlock(function.body);
}

} // namespace NG::typecheck::detail
