// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/hir.hpp"

#include <format>
#include <utility>

namespace NG::vnext::hir
{
  namespace
  {
    [[nodiscard]] auto renderTypeName(const syntax::TypeSyntax &type) -> std::string
    {
      if (const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(&type))
      {
        return named->name;
      }
      if (const auto *reference = dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(&type))
      {
        return renderTypeName(*reference->target) + (reference->isMutable ? " ref mut" : " ref");
      }
      if (const auto *pointer = dynamic_cast<const syntax::RawPointerTypeSyntax *>(&type))
      {
        return renderTypeName(*pointer->pointee) + (pointer->isMutable ? " *mut" : " *const");
      }
      throw ResolutionError("unsupported type during name resolution", type.span);
    }
  } // namespace

  auto Resolver::resolve(const syntax::SourceUnit &unit) -> Module
  {
    functions_.clear();
    for (const auto &item : unit.items)
    {
      const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get());
      if (function == nullptr)
      {
        throw ResolutionError("unsupported module item during name resolution", item->span);
      }

      const DefId id{static_cast<uint32_t>(functions_.size())};
      const auto [_, inserted] = functions_.emplace(function->name, id);
      if (!inserted)
      {
        throw ResolutionError(std::format("duplicate module declaration `{}`", function->name), function->span);
      }
    }

    Module module;
    module.functions.reserve(unit.items.size());
    for (const auto &item : unit.items)
    {
      const auto &function = static_cast<const syntax::FunctionDeclaration &>(*item);
      module.functions.push_back(resolveFunction(function, functions_.at(function.name)));
    }
    return module;
  }

  auto Resolver::resolveFunction(const syntax::FunctionDeclaration &function, DefId id) -> Function
  {
    scopes_.clear();
    scopes_.emplace_back();
    loops_.clear();
    localMutability_.clear();
    currentFunction_ = id;
    nextLocal_ = 0;
    nextLoop_ = 0;

    Function resolved{.id = id, .name = function.name, .span = function.span};
    resolved.parameters.reserve(function.parameters.size());
    for (const auto &parameter : function.parameters)
    {
      const LocalId local = declareLocal(parameter.name, parameter.span);
      localMutability_.emplace(local.value, false);
      resolved.parameters.push_back(
          Parameter{.name = parameter.name, .typeName = renderTypeName(*parameter.type), .local = local, .span = parameter.span});
    }
    if (function.returnType != nullptr)
    {
      resolved.returnTypeName = renderTypeName(*function.returnType);
    }
    resolved.body = resolveBlock(function.body, false);
    currentFunction_.reset();
    return resolved;
  }

  auto Resolver::resolveBlock(const syntax::Block &block, bool introduceScope) -> Block
  {
    if (introduceScope)
    {
      scopes_.emplace_back();
    }

    Block resolved{.span = block.span};
    resolved.statements.reserve(block.statements.size());
    for (const auto &statement : block.statements)
    {
      resolved.statements.push_back(resolveStatement(*statement));
    }
    if (block.tailExpression != nullptr)
    {
      resolved.tailExpression = resolveExpression(*block.tailExpression);
    }

    if (introduceScope)
    {
      scopes_.pop_back();
    }
    return resolved;
  }

  auto Resolver::resolveStatement(const syntax::Statement &statement) -> Statement
  {
    if (const auto *let = dynamic_cast<const syntax::LetStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Let, .span = let->span};
      resolved.expression = resolveExpression(*let->initializer);
      resolved.local = declareLocal(let->name, let->span);
      resolved.mutableBinding = let->isMutable;
      localMutability_[resolved.local->value] = let->isMutable;
      return resolved;
    }

    if (const auto *assign = dynamic_cast<const syntax::AssignStatement *>(&statement))
    {
      const syntax::IdentifierExpression target{assign->name, assign->span};
      const auto resolvedTarget = resolveName(target);
      if (resolvedTarget.kind != ResolvedNameKind::Local)
      {
        throw ResolutionError(std::format("assignment target `{}` is not a local binding", assign->name), assign->span);
      }
      if (!localMutability_.at(resolvedTarget.id))
      {
        throw ResolutionError(std::format("cannot assign to immutable binding `{}`", assign->name), assign->span);
      }
      return Statement{.kind = StatementKind::Assign,
                       .span = assign->span,
                       .local = LocalId{resolvedTarget.id},
                       .expression = resolveExpression(*assign->value)};
    }

    if (const auto *returnStatement = dynamic_cast<const syntax::ReturnStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Return, .span = returnStatement->span};
      if (returnStatement->value != nullptr)
      {
        resolved.expression = resolveExpression(*returnStatement->value);
      }
      return resolved;
    }

    if (const auto *ifStatement = dynamic_cast<const syntax::IfStatement *>(&statement))
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

    if (const auto *loop = dynamic_cast<const syntax::LoopStatement *>(&statement))
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
      loops_.push_back(ActiveLoop{*resolved.loop});
      resolved.body = std::make_unique<Block>(resolveBlock(loop->body, false));
      loops_.pop_back();
      scopes_.pop_back();
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
      if (!loops_.empty())
      {
        resolved.nextTarget = NextTarget{.kind = NextTargetKind::Loop, .id = loops_.back().id.value};
      }
      else if (currentFunction_.has_value())
      {
        resolved.nextTarget = NextTarget{.kind = NextTargetKind::Function, .id = currentFunction_->value};
      }
      else
      {
        throw ResolutionError("next has no enclosing loop or function", next->span);
      }
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

  auto Resolver::resolveExpression(const syntax::Expression &expression) -> ExpressionPtr
  {
    auto resolved = std::make_unique<Expression>(Expression{.span = expression.span});

    if (const auto *identifier = dynamic_cast<const syntax::IdentifierExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::ResolvedName;
      resolved->text = identifier->name;
      resolved->resolvedName = resolveName(*identifier);
      return resolved;
    }
    if (const auto *integer = dynamic_cast<const syntax::IntegerLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::IntegerLiteral;
      resolved->text = integer->text;
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
      resolved->kind = ExpressionKind::Call;
      resolved->operands.push_back(resolveExpression(*call->callee));
      for (const auto &argument : call->arguments)
      {
        resolved->operands.push_back(resolveExpression(*argument));
      }
      return resolved;
    }
    if (const auto *index = dynamic_cast<const syntax::IndexExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::Index;
      resolved->operands.push_back(resolveExpression(*index->receiver));
      resolved->operands.push_back(resolveExpression(*index->index));
      return resolved;
    }
    if (const auto *member = dynamic_cast<const syntax::MemberExpression *>(&expression))
    {
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

    throw ResolutionError("unsupported expression during name resolution", expression.span);
  }

  auto Resolver::resolveName(const syntax::IdentifierExpression &expression) const -> ResolvedName
  {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope)
    {
      if (const auto local = scope->find(expression.name); local != scope->end())
      {
        return ResolvedName{.kind = ResolvedNameKind::Local, .id = local->second.value};
      }
    }
    if (const auto function = functions_.find(expression.name); function != functions_.end())
    {
      return ResolvedName{.kind = ResolvedNameKind::Function, .id = function->second.value};
    }
    throw ResolutionError(std::format("unresolved name `{}`", expression.name), expression.span);
  }

  auto Resolver::declareLocal(const std::string &name, syntax::SourceSpan span) -> LocalId
  {
    auto &scope = scopes_.back();
    if (scope.contains(name))
    {
      throw ResolutionError(std::format("duplicate local binding `{}`", name), span);
    }
    const LocalId local{nextLocal_++};
    scope.emplace(name, local);
    return local;
  }
} // namespace NG::vnext::hir
