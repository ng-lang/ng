// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/hir.hpp"

#include <charconv>
#include <format>
#include <unordered_set>
#include <utility>

namespace NG::vnext::hir
{
  namespace
  {
    [[nodiscard]] auto lowerType(const syntax::TypeSyntax &type) -> Type
    {
      if (const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(&type))
        return Type{.kind = TypeKind::Named, .span = type.span, .name = named->name};
      if (const auto *applied = dynamic_cast<const syntax::AppliedTypeSyntax *>(&type))
      {
        Type result{.kind = TypeKind::Applied, .span = type.span, .target = std::make_unique<Type>(lowerType(*applied->constructor))};
        for (const auto &argument : applied->arguments)
        {
          TypeArgument lowered{.kind = argument.kind, .span = argument.span};
          if (argument.kind == syntax::GenericArgumentKind::Type) lowered.type = std::make_unique<Type>(lowerType(*argument.type));
          else
          {
            const auto [end, error] = std::from_chars(argument.text.data(), argument.text.data() + argument.text.size(), lowered.constInteger);
            if (error != std::errc{} || end != argument.text.data() + argument.text.size())
              throw ResolutionError("const generic integer is out of range", argument.span);
          }
          result.arguments.push_back(std::move(lowered));
        }
        return result;
      }
      if (const auto *reference = dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(&type))
        return Type{.kind = TypeKind::ScopedReference, .span = type.span,
                    .target = std::make_unique<Type>(lowerType(*reference->target)), .isMutable = reference->isMutable};
      if (const auto *pointer = dynamic_cast<const syntax::RawPointerTypeSyntax *>(&type))
        return Type{.kind = TypeKind::RawPointer, .span = type.span,
                    .target = std::make_unique<Type>(lowerType(*pointer->pointee)), .isMutable = pointer->isMutable};
      throw ResolutionError("unsupported type during name resolution", type.span);
    }

    [[nodiscard]] auto renderTypeName(const syntax::TypeSyntax &type) -> std::string
    {
      if (const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(&type))
      {
        return named->name;
      }
      if (const auto *applied = dynamic_cast<const syntax::AppliedTypeSyntax *>(&type))
      {
        std::string result = renderTypeName(*applied->constructor) + "<";
        for (size_t index = 0; index < applied->arguments.size(); ++index)
        {
          if (index != 0) result += ", ";
          const auto &argument = applied->arguments[index];
          result += argument.kind == syntax::GenericArgumentKind::Type ? renderTypeName(*argument.type) : argument.text;
        }
        return result + ">";
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
    structs_.clear();
    enums_.clear();
    enumVariants_.clear();
    uint32_t functionCount{};
    uint32_t structCount{};
    uint32_t enumCount{};
    for (const auto &item : unit.items)
    {
      if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()))
      {
        const auto [_, inserted] = functions_.emplace(function->name, DefId{functionCount++});
        if (!inserted) throw ResolutionError(std::format("duplicate module declaration `{}`", function->name), function->span);
      }
      else if (const auto *structure = dynamic_cast<const syntax::StructDeclaration *>(item.get()))
      {
        const auto [_, inserted] = structs_.emplace(structure->name, StructId{structCount++});
        if (!inserted) throw ResolutionError(std::format("duplicate module declaration `{}`", structure->name), structure->span);
      }
      else if (const auto *enumeration = dynamic_cast<const syntax::EnumDeclaration *>(item.get()))
      {
        const auto [_, inserted] = enums_.emplace(enumeration->name, EnumId{enumCount++});
        if (!inserted) throw ResolutionError(std::format("duplicate module declaration `{}`", enumeration->name), enumeration->span);
        auto &variants = enumVariants_[enumeration->name];
        for (const auto &variant : enumeration->variants) variants.push_back(variant.name);
      }
      else throw ResolutionError("unsupported module item during name resolution", item->span);
    }

    Module module;
    module.functions.reserve(functionCount);
    module.structs.reserve(structCount);
    module.enums.reserve(enumCount);
    for (const auto &item : unit.items)
    {
      if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()))
        module.functions.push_back(resolveFunction(*function, functions_.at(function->name)));
      else if (const auto *structure = dynamic_cast<const syntax::StructDeclaration *>(item.get()))
      {
        module.structs.push_back(resolveStruct(*structure, structs_.at(structure->name)));
      }
      else
      {
        const auto *enumeration = static_cast<const syntax::EnumDeclaration *>(item.get());
        module.enums.push_back(resolveEnum(*enumeration, enums_.at(enumeration->name)));
      }
    }
    return module;
  }

  auto Resolver::resolveEnum(const syntax::EnumDeclaration &enumeration, EnumId id) -> Enum
  {
    Enum resolved{.id = id, .name = enumeration.name, .span = enumeration.span,
                  .genericParameters = enumeration.genericParameters};
    std::unordered_set<std::string> names;
    for (const auto &variant : enumeration.variants)
    {
      if (!names.insert(variant.name).second)
        throw ResolutionError(std::format("duplicate variant `{}` in enum `{}`", variant.name, enumeration.name), variant.span);
      EnumVariant lowered{.name = variant.name, .span = variant.span};
      if (variant.payloadType != nullptr) lowered.payloadType = std::make_unique<Type>(lowerType(*variant.payloadType));
      resolved.variants.push_back(std::move(lowered));
    }
    return resolved;
  }

  auto Resolver::resolveStruct(const syntax::StructDeclaration &structure, StructId id) -> Struct
  {
    Struct resolved{.id = id, .name = structure.name, .span = structure.span};
    std::unordered_set<std::string> names;
    for (const auto &field : structure.fields)
    {
      if (!names.insert(field.name).second)
        throw ResolutionError(std::format("duplicate field `{}` in struct `{}`", field.name, structure.name), field.span);
      resolved.fields.push_back(StructField{.name = field.name, .type = lowerType(*field.type), .span = field.span});
    }
    return resolved;
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
      resolved.parameters.push_back(Parameter{.name = parameter.name,
                                               .typeName = renderTypeName(*parameter.type),
                                               .type = lowerType(*parameter.type),
                                               .local = local,
                                               .span = parameter.span});
    }
    if (function.returnType != nullptr)
    {
      resolved.returnTypeName = renderTypeName(*function.returnType);
      resolved.returnType = std::make_unique<Type>(lowerType(*function.returnType));
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
      resolved.mutableBinding = let->isMutable;
      if (!let->destructuredNames.empty())
      {
        for (size_t index = 0; index < let->destructuredNames.size(); ++index)
        {
          const LocalId local = declareLocal(let->destructuredNames[index], let->span);
          resolved.destructuredLocals.push_back(local);
          resolved.destructuredIndices.push_back(index);
          localMutability_[local.value] = let->isMutable;
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
      while (true)
      {
        if (const auto *index = dynamic_cast<const syntax::IndexExpression *>(root)) root = index->receiver.get();
        else if (const auto *member = dynamic_cast<const syntax::MemberExpression *>(root)) root = member->receiver.get();
        else break;
      }
      const auto *identifier = dynamic_cast<const syntax::IdentifierExpression *>(root);
      if (identifier == nullptr)
        throw ResolutionError("assignment target is not a local place", assign->target->span);
      const auto resolvedRoot = resolveName(*identifier);
      if (resolvedRoot.kind != ResolvedNameKind::Local)
        throw ResolutionError(std::format("assignment target `{}` is not a local binding", identifier->name), assign->target->span);
      if (!localMutability_.at(resolvedRoot.id))
        throw ResolutionError(std::format("cannot assign to immutable binding `{}`", identifier->name), assign->target->span);

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
    if (const auto *string = dynamic_cast<const syntax::StringLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::StringLiteral;
      resolved->text = string->value;
      return resolved;
    }
    if (const auto *array = dynamic_cast<const syntax::ArrayLiteralExpression *>(&expression))
    {
      resolved->kind = ExpressionKind::ArrayLiteral;
      for (const auto &element : array->elements) resolved->operands.push_back(resolveExpression(*element));
      return resolved;
    }
    if (const auto *structure = dynamic_cast<const syntax::StructLiteralExpression *>(&expression))
    {
      const auto found = structs_.find(structure->typeName);
      if (found == structs_.end()) throw ResolutionError(std::format("unknown struct `{}`", structure->typeName), expression.span);
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
      for (const auto &element : tuple->elements) resolved->operands.push_back(resolveExpression(*element));
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
          if (const auto enumeration = enums_.find(owner->name); enumeration != enums_.end())
          {
            const auto &variants = enumVariants_.at(owner->name);
            const auto variant = std::find(variants.begin(), variants.end(), member->member);
            if (variant == variants.end())
              throw ResolutionError(std::format("unknown variant `{}` in enum `{}`", member->member, owner->name), member->span);
            resolved->kind = ExpressionKind::EnumLiteral;
            resolved->text = owner->name;
            resolved->enumId = enumeration->second;
            resolved->variant = static_cast<uint32_t>(std::distance(variants.begin(), variant));
            for (const auto &argument : call->arguments) resolved->operands.push_back(resolveExpression(*argument));
            return resolved;
          }
        }
      }
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
        if (const auto enumeration = enums_.find(owner->name); enumeration != enums_.end())
        {
          const auto &variants = enumVariants_.at(owner->name);
          const auto variant = std::find(variants.begin(), variants.end(), member->member);
          if (variant == variants.end())
            throw ResolutionError(std::format("unknown variant `{}` in enum `{}`", member->member, owner->name), member->span);
          resolved->kind = ExpressionKind::EnumLiteral;
          resolved->text = owner->name;
          resolved->enumId = enumeration->second;
          resolved->variant = static_cast<uint32_t>(std::distance(variants.begin(), variant));
          return resolved;
        }
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
