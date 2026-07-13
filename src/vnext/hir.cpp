// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/hir.hpp"

#include <format>
#include <utility>

namespace NG::vnext::hir
{
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
    nextLocal_ = 0;

    Function resolved{.id = id, .name = function.name, .span = function.span};
    resolved.parameters.reserve(function.parameters.size());
    for (const auto &parameter : function.parameters)
    {
      const LocalId local = declareLocal(parameter.name, parameter.span);
      resolved.parameters.push_back(Parameter{.name = parameter.name, .local = local, .span = parameter.span});
    }
    resolved.body = resolveBlock(function.body, false);
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
      return resolved;
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
