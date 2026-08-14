// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/hir.hpp"
#include "vnext/syntax/const_expr.hpp"

#include <array>
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
          else lowered.constExpr = syntax::cloneConstExpr(*argument.constExpr);
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
      if (const auto *pack = dynamic_cast<const syntax::PackTypeSyntax *>(&type))
        return Type{.kind = TypeKind::Pack, .span = type.span,
                    .target = std::make_unique<Type>(lowerType(*pack->target))};
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
          result += argument.kind == syntax::GenericArgumentKind::Type ? renderTypeName(*argument.type)
                                                                       : syntax::renderConstExpr(*argument.constExpr);
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
      if (const auto *pack = dynamic_cast<const syntax::PackTypeSyntax *>(&type))
      {
        return renderTypeName(*pack->target) + "...";
      }
      throw ResolutionError("unsupported type during name resolution", type.span);
    }
  } // namespace

  auto Resolver::resolve(const syntax::SourceUnit &unit) -> Module
  {
    functions_.clear();
    functionIds_.clear();
    structs_.clear();
    enums_.clear();
    opaqueTypes_.clear();
    enumVariants_.clear();
    nextLocal_ = 0;
    nextConstId_ = 0;
    uint32_t functionCount{};
    uint32_t structCount{};
    uint32_t enumCount{};
    uint32_t constCount{};
    uint32_t implMethodCount{};
    for (const auto &item : unit.items)
    {
      if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()))
      {
        const DefId id{functionCount++};
        functions_[function->name].push_back(id);
        functionIds_.emplace(function, id);
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
      else if (const auto *declaration = dynamic_cast<const syntax::ConstDeclaration *>(item.get()))
      {
        static_cast<void>(declaration);
        ++constCount;
      }
      else if (const auto *opaque = dynamic_cast<const syntax::OpaqueTypeDeclaration *>(item.get()))
      {
        if (!opaqueTypes_.emplace(opaque->name, opaque->span).second)
          throw ResolutionError(std::format("duplicate module declaration `{}`", opaque->name), opaque->span);
      }
      else if (const auto *trait = dynamic_cast<const syntax::TraitDeclaration *>(item.get()))
      {
        if (!traits_.emplace(trait->name, trait->span).second)
          throw ResolutionError(std::format("duplicate module declaration `{}`", trait->name), trait->span);
      }
      else if (const auto *impl = dynamic_cast<const syntax::ImplDeclaration *>(item.get()))
      {
        implMethodCount += impl->methods.size();
      }
      else if (dynamic_cast<const syntax::ImportDeclaration *>(item.get()) != nullptr)
      {
        // Import directives are consumed by the module loader; merged units
        // no longer carry them.
      }
      else throw ResolutionError("unsupported module item during name resolution", item->span);
    }

    Module module;
    module.functions.reserve(functionCount);
    module.structs.reserve(structCount);
    module.enums.reserve(enumCount);
    module.consts.reserve(constCount);
    // Functions resolve first so their DefIds equal their positions in
    // module.functions; impl/default methods follow with consecutive ids.
    for (const auto &item : unit.items)
    {
      if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()))
        module.functions.push_back(resolveFunction(*function, functionIds_.at(function)));
    }
    for (const auto &item : unit.items)
    {
      if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()))
      {
        static_cast<void>(function);
      }
      else if (const auto *structure = dynamic_cast<const syntax::StructDeclaration *>(item.get()))
      {
        module.structs.push_back(resolveStruct(*structure, structs_.at(structure->name)));
      }
      else if (const auto *enumeration = dynamic_cast<const syntax::EnumDeclaration *>(item.get()))
      {
        module.enums.push_back(resolveEnum(*enumeration, enums_.at(enumeration->name)));
      }
      else if (const auto *declaration = dynamic_cast<const syntax::ConstDeclaration *>(item.get()))
      {
        module.consts.push_back(resolveConstDeclaration(*declaration, DefId{nextConstId_++}));
      }
      else if (const auto *opaque = dynamic_cast<const syntax::OpaqueTypeDeclaration *>(item.get()))
      {
        module.opaqueTypes.push_back(OpaqueType{.name = opaque->name, .abstract = opaque->abstract, .span = opaque->span});
      }
      else if (const auto *trait = dynamic_cast<const syntax::TraitDeclaration *>(item.get()))
      {
        module.traits.push_back(resolveTrait(*trait));
        for (auto &method : module.traits.back().methods)
        {
          if (!method.body.has_value()) continue;
          Function lowered{.id = DefId{static_cast<uint32_t>(module.functions.size())},
                           .name = std::format("default${}${}", trait->name, method.name),
                           .span = method.span};
          if (method.returnType != nullptr) lowered.returnType = std::move(method.returnType);
          lowered.parameters = std::move(method.parameters);
          lowered.body = std::move(*method.body);
          module.traits.back().methodIds.push_back(lowered.id);
          module.functions.push_back(std::move(lowered));
          ++nextImplMethodId_;
        }
      }
      else if (dynamic_cast<const syntax::ImportDeclaration *>(item.get()) != nullptr)
      {
      }
      else if (const auto *impl = dynamic_cast<const syntax::ImplDeclaration *>(item.get()))
      {
        module.impls.push_back(resolveImpl(*impl));
        for (auto &method : module.impls.back().methods)
        {
          module.impls.back().methodIds.push_back(DefId{static_cast<uint32_t>(module.functions.size())});
          Function lowered{.id = DefId{static_cast<uint32_t>(module.functions.size())},
                           .name = std::format("impl${}${}${}", impl->traitName, method.name,
                                               renderTypeName(*impl->targetType)),
                           .span = method.span};
          if (method.returnType != nullptr) lowered.returnType = std::move(method.returnType);
          lowered.parameters = std::move(method.parameters);
          if (method.body.has_value()) lowered.body = std::move(*method.body);
          module.functions.push_back(std::move(lowered));
          ++nextImplMethodId_;
        }
      }
    }
    return module;
  }

  auto Resolver::resolveTraitMethod(const syntax::TraitMethodDeclaration &method) -> TraitMethod
  {
    scopes_.emplace_back();
    TraitMethod resolved{.name = method.name, .span = method.span};
    for (const auto &parameter : method.parameters)
    {
      const LocalId local = declareLocal(parameter.name, parameter.span);
      resolved.parameters.push_back(Parameter{.name = parameter.name,
                                               .typeName = renderTypeName(*parameter.type),
                                               .type = lowerType(*parameter.type),
                                               .local = local,
                                               .span = parameter.span});
    }
    if (method.returnType != nullptr) resolved.returnType = std::make_unique<Type>(lowerType(*method.returnType));
    if (method.body.has_value()) resolved.body = resolveBlock(*method.body, false);
    scopes_.pop_back();
    return resolved;
  }

  auto Resolver::resolveTrait(const syntax::TraitDeclaration &declaration) -> Trait
  {
    Trait resolved{.name = declaration.name, .supertraits = declaration.supertraits,
                    .autoTrait = declaration.autoTrait, .span = declaration.span};
    for (const auto &method : declaration.methods) resolved.methods.push_back(resolveTraitMethod(method));
    return resolved;
  }

  auto Resolver::resolveImpl(const syntax::ImplDeclaration &declaration) -> Impl
  {
    Impl resolved{.traitName = declaration.traitName, .span = declaration.span};
    if (declaration.targetType != nullptr) resolved.targetType = std::make_unique<Type>(lowerType(*declaration.targetType));
    for (const auto &method : declaration.methods) resolved.methods.push_back(resolveTraitMethod(method));
    return resolved;
  }

  auto Resolver::resolveConstDeclaration(const syntax::ConstDeclaration &declaration, DefId id) -> ConstDeclaration
  {
    ConstDeclaration resolved{.id = id,
                              .name = declaration.name,
                              .span = declaration.span,
                              .bodyKind = declaration.bodyKind == syntax::ConstBodyKind::Native
                                              ? ConstDeclarationBodyKind::Native
                                              : declaration.bodyKind == syntax::ConstBodyKind::Delete
                                                    ? ConstDeclarationBodyKind::Delete
                                                    : ConstDeclarationBodyKind::Expression};
    for (const auto &parameter : declaration.parameters) resolved.typeParameters.push_back(parameter.name);
    // Bare identifiers appearing as top-level pattern arguments introduce
    // type parameters implicitly, but only for prefix-less (primary)
    // declarations: `const<T> name<...>` declares its parameters explicitly.
    // Names that already denote a concrete type (builtins, structs, enums)
    // are never introduced as parameters.
    if (declaration.parameters.empty())
    {
      constexpr std::array<std::string_view, 5> BuiltinNames{"i64", "u8", "bool", "unit", "string"};
      for (const auto &argument : declaration.patternArguments)
      {
        if (const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(argument.get()); named != nullptr)
        {
          const bool concrete = std::find(BuiltinNames.begin(), BuiltinNames.end(), named->name) != BuiltinNames.end() ||
                                structs_.contains(named->name) || enums_.contains(named->name);
          if (!concrete && std::find(resolved.typeParameters.begin(), resolved.typeParameters.end(), named->name) ==
                               resolved.typeParameters.end())
            resolved.typeParameters.push_back(named->name);
        }
      }
    }
    std::unordered_set<std::string> names{resolved.typeParameters.begin(), resolved.typeParameters.end()};
    if (names.size() != resolved.typeParameters.size())
      throw ResolutionError(std::format("duplicate generic parameter in const declaration `{}`", declaration.name),
                            declaration.span);
    for (const auto &argument : declaration.patternArguments)
      resolved.pattern.push_back(std::make_unique<Type>(lowerType(*argument)));
    if (declaration.targetType != nullptr)
      resolved.targetType = std::make_unique<Type>(lowerType(*declaration.targetType));
    if (declaration.body != nullptr) resolved.body = cloneConstExpr(*declaration.body);
    return resolved;
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
    Struct resolved{.id = id, .name = structure.name, .span = structure.span,
                    .genericParameters = structure.genericParameters, .derivedTraits = structure.derivedTraits};
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
    nextLoop_ = 0;

    constParameters_.clear();
    Function resolved{.id = id, .name = function.name, .span = function.span, .constFunction = function.constFunction,
                      .exported = function.exported, .nativeFunction = function.nativeFunction};
    for (const auto &parameter : function.genericParameters)
    {
      resolved.genericParameterOrder.push_back(parameter.kind);
      if (parameter.kind == syntax::GenericParameterKind::Const)
      {
        resolved.constParameters.push_back(ConstParameter{.name = parameter.name,
                                                          .typeName = renderTypeName(*parameter.type),
                                                          .type = lowerType(*parameter.type),
                                                          .span = parameter.span});
        constParameters_.emplace(parameter.name, static_cast<uint32_t>(resolved.constParameters.size() - 1));
      }
      else
      {
        if (parameter.kind == syntax::GenericParameterKind::Pack) resolved.packParameters.push_back(parameter.name);
        else if (parameter.kind == syntax::GenericParameterKind::TypeConstructor)
          resolved.constructorParameters.push_back(parameter.name);
        else resolved.genericParameters.push_back(parameter.name);
        if (!parameter.traitBounds.empty()) resolved.traitBounds.emplace_back(parameter.name, parameter.traitBounds);
      }
    }
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
    if (function.whereClause != nullptr) resolved.whereClause = resolveExpression(*function.whereClause);
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
      if (let->annotation != nullptr) resolved.bindingType = std::make_shared<Type>(lowerType(*let->annotation));
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
      bool derefTarget{};
      while (true)
      {
        if (const auto *index = dynamic_cast<const syntax::IndexExpression *>(root)) root = index->receiver.get();
        else if (const auto *member = dynamic_cast<const syntax::MemberExpression *>(root)) root = member->receiver.get();
        else if (const auto *grouped = dynamic_cast<const syntax::GroupedExpression *>(root)) root = grouped->expression.get();
        else if (const auto *prefix = dynamic_cast<const syntax::PrefixExpression *>(root); prefix != nullptr && prefix->operatorText == "*")
        {
          derefTarget = true;
          root = prefix->operand.get();
        }
        else break;
      }
      const auto *identifier = dynamic_cast<const syntax::IdentifierExpression *>(root);
      if (identifier == nullptr)
        throw ResolutionError("assignment target is not a local place", assign->target->span);
      const auto resolvedRoot = resolveName(*identifier);
      if (resolvedRoot.kind != ResolvedNameKind::Local)
        throw ResolutionError(std::format("assignment target `{}` is not a local binding", identifier->name), assign->target->span);
      // Writing through a reference does not rebind the reference itself; the
      // reference's mutability is enforced by the type checker.
      if (!derefTarget && !localMutability_.at(resolvedRoot.id))
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

    if (const auto *constIf = dynamic_cast<const syntax::ConstIfStatement *>(&statement))
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

    if (const auto *switchStatement = dynamic_cast<const syntax::SwitchStatement *>(&statement))
    {
      Statement resolved{.kind = StatementKind::Switch,
                         .span = switchStatement->span,
                         .expression = resolveExpression(*switchStatement->value)};
      resolved.switchCases.reserve(switchStatement->cases.size());
      for (const auto &switchCase : switchStatement->cases)
      {
        SwitchCase caseResolved{.variantName = switchCase.pattern.variantName, .span = switchCase.pattern.span};
        if (switchCase.pattern.bindingName.has_value())
        {
          scopes_.emplace_back();
          caseResolved.binding = declareLocal(*switchCase.pattern.bindingName, switchCase.pattern.span);
        }
        caseResolved.body = std::make_unique<Block>(resolveBlock(switchCase.body, false));
        if (switchCase.pattern.bindingName.has_value()) scopes_.pop_back();
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

  auto Resolver::resolveExpression(const syntax::Expression &expression) -> ExpressionPtr
  {
    auto resolved = std::make_unique<Expression>(Expression{.span = expression.span});

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
          if (traits_.contains(owner->name))
          {
            // Qualified trait call: `Trait.method(receiver, args...)`.
            resolved->kind = ExpressionKind::Call;
            resolved->text = member->member;
            resolved->methodCall = true;
            auto callee = std::make_unique<Expression>(Expression{.kind = ExpressionKind::Member,
                                                                 .span = member->span,
                                                                 .text = member->member});
            callee->operands.push_back(
                std::make_unique<Expression>(Expression{.kind = ExpressionKind::ResolvedName,
                                                        .span = owner->span,
                                                        .text = owner->name}));
            resolved->operands.push_back(std::move(callee));
            for (const auto &argument : call->arguments) resolved->operands.push_back(resolveExpression(*argument));
            return resolved;
          }
        }
        // Receiver method call: `receiver.method(args...)`.
        resolved->kind = ExpressionKind::Call;
        resolved->text = member->member;
        resolved->methodCall = true;
        auto callee = std::make_unique<Expression>(Expression{.kind = ExpressionKind::Member,
                                                             .span = member->span,
                                                             .text = member->member});
        callee->operands.push_back(resolveExpression(*member->receiver));
        resolved->operands.push_back(std::move(callee));
        for (const auto &argument : call->arguments) resolved->operands.push_back(resolveExpression(*argument));
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
        for (const auto &argument : application->arguments)
        {
          TypeArgument lowered{.kind = argument.kind, .span = argument.span};
          if (argument.type != nullptr) lowered.type = std::make_unique<Type>(lowerType(*argument.type));
          if (argument.constExpr != nullptr) lowered.constExpr = cloneConstExpr(*argument.constExpr);
          resolved->genericArguments.push_back(std::move(lowered));
        }
        resolved->kind = ExpressionKind::Call;
        resolved->operands.push_back(std::move(target));
        for (const auto &argument : call->arguments) resolved->operands.push_back(resolveExpression(*argument));
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
      for (const auto &argument : application->arguments)
      {
        TypeArgument lowered{.kind = argument.kind, .span = argument.span};
        if (argument.type != nullptr) lowered.type = std::make_unique<Type>(lowerType(*argument.type));
        if (argument.constExpr != nullptr) lowered.constExpr = cloneConstExpr(*argument.constExpr);
        resolved->genericArguments.push_back(std::move(lowered));
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
    if (const auto parameter = constParameters_.find(expression.name); parameter != constParameters_.end())
    {
      return ResolvedName{.kind = ResolvedNameKind::ConstParameter, .id = parameter->second};
    }
    if (const auto function = functions_.find(expression.name); function != functions_.end())
    {
      return ResolvedName{.kind = ResolvedNameKind::Function, .id = function->second.front().value};
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

namespace NG::vnext::hir
{
namespace
{
  struct CloneContext
  {
    std::unordered_map<uint32_t, uint32_t> locals;
    std::unordered_map<uint32_t, uint32_t> loops;
    uint32_t &nextLocal;
  };

  [[nodiscard]] auto remapLocal(CloneContext &context, hir::LocalId local) -> hir::LocalId
  {
    const auto found = context.locals.find(local.value);
    if (found != context.locals.end()) return hir::LocalId{found->second};
    const uint32_t fresh = context.nextLocal++;
    context.locals.emplace(local.value, fresh);
    return hir::LocalId{fresh};
  }

  [[nodiscard]] auto remapLoop(CloneContext &context, hir::LoopId loop) -> hir::LoopId
  {
    const auto found = context.loops.find(loop.value);
    if (found != context.loops.end()) return hir::LoopId{found->second};
    const uint32_t fresh = context.nextLocal++;
    context.loops.emplace(loop.value, fresh);
    return hir::LoopId{fresh};
  }

  [[nodiscard]] auto cloneType(const hir::Type &type) -> hir::Type
  {
    hir::Type cloned{.kind = type.kind, .span = type.span, .name = type.name, .isMutable = type.isMutable};
    if (type.target != nullptr) cloned.target = std::make_unique<hir::Type>(cloneType(*type.target));
    for (const auto &argument : type.arguments)
    {
      hir::TypeArgument lowered{.kind = argument.kind, .span = argument.span};
      if (argument.type != nullptr) lowered.type = std::make_unique<hir::Type>(cloneType(*argument.type));
      if (argument.constExpr != nullptr) lowered.constExpr = cloneConstExpr(*argument.constExpr);
      cloned.arguments.push_back(std::move(lowered));
    }
    return cloned;
  }

  [[nodiscard]] auto cloneExpression(CloneContext &context, const hir::Expression &source) -> hir::ExpressionPtr
  {
    auto cloned = std::make_unique<hir::Expression>();
    cloned->kind = source.kind;
    cloned->span = source.span;
    cloned->text = source.text;
    cloned->resolvedName = source.resolvedName;
    cloned->functionCandidates = source.functionCandidates;
    cloned->structId = source.structId;
    cloned->enumId = source.enumId;
    cloned->variant = source.variant;
    cloned->memberNames = source.memberNames;
    for (const auto &argument : source.genericArguments)
    {
      hir::TypeArgument lowered{.kind = argument.kind, .span = argument.span};
      if (argument.type != nullptr) lowered.type = std::make_unique<hir::Type>(cloneType(*argument.type));
      if (argument.constExpr != nullptr) lowered.constExpr = cloneConstExpr(*argument.constExpr);
      cloned->genericArguments.push_back(std::move(lowered));
    }
    if (source.testedType != nullptr) cloned->testedType = std::make_unique<hir::Type>(cloneType(*source.testedType));
    cloned->traitNames = source.traitNames;
    cloned->methodCall = source.methodCall;
    if (cloned->resolvedName.has_value() && cloned->resolvedName->kind == hir::ResolvedNameKind::Local)
      cloned->resolvedName->id = remapLocal(context, LocalId{cloned->resolvedName->id}).value;
    for (const auto &operand : source.operands) cloned->operands.push_back(cloneExpression(context, *operand));
    return cloned;
  }

  [[nodiscard]] auto cloneBlock(CloneContext &context, const hir::Block &source) -> hir::Block
  {
    hir::Block cloned{.span = source.span};
    for (const auto &statement : source.statements)
    {
      hir::Statement copy{.kind = statement.kind, .span = statement.span};
      if (statement.local.has_value()) copy.local = remapLocal(context, *statement.local);
      if (statement.bindingType != nullptr) copy.bindingType = std::make_shared<hir::Type>(cloneType(*statement.bindingType));
      for (const auto local : statement.destructuredLocals) copy.destructuredLocals.push_back(remapLocal(context, local));
      copy.destructuredIndices = statement.destructuredIndices;
      copy.mutableBinding = statement.mutableBinding;
      if (statement.loop.has_value()) copy.loop = remapLoop(context, *statement.loop);
      if (statement.nextTarget.has_value())
        copy.nextTarget = statement.nextTarget->kind == hir::NextTargetKind::Loop
                              ? NextTarget{.kind = NextTargetKind::Loop, .id = remapLoop(context, LoopId{statement.nextTarget->id}).value}
                              : *statement.nextTarget;
      if (statement.expression != nullptr) copy.expression = cloneExpression(context, *statement.expression);
      if (statement.assignmentTarget != nullptr) copy.assignmentTarget = cloneExpression(context, *statement.assignmentTarget);
      for (const auto &argument : statement.arguments) copy.arguments.push_back(cloneExpression(context, *argument));
      for (const auto local : statement.loopBindings) copy.loopBindings.push_back(remapLocal(context, local));
      for (const auto &switchCase : statement.switchCases)
      {
        hir::SwitchCase clonedCase{.variantName = switchCase.variantName, .span = switchCase.span};
        if (switchCase.binding.has_value()) clonedCase.binding = remapLocal(context, *switchCase.binding);
        clonedCase.body = std::make_unique<hir::Block>(cloneBlock(context, *switchCase.body));
        copy.switchCases.push_back(std::move(clonedCase));
      }
      if (statement.consequence != nullptr) copy.consequence = std::make_unique<hir::Block>(cloneBlock(context, *statement.consequence));
      if (statement.alternative != nullptr) copy.alternative = std::make_unique<hir::Block>(cloneBlock(context, *statement.alternative));
      if (statement.body != nullptr) copy.body = std::make_unique<hir::Block>(cloneBlock(context, *statement.body));
      cloned.statements.push_back(std::move(copy));
    }
    if (source.tailExpression != nullptr) cloned.tailExpression = cloneExpression(context, *source.tailExpression);
    return cloned;
  }
} // namespace

  auto cloneFunction(const Function &source, uint32_t &nextLocal) -> Function
  {
    CloneContext context{.nextLocal = nextLocal};
    Function cloned{.id = source.id,
                    .name = source.name,
                    .span = source.span,
                    .genericParameters = source.genericParameters,
                    .packParameters = source.packParameters,
                    .constructorParameters = source.constructorParameters,
                    .genericParameterOrder = source.genericParameterOrder,
                    .constFunction = source.constFunction,
                    .exported = source.exported,
                    .nativeFunction = source.nativeFunction,
                    .traitBounds = source.traitBounds};
    for (const auto &parameter : source.constParameters)
      cloned.constParameters.push_back(ConstParameter{.name = parameter.name,
                                                      .typeName = parameter.typeName,
                                                      .type = cloneType(parameter.type),
                                                      .span = parameter.span});
    if (source.returnType != nullptr) cloned.returnType = std::make_unique<Type>(cloneType(*source.returnType));
    for (const auto &parameter : source.parameters)
    {
      Parameter copy{.name = parameter.name, .typeName = parameter.typeName, .span = parameter.span};
      copy.type = cloneType(parameter.type);
      copy.local = remapLocal(context, parameter.local);
      cloned.parameters.push_back(std::move(copy));
    }
    if (source.whereClause != nullptr) cloned.whereClause = cloneExpression(context, *source.whereClause);
    cloned.body = cloneBlock(context, source.body);
    return cloned;
  }
} // namespace NG::vnext::hir
