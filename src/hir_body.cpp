// AI-generated code; reviewed for this repository's vNext rewrite.
// HIR statement and expression resolution, including the INC-13 (D-007)
// `expr?` desugaring into a scrutinee let plus an Ok/Err switch.
#include "hir_internal.hpp"
#include "syntax/const_expr.hpp"

#include <algorithm>
#include <format>
#include <unordered_set>
#include <utility>

namespace NG::hir
{
  using detail::lowerType;
  using detail::lowerTypeArguments;

  auto Resolver::resolveBlock(syntax::Block &block, bool introduceScope, bool isFunctionBody) -> Block
  {
    if (introduceScope)
    {
      scopes_.emplace_back();
    }

    Block resolved{.span = block.span};
    resolveStatementList(block, isFunctionBody, resolved);

    if (introduceScope)
    {
      scopes_.pop_back();
    }
    return resolved;
  }

  namespace
  {
    auto findTrySlot(syntax::ExpressionPtr &expression) -> syntax::ExpressionPtr *
    {
      if (expression == nullptr)
        return nullptr;
      if (expression->kind == syntax::ExpressionKind::Try)
        return &expression;
      if (auto *grouped = dynamic_cast<syntax::GroupedExpression *>(expression.get()))
        return findTrySlot(grouped->expression);
      if (auto *prefix = dynamic_cast<syntax::PrefixExpression *>(expression.get()))
        return findTrySlot(prefix->operand);
      if (auto *binary = dynamic_cast<syntax::BinaryExpression *>(expression.get()))
      {
        if (auto *slot = findTrySlot(binary->left))
          return slot;
        return findTrySlot(binary->right);
      }
      if (auto *call = dynamic_cast<syntax::CallExpression *>(expression.get()))
      {
        if (auto *slot = findTrySlot(call->callee))
          return slot;
        for (auto &argument : call->arguments)
          if (auto *slot = findTrySlot(argument))
            return slot;
        return nullptr;
      }
      if (auto *index = dynamic_cast<syntax::IndexExpression *>(expression.get()))
      {
        if (auto *slot = findTrySlot(index->receiver))
          return slot;
        return findTrySlot(index->index);
      }
      if (auto *member = dynamic_cast<syntax::MemberExpression *>(expression.get()))
        return findTrySlot(member->receiver);
      if (auto *array = dynamic_cast<syntax::ArrayLiteralExpression *>(expression.get()))
      {
        for (auto &element : array->elements)
          if (auto *slot = findTrySlot(element))
            return slot;
        return nullptr;
      }
      if (auto *tuple = dynamic_cast<syntax::TupleLiteralExpression *>(expression.get()))
      {
        for (auto &element : tuple->elements)
          if (auto *slot = findTrySlot(element))
            return slot;
        return nullptr;
      }
      if (auto *structure = dynamic_cast<syntax::StructLiteralExpression *>(expression.get()))
      {
        for (auto &field : structure->fields)
          if (auto *slot = findTrySlot(field.value))
            return slot;
        return nullptr;
      }
      return nullptr;
    }

    auto findTrySlotInStatement(syntax::Statement &statement) -> syntax::ExpressionPtr *
    {
      if (auto *let = dynamic_cast<syntax::LetStatement *>(&statement))
        return findTrySlot(let->initializer);
      if (auto *ret = dynamic_cast<syntax::ReturnStatement *>(&statement))
        return findTrySlot(ret->value);
      if (auto *expression = dynamic_cast<syntax::ExpressionStatement *>(&statement))
        return findTrySlot(expression->expression);
      if (auto *assign = dynamic_cast<syntax::AssignStatement *>(&statement))
      {
        if (auto *slot = findTrySlot(assign->target))
          return slot;
        return findTrySlot(assign->value);
      }
      if (auto *branch = dynamic_cast<syntax::IfStatement *>(&statement))
        return findTrySlot(branch->condition);
      return nullptr;
    }
  } // namespace

  void Resolver::resolveStatementList(syntax::Block &source, bool isFunctionBody, Block &out)
  {
    out.statements.reserve(source.statements.size());
    for (size_t index = 0; index < source.statements.size(); ++index)
    {
      auto &statement = source.statements[index];
      if (auto *slot = findTrySlotInStatement(*statement))
      {
        desugarTry(statement, *slot, source, index, isFunctionBody, out);
        return;
      }
      out.statements.push_back(resolveStatement(*statement));
    }
    if (source.tailExpression != nullptr)
    {
      if (findTrySlot(source.tailExpression) != nullptr)
      {
        const auto span = source.tailExpression->span;
        if (isFunctionBody)
          source.statements.push_back(
              std::make_unique<syntax::ReturnStatement>(std::move(source.tailExpression), span));
        else
          source.statements.push_back(
              std::make_unique<syntax::ExpressionStatement>(std::move(source.tailExpression), span));
        auto &wrapped = source.statements.back();
        auto *wrappedSlot = findTrySlotInStatement(*wrapped);
        desugarTry(wrapped, *wrappedSlot, source, source.statements.size() - 1, isFunctionBody, out);
        return;
      }
      out.tailExpression = resolveExpression(*source.tailExpression);
    }
  }

  void Resolver::desugarTry(syntax::StatementPtr &statement, syntax::ExpressionPtr &trySlot, syntax::Block &source,
                            size_t statementIndex, bool isFunctionBody, Block &out)
  {
    auto *tryExpr = dynamic_cast<syntax::TryExpression *>(trySlot.get());
    if (tryExpr == nullptr)
      throw ResolutionError("`?` cannot appear mid-expression", trySlot->span);
    const auto trySpan = tryExpr->span;
    if (functionBodyDepth_ == 0)
      throw ResolutionError("cannot use `?` outside of a function body", trySpan);
    if (closureBodyDepth_ > 0)
      throw ResolutionError("`?` is not supported inside a closure body", trySpan);
    if (currentReturnTypeName_.empty() || !currentReturnTypeName_.starts_with("Result"))
    {
      const std::string actual = currentReturnTypeName_.empty() ? std::string{"unit"} : currentReturnTypeName_;
      throw ResolutionError(std::format("`?` requires the enclosing function to return "
                                        "`Result` (it returns `{}`)",
                                        actual),
                            trySpan);
    }
    if (const auto *let = dynamic_cast<const syntax::LetStatement *>(statement.get()))
    {
      if (!let->destructuredNames.empty() || let->restName.has_value())
        throw ResolutionError("`?` with a destructuring `let` is not supported", let->span);
    }

    const std::string tryLocal = std::format("$try{}", nextTryId_);
    const std::string okLocal = std::format("$ok{}", nextTryId_);
    const std::string errorLocal = std::format("$err{}", nextTryId_);
    ++nextTryId_;

    auto operand = std::move(tryExpr->operand);
    bool tryIsWholeReturn = false;
    if (auto *ret = dynamic_cast<syntax::ReturnStatement *>(statement.get()))
    {
      syntax::Expression *root = ret->value.get();
      while (auto *grouped = dynamic_cast<syntax::GroupedExpression *>(root))
        root = grouped->expression.get();
      tryIsWholeReturn = root == tryExpr;
    }
    trySlot = std::make_unique<syntax::IdentifierExpression>(okLocal, trySpan);
    if (tryIsWholeReturn)
    {
      auto *ret = dynamic_cast<syntax::ReturnStatement *>(statement.get());
      std::vector<syntax::ExpressionPtr> okArguments;
      okArguments.push_back(std::move(ret->value));
      ret->value = std::make_unique<syntax::CallExpression>(
          std::make_unique<syntax::MemberExpression>(std::make_unique<syntax::IdentifierExpression>("Result", trySpan),
                                                     "Ok", trySpan),
          std::move(okArguments), trySpan);
    }

    auto scrutinee = std::make_unique<syntax::LetStatement>(tryLocal, false, nullptr, std::move(operand), trySpan);
    out.statements.push_back(resolveStatement(*scrutinee));

    std::vector<syntax::StatementPtr> okStatements;
    okStatements.push_back(std::move(statement));
    for (size_t index = statementIndex + 1; index < source.statements.size(); ++index)
      okStatements.push_back(std::move(source.statements[index]));

    syntax::ExpressionPtr okTail;
    if (source.tailExpression != nullptr)
    {
      if (isFunctionBody)
      {
        okStatements.push_back(
            std::make_unique<syntax::ReturnStatement>(std::exchange(source.tailExpression, nullptr), trySpan));
      }
      else
      {
        okTail = std::exchange(source.tailExpression, nullptr);
      }
    }
    auto okBody = syntax::Block{trySpan, std::move(okStatements), std::move(okTail)};

    std::vector<syntax::StatementPtr> errorStatements;
    std::vector<syntax::ExpressionPtr> errorArguments;
    errorArguments.push_back(std::make_unique<syntax::IdentifierExpression>(errorLocal, trySpan));
    errorStatements.push_back(std::make_unique<syntax::ReturnStatement>(
        std::make_unique<syntax::CallExpression>(
            std::make_unique<syntax::MemberExpression>(
                std::make_unique<syntax::IdentifierExpression>("Result", trySpan), "Err", trySpan),
            std::move(errorArguments), trySpan),
        trySpan));

    std::vector<syntax::SwitchCase> cases;
    cases.push_back(syntax::SwitchCase{syntax::SwitchCasePattern{"Ok", okLocal, {}, trySpan}, std::move(okBody)});
    cases.push_back(syntax::SwitchCase{syntax::SwitchCasePattern{"Err", errorLocal, {}, trySpan},
                                       syntax::Block{trySpan, std::move(errorStatements), nullptr}});

    auto switchStatement = std::make_unique<syntax::SwitchStatement>(
        std::make_unique<syntax::IdentifierExpression>(tryLocal, trySpan), std::move(cases), nullptr, trySpan);
    out.statements.push_back(resolveStatement(*switchStatement));
  }
  auto Resolver::resolveStatement(syntax::Statement &statement) -> Statement
  {
    if (const auto *let = dynamic_cast<const syntax::LetStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Let, .span = let->span};
      if (let->annotation != nullptr)
        resolved.bindingType = std::make_shared<Type>(lowerType(*let->annotation));
      resolved.expression = resolveExpression(*let->initializer);
      resolved.mutableBinding = let->isMutable;
      if (!let->destructuredNames.empty() || let->restName.has_value())
      {
        for (size_t index = 0; index < let->destructuredNames.size(); ++index)
        {
          const LocalId local = declareLocal(let->destructuredNames[index], let->span);
          resolved.destructuredLocals.push_back(local);
          resolved.destructuredIndices.push_back(index);
          localMutability_[local.value] = let->isMutable;
        }
        if (let->restName.has_value())
        {
          resolved.restLocal = declareLocal(*let->restName, let->span);
          localMutability_[resolved.restLocal->value] = let->isMutable;
        }
      }
      else
      {
        resolved.local = declareLocal(let->name, let->span);
        localMutability_[resolved.local->value] = let->isMutable;
      }
      return resolved;
    }

    if (const auto *assign = dynamic_cast<const syntax::AssignStatement *>(&statement))
    {
      const syntax::Expression *root = assign->target.get();
      bool derefTarget{};
      while (true)
      {
        if (const auto *index = dynamic_cast<const syntax::IndexExpression *>(root))
          root = index->receiver.get();
        else if (const auto *member = dynamic_cast<const syntax::MemberExpression *>(root))
          root = member->receiver.get();
        else if (const auto *grouped = dynamic_cast<const syntax::GroupedExpression *>(root))
          root = grouped->expression.get();
        else if (const auto *prefix = dynamic_cast<const syntax::PrefixExpression *>(root);
                 prefix != nullptr && prefix->operatorText == "*")
        {
          derefTarget = true;
          root = prefix->operand.get();
        }
        else
          break;
      }
      const auto *identifier = dynamic_cast<const syntax::IdentifierExpression *>(root);
      if (identifier == nullptr)
        throw ResolutionError("assignment target is not a local place", assign->target->span);
      const auto resolvedRoot = resolveName(*identifier);
      if (resolvedRoot.kind != ResolvedNameKind::Local)
        throw ResolutionError(std::format("assignment target `{}` is not a local binding", identifier->name),
                              assign->target->span);
      // Writing through a reference does not rebind the reference itself; the
      // reference's mutability is enforced by the type checker.
      if (!derefTarget && !localMutability_.at(resolvedRoot.id))
        throw ResolutionError(std::format("cannot assign to immutable binding `{}`", identifier->name),
                              assign->target->span);

      Statement resolved{.kind = StatementKind::Assign,
                         .span = assign->span,
                         .local = LocalId{resolvedRoot.id},
                         .expression = resolveExpression(*assign->value)};
      if (assign->target->kind != syntax::ExpressionKind::Identifier)
        resolved.assignmentTarget = resolveExpression(*assign->target);
      return resolved;
    }

    if (const auto *returnStatement = dynamic_cast<const syntax::ReturnStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Return, .span = returnStatement->span};
      if (returnStatement->value != nullptr)
      {
        // INC-13: `return e?` is hoisted in resolveStatementList before this
        // point, so any remaining `?` is rejected by resolveExpression.
        resolved.expression = resolveExpression(*returnStatement->value);
      }
      return resolved;
    }

    if (auto *ifStatement = dynamic_cast<syntax::IfStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::If, .span = ifStatement->span};
      resolved.expression = resolveExpression(*ifStatement->condition);
      resolved.consequence = std::make_unique<Block>(resolveBlock(ifStatement->consequence, true));
      if (ifStatement->alternative != nullptr)
      {
        resolved.alternative = std::make_unique<Block>(resolveBlock(*ifStatement->alternative, true));
      }
      return resolved;
    }

    if (auto *constIf = dynamic_cast<syntax::ConstIfStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::ConstIf, .span = constIf->span};
      resolved.expression = resolveExpression(*constIf->condition);
      resolved.consequence = std::make_unique<Block>(resolveBlock(constIf->consequence, true));
      if (constIf->alternative != nullptr)
      {
        resolved.alternative = std::make_unique<Block>(resolveBlock(*constIf->alternative, true));
      }
      return resolved;
    }

    if (auto *loop = dynamic_cast<syntax::LoopStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Loop, .span = loop->span};
      std::vector<ExpressionPtr> initializers;
      initializers.reserve(loop->bindings.size());
      for (const auto &binding : loop->bindings)
      {
        initializers.push_back(resolveExpression(*binding.initializer));
      }

      scopes_.emplace_back();
      resolved.loop = LoopId{nextLoop_++};
      for (size_t index = 0; index < loop->bindings.size(); ++index)
      {
        const LocalId local = declareLocal(loop->bindings[index].name, loop->bindings[index].span);
        localMutability_.emplace(local.value, false);
        resolved.loopBindings.push_back(local);
        resolved.arguments.push_back(std::move(initializers[index]));
      }
      loops_.push_back(ActiveLoop{*resolved.loop, /*capturesNext=*/!loop->bindings.empty(), /*isWhile=*/false});
      resolved.body = std::make_unique<Block>(resolveBlock(loop->body, false));
      loops_.pop_back();
      scopes_.pop_back();
      return resolved;
    }

    if (auto *whileLoop = dynamic_cast<syntax::WhileStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::While, .span = whileLoop->span};
      scopes_.emplace_back();
      resolved.loop = LoopId{nextLoop_++};
      resolved.expression = resolveExpression(*whileLoop->condition);
      // `while` never captures bare `next`; tail recursion inside its body
      // still targets the enclosing function (DEC-B).
      loops_.push_back(ActiveLoop{*resolved.loop, /*capturesNext=*/false, /*isWhile=*/true});
      resolved.body = std::make_unique<Block>(resolveBlock(whileLoop->body, false));
      loops_.pop_back();
      scopes_.pop_back();
      return resolved;
    }

    if (const auto *continueStatement = dynamic_cast<const syntax::ContinueStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Continue, .span = continueStatement->span};
      if (loops_.empty())
        throw ResolutionError("`continue` is only valid inside a `while` loop", continueStatement->span);
      // `continue` belongs to `while` only: the nearest enclosing loop-like
      // construct must be a `while`; a `loop` (binderless or not) in between
      // is an error (continue is not a `loop` control).
      if (!loops_.back().isWhile)
        throw ResolutionError("`continue` is not allowed inside a `loop`", continueStatement->span);
      resolved.loop = loops_.back().id;
      return resolved;
    }

    if (const auto *breakStatement = dynamic_cast<const syntax::BreakStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Break, .span = breakStatement->span};
      if (loops_.empty())
        throw ResolutionError("`break` is only valid inside a `loop` or `while` loop", breakStatement->span);
      resolved.loop = loops_.back().id;
      return resolved;
    }

    if (const auto *next = dynamic_cast<const syntax::NextStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Next, .span = next->span};
      resolved.arguments.reserve(next->arguments.size());
      for (const auto &argument : next->arguments)
      {
        resolved.arguments.push_back(resolveExpression(*argument));
      }
      // DEC-B: bare `next` is captured only by the nearest `loop` WITH binders.
      for (auto it = loops_.rbegin(); it != loops_.rend(); ++it)
      {
        if (it->capturesNext)
        {
          resolved.nextTarget = NextTarget{.kind = NextTargetKind::Loop, .id = it->id.value};
          return resolved;
        }
      }
      if (currentFunction_.has_value())
      {
        resolved.nextTarget = NextTarget{.kind = NextTargetKind::Function, .id = currentFunction_->value};
        return resolved;
      }
      throw ResolutionError("next has no enclosing loop or function", next->span);
    }

    if (auto *switchStatement = dynamic_cast<syntax::SwitchStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Switch,
                         .span = switchStatement->span,
                         .expression = resolveExpression(*switchStatement->value)};
      resolved.switchCases.reserve(switchStatement->cases.size());
      for (auto &switchCase : switchStatement->cases)
      {
        SwitchCase caseResolved{.variantName = switchCase.pattern.variantName,
                                .literalTexts = switchCase.pattern.literalTexts,
                                .span = switchCase.pattern.span};
        if (switchCase.pattern.memberType != nullptr)
          caseResolved.memberType = std::make_unique<Type>(lowerType(*switchCase.pattern.memberType));
        if (switchCase.pattern.bindingName.has_value() || !switchCase.pattern.bindingNames.empty())
        {
          scopes_.emplace_back();
          if (switchCase.pattern.bindingName.has_value())
            caseResolved.binding = declareLocal(*switchCase.pattern.bindingName, switchCase.pattern.span);
          for (const auto &bindingName : switchCase.pattern.bindingNames)
            caseResolved.bindings.push_back(declareLocal(bindingName, switchCase.pattern.span));
        }
        caseResolved.body = std::make_unique<Block>(resolveBlock(switchCase.body, false));
        if (switchCase.pattern.bindingName.has_value() || !switchCase.pattern.bindingNames.empty())
          scopes_.pop_back();
        resolved.switchCases.push_back(std::move(caseResolved));
      }
      if (switchStatement->otherwise != nullptr)
        resolved.alternative = std::make_unique<Block>(resolveBlock(*switchStatement->otherwise, true));
      return resolved;
    }

    if (const auto *expression = dynamic_cast<const syntax::ExpressionStatement *>(&statement))
    {
      return Statement{.kind = StatementKind::Expression,
                       .span = expression->span,
                       .expression = resolveExpression(*expression->expression)};
    }

    throw ResolutionError("unsupported statement during name resolution", statement.span);
  }

  namespace
  {
    /// Splits a numeric literal's suffix (`1u8`, `1.5f32`) out of `text` into
    /// `numericSuffix`; the lexer already validated the suffix set.
    void splitNumericSuffix(hir::Expression &resolved)
    {
      static const std::unordered_set<std::string> suffixes{"i8",  "i16",   "i32", "i64", "u8",    "u16",
                                                            "u32", "u64",   "f32", "f64", "isize", "usize"};
      for (const auto &suffix : suffixes)
      {
        if (resolved.text.size() > suffix.size() && resolved.text.ends_with(suffix))
        {
          resolved.numericSuffix = suffix;
          resolved.text.resize(resolved.text.size() - suffix.size());
          return;
        }
      }
    }
  } // namespace

  /// `Enum.Variant` (optionally applied as a constructor call).
  auto Resolver::resolveEnumVariant(const syntax::IdentifierExpression &owner, const syntax::MemberExpression &member,
                                    Expression &resolved) const -> bool
  {
    const auto enumeration = enums_.find(owner.name);
    if (enumeration == enums_.end())
      return false;
    const auto &variants = enumVariants_.at(owner.name);
    const auto variant = std::ranges::find(variants, member.member);
    if (variant == variants.end())
      throw ResolutionError(std::format("unknown variant `{}` in enum `{}`", member.member, owner.name), member.span);
    resolved.kind = ExpressionKind::EnumLiteral;
    resolved.text = owner.name;
    resolved.enumId = enumeration->second;
    resolved.variant = static_cast<uint32_t>(std::distance(variants.begin(), variant));
    return true;
  }

  auto Resolver::resolveExpression(const syntax::Expression &expression) -> ExpressionPtr
  {
    auto resolved = std::make_unique<Expression>(Expression{.span = expression.span});

    // A try operator that is not a statement root (let initializer or bare
    // expression statement) is rejected: INC-13 desugars `?` only at roots.
    if (dynamic_cast<const syntax::TryExpression *>(&expression) != nullptr)
    {
      if (closureBodyDepth_ > 0)
        throw ResolutionError("`?` is not supported inside a closure body", expression.span);
      throw ResolutionError("`?` cannot appear mid-expression", expression.span);
    }

    if (const auto *identifier = dynamic_cast<const syntax::IdentifierExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::ResolvedName;
      resolved->text = identifier->name;
      resolved->resolvedName = resolveName(*identifier);
      if (resolved->resolvedName->kind == ResolvedNameKind::Function)
        resolved->functionCandidates = functions_.at(identifier->name);
      return resolved;
    }
    if (const auto *integer = dynamic_cast<const syntax::IntegerLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::IntegerLiteral;
      resolved->text = integer->text;
      splitNumericSuffix(*resolved);
      return resolved;
    }
    if (const auto *floating = dynamic_cast<const syntax::FloatLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::FloatLiteral;
      resolved->text = floating->text;
      splitNumericSuffix(*resolved);
      return resolved;
    }
    if (const auto *string = dynamic_cast<const syntax::StringLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::StringLiteral;
      resolved->text = string->value;
      return resolved;
    }
    if (const auto *array = dynamic_cast<const syntax::ArrayLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::ArrayLiteral;
      for (const auto &element : array->elements)
        resolved->operands.push_back(resolveExpression(*element));
      return resolved;
    }
    if (const auto *structure = dynamic_cast<const syntax::StructLiteralExpression *>(&expression))
    {
      const auto found = structs_.find(structure->typeName);
      if (found == structs_.end())
        throw ResolutionError(std::format("unknown struct `{}`", structure->typeName), expression.span);
      resolved->kind = ExpressionKind::StructLiteral;
      resolved->text = structure->typeName;
      resolved->structId = found->second;
      for (const auto &field : structure->fields)
      {
        resolved->memberNames.push_back(field.name);
        resolved->operands.push_back(resolveExpression(*field.value));
      }
      return resolved;
    }
    if (const auto *tuple = dynamic_cast<const syntax::TupleLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::TupleLiteral;
      for (const auto &element : tuple->elements)
        resolved->operands.push_back(resolveExpression(*element));
      return resolved;
    }
    if (const auto *boolean = dynamic_cast<const syntax::BooleanLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::BooleanLiteral;
      resolved->text = boolean->value ? "true" : "false";
      return resolved;
    }
    if (const auto *prefix = dynamic_cast<const syntax::PrefixExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::Prefix;
      resolved->text = prefix->operatorText;
      resolved->operands.push_back(resolveExpression(*prefix->operand));
      return resolved;
    }
    if (const auto *grouped = dynamic_cast<const syntax::GroupedExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::Grouped;
      resolved->operands.push_back(resolveExpression(*grouped->expression));
      return resolved;
    }
    if (const auto *call = dynamic_cast<const syntax::CallExpression *>(&expression))
    {
      if (const auto *member = dynamic_cast<const syntax::MemberExpression *>(call->callee.get()))
      {
        if (const auto *owner = dynamic_cast<const syntax::IdentifierExpression *>(member->receiver.get()))
        {
          if (resolveEnumVariant(*owner, *member, *resolved))
          {
            for (const auto &argument : call->arguments)
              resolved->operands.push_back(resolveExpression(*argument));
            return resolved;
          }
        }
        // `receiver.method(args...)`, or the qualified trait call
        // `Trait.method(receiver, args...)` whose callee receiver is an
        // unresolved trait name.
        const auto *owner = dynamic_cast<const syntax::IdentifierExpression *>(member->receiver.get());
        const bool qualified = owner != nullptr && traits_.contains(owner->name);
        resolved->kind = ExpressionKind::Call;
        resolved->text = member->member;
        resolved->methodCall = true;
        auto callee = std::make_unique<Expression>(
            Expression{.kind = ExpressionKind::Member, .span = member->span, .text = member->member});
        callee->operands.push_back(
            qualified ? std::make_unique<Expression>(
                            Expression{.kind = ExpressionKind::ResolvedName, .span = owner->span, .text = owner->name})
                      : resolveExpression(*member->receiver));
        resolved->operands.push_back(std::move(callee));
        for (const auto &argument : call->arguments)
          resolved->operands.push_back(resolveExpression(*argument));
        return resolved;
      }
      if (const auto *application = dynamic_cast<const syntax::GenericApplicationExpression *>(call->callee.get()))
      {
        // Explicit generic arguments on a call: `name<args>(...)`. Resolve the
        // applied name as the call target and keep the arguments on the call.
        syntax::IdentifierExpression identifier{application->name, application->span};
        auto target = std::make_unique<Expression>(Expression{.kind = ExpressionKind::ResolvedName,
                                                              .span = application->span,
                                                              .text = application->name,
                                                              .resolvedName = resolveName(identifier)});
        if (target->resolvedName->kind == ResolvedNameKind::Function)
          target->functionCandidates = functions_.at(application->name);
        resolved->genericArguments = lowerTypeArguments(application->arguments);
        resolved->kind = ExpressionKind::Call;
        resolved->operands.push_back(std::move(target));
        for (const auto &argument : call->arguments)
          resolved->operands.push_back(resolveExpression(*argument));
        return resolved;
      }
      resolved->kind = ExpressionKind::Call;
      resolved->operands.push_back(resolveExpression(*call->callee));
      for (const auto &argument : call->arguments)
      {
        resolved->operands.push_back(resolveExpression(*argument));
      }
      return resolved;
    }
    if (const auto *test = dynamic_cast<const syntax::TypeTestExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::TypeTest;
      resolved->text = test->name;
      resolved->testedType = std::make_unique<Type>(lowerType(*test->testedType));
      return resolved;
    }
    if (const auto *bound = dynamic_cast<const syntax::TraitBoundExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::TraitBound;
      resolved->text = bound->name;
      resolved->traitNames = bound->traitNames;
      return resolved;
    }
    if (const auto *application = dynamic_cast<const syntax::GenericApplicationExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::GenericApplication;
      resolved->text = application->name;
      resolved->genericArguments = lowerTypeArguments(application->arguments);
      return resolved;
    }
    if (const auto *index = dynamic_cast<const syntax::IndexExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::Index;
      if (const auto *integer = dynamic_cast<const syntax::IntegerLiteralExpression *>(index->index.get()))
        resolved->text = integer->text;
      resolved->operands.push_back(resolveExpression(*index->receiver));
      resolved->operands.push_back(resolveExpression(*index->index));
      return resolved;
    }
    if (const auto *member = dynamic_cast<const syntax::MemberExpression *>(&expression))
    {
      if (const auto *owner = dynamic_cast<const syntax::IdentifierExpression *>(member->receiver.get()))
      {
        if (resolveEnumVariant(*owner, *member, *resolved))
          return resolved;
      }
      resolved->kind = ExpressionKind::Member;
      resolved->text = member->member;
      resolved->operands.push_back(resolveExpression(*member->receiver));
      return resolved;
    }
    if (const auto *binary = dynamic_cast<const syntax::BinaryExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::Binary;
      resolved->text = binary->operatorText;
      resolved->operands.push_back(resolveExpression(*binary->left));
      resolved->operands.push_back(resolveExpression(*binary->right));
      return resolved;
    }
    if (const auto *closure = dynamic_cast<const syntax::ClosureExpression *>(&expression))
    {
      scopes_.emplace_back();
      resolved->kind = ExpressionKind::Closure;
      // INC-14 S3: resolve capture names to enclosing-scope locals.
      resolved->closureCaptures.reserve(closure->captures.size());
      resolved->closureCaptureParams.reserve(closure->captures.size());
      for (const auto &capture : closure->captures)
      {
        const LocalId source{resolveName(syntax::IdentifierExpression{capture.name, capture.span}).id};
        // Shadow the captured name with a fresh closure-body-scope local so
        // body references read the captured value (a code-function param),
        // not the enclosing local directly.
        const LocalId param = declareLocal(capture.name, capture.span);
        // INC-17 (M2b): capture params shadow enclosing locals and carry their
        // types (which may be `T ref` / `T ref mut`). The resolver cannot see
        // captured types, so the ref mutability of a write is enforced by the
        // type checker (`requireMutableDeref`) — mirroring the function-param
        // comment in `resolveFunction`: writing through a reference does not
        // rebind the reference itself.
        localMutability_.emplace(param.value, true);
        resolved->closureCaptures.push_back(Expression::CaptureInfo{.local = source, .isMove = capture.isMove});
        resolved->closureCaptureParams.push_back(param);
      }
      resolved->closureParameterNames.reserve(closure->parameters.size());
      resolved->closureParameterIds.reserve(closure->parameters.size());
      resolved->closureParameterTypes.reserve(closure->parameters.size());
      for (const auto &parameter : closure->parameters)
      {
        const LocalId local = declareLocal(parameter.name, parameter.span);
        localMutability_.emplace(local.value, false);
        resolved->closureParameterNames.push_back(parameter.name);
        resolved->closureParameterIds.push_back(local);
        resolved->closureParameterTypes.push_back(
            parameter.type != nullptr ? std::make_shared<Type>(lowerType(*parameter.type)) : nullptr);
      }
      ++closureBodyDepth_;
      resolved->closureBody =
          std::make_unique<Block>(resolveBlock(*const_cast<syntax::Block *>(closure->body.get()), false));
      --closureBodyDepth_;
      scopes_.pop_back();
      return resolved;
    }

    throw ResolutionError("unsupported expression during name resolution", expression.span);
  }
} // namespace NG::hir
