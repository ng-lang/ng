// AI-generated code; reviewed for this repository's vNext rewrite.
// HIR name resolution: module items and declarations (functions, structs,
// enums, consts, traits, impls), name lookup with did-you-mean suggestions,
// and local scopes. Statements/expressions resolve in hir_body.cpp.
#include "hir_internal.hpp"
#include "syntax/const_expr.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <unordered_set>
#include <utility>
#include <vector>

namespace NG::hir
{
  using detail::lowerType;
  using detail::renderTypeName;

  namespace
  {

    /// Levenshtein distance, cut off at `limit` to keep typo scanning cheap.
    [[nodiscard]] auto editDistance(std::string_view left, std::string_view right, size_t limit) -> size_t
    {
      if (left.size() > right.size())
        return editDistance(right, left, limit);
      if (right.size() - left.size() > limit)
        return limit + 1;
      std::vector<size_t> previous(left.size() + 1), current(left.size() + 1);
      for (size_t column = 0; column <= left.size(); ++column)
        previous[column] = column;
      for (size_t row = 1; row <= right.size(); ++row)
      {
        current[0] = row;
        size_t rowMinimum = row;
        for (size_t column = 1; column <= left.size(); ++column)
        {
          const size_t deletion = previous[column] + 1;
          const size_t insertion = current[column - 1] + 1;
          const size_t substitution = previous[column - 1] + (left[column - 1] == right[row - 1] ? 0 : 1);
          current[column] = std::min({deletion, insertion, substitution});
          rowMinimum = std::min(rowMinimum, current[column]);
        }
        if (rowMinimum > limit)
          return limit + 1;
        previous.swap(current);
      }
      return previous[left.size()];
    }

    /// Best candidate within `limit` edits of `name`, never empty.
    [[nodiscard]] auto closestName(std::string_view name, const std::unordered_set<std::string> &candidates,
                                   size_t limit) -> std::optional<std::string>
    {
      std::optional<std::string> best;
      size_t bestDistance = limit + 1;
      for (const auto &candidate : candidates)
      {
        const size_t distance = editDistance(name, candidate, limit);
        if (distance < bestDistance)
        {
          bestDistance = distance;
          best = candidate;
        }
      }
      return best;
    }
  } // namespace

  auto Resolver::resolve(const syntax::SourceUnit &unit) -> Module
  {
    functions_.clear();
    functionIds_.clear();
    structs_.clear();
    enums_.clear();
    traits_.clear();
    opaqueTypes_.clear();
    enumVariants_.clear();
    visibleNames_ = unit.visibleNames;
    currentOrigin_ = 0;
    nextLocal_ = 0;
    nextConstId_ = 0;
    declareModuleItems(unit);

    Module module;
    // Functions resolve first so their DefIds equal their positions in
    // module.functions; trait default and impl methods follow with
    // consecutive ids.
    for (const auto &item : unit.items)
      if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()))
        module.functions.push_back(
            resolveFunction(const_cast<syntax::FunctionDeclaration &>(*function), functionIds_.at(function)));
    for (const auto &item : unit.items)
    {
      if (const auto *structure = dynamic_cast<const syntax::StructDeclaration *>(item.get()))
        module.structs.push_back(resolveStruct(*structure, structs_.at(structure->name)));
      else if (const auto *enumeration = dynamic_cast<const syntax::EnumDeclaration *>(item.get()))
        module.enums.push_back(resolveEnum(*enumeration, enums_.at(enumeration->name)));
      else if (const auto *declaration = dynamic_cast<const syntax::ConstDeclaration *>(item.get()))
        module.consts.push_back(resolveConstDeclaration(*declaration, DefId{nextConstId_++}));
      else if (const auto *opaque = dynamic_cast<const syntax::OpaqueTypeDeclaration *>(item.get()))
        module.opaqueTypes.push_back(OpaqueType{.name = opaque->name,
                                                .abstract = opaque->abstract,
                                                .genericParameters = opaque->genericParameters,
                                                .packParameter = opaque->packParameter,
                                                .span = opaque->span});
      else if (const auto *trait = dynamic_cast<const syntax::TraitDeclaration *>(item.get()))
      {
        module.traits.push_back(resolveTrait(*trait));
        auto &resolved = module.traits.back();
        for (auto &method : resolved.methods)
        {
          // Declaration-only methods keep a zero placeholder so methodIds
          // stays parallel to `methods` (indexed by declaration position).
          resolved.methodIds.push_back(
              method.body.has_value()
                  ? appendMethodFunction(module, method, std::format("default${}${}", trait->name, method.name))
                  : DefId{});
        }
      }
      else if (const auto *impl = dynamic_cast<const syntax::ImplDeclaration *>(item.get()))
      {
        module.impls.push_back(resolveImpl(*impl));
        auto &resolved = module.impls.back();
        for (auto &method : resolved.methods)
          resolved.methodIds.push_back(appendMethodFunction(
              module, method,
              std::format("impl${}${}${}", impl->traitName, method.name, renderTypeName(*impl->targetType))));
      }
    }
    return module;
  }

  /// First pass: registers every top-level name (functions may overload;
  /// other declarations must be unique).
  void Resolver::declareModuleItems(const syntax::SourceUnit &unit)
  {
    uint32_t functionCount{};
    uint32_t structCount{};
    uint32_t enumCount{};
    const auto requireUnique = [](bool inserted, const std::string &name, syntax::SourceSpan span)
    {
      if (!inserted)
        throw ResolutionError(std::format("duplicate module declaration `{}`", name), span);
    };
    for (const auto &item : unit.items)
    {
      if (const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get()))
      {
        const DefId id{functionCount++};
        functions_[function->name].push_back(id);
        functionIds_.emplace(function, id);
      }
      else if (const auto *structure = dynamic_cast<const syntax::StructDeclaration *>(item.get()))
        requireUnique(structs_.emplace(structure->name, StructId{structCount++}).second, structure->name,
                      structure->span);
      else if (const auto *enumeration = dynamic_cast<const syntax::EnumDeclaration *>(item.get()))
      {
        requireUnique(enums_.emplace(enumeration->name, EnumId{enumCount++}).second, enumeration->name,
                      enumeration->span);
        auto &variants = enumVariants_[enumeration->name];
        for (const auto &variant : enumeration->variants)
          variants.push_back(variant.name);
      }
      else if (const auto *opaque = dynamic_cast<const syntax::OpaqueTypeDeclaration *>(item.get()))
        requireUnique(opaqueTypes_.emplace(opaque->name, opaque->span).second, opaque->name, opaque->span);
      else if (const auto *trait = dynamic_cast<const syntax::TraitDeclaration *>(item.get()))
        requireUnique(traits_.emplace(trait->name, trait->span).second, trait->name, trait->span);
      // Const declarations and impls are anonymous at module scope; import
      // directives are consumed by the module loader.
      else if (dynamic_cast<const syntax::ConstDeclaration *>(item.get()) == nullptr &&
               dynamic_cast<const syntax::ImplDeclaration *>(item.get()) == nullptr &&
               dynamic_cast<const syntax::ImportDeclaration *>(item.get()) == nullptr)
        throw ResolutionError("unsupported module item during name resolution", item->span);
    }
  }

  /// Lowers a resolved trait/impl method into a module function with the
  /// next DefId.
  auto Resolver::appendMethodFunction(Module &module, TraitMethod &method, std::string name) -> DefId
  {
    const DefId id{static_cast<uint32_t>(module.functions.size())};
    Function lowered{.id = id, .name = std::move(name), .span = method.span};
    lowered.returnType = std::move(method.returnType);
    lowered.parameters = std::move(method.parameters);
    if (method.body.has_value())
      lowered.body = std::move(*method.body);
    module.functions.push_back(std::move(lowered));
    return id;
  }

  auto Resolver::resolveTraitMethod(syntax::TraitMethodDeclaration &method) -> TraitMethod
  {
    scopes_.emplace_back();
    TraitMethod resolved{.name = method.name, .span = method.span};
    paramOrdinals_.clear();
    for (size_t index = 0; index < method.parameters.size(); ++index)
    {
      const auto &parameter = method.parameters[index];
      const LocalId local = declareLocal(parameter.name, parameter.span);
      paramOrdinals_.emplace(parameter.name, static_cast<uint32_t>(index));
      resolved.parameters.push_back(Parameter{.name = parameter.name,
                                              .typeName = renderTypeName(*parameter.type),
                                              .type = lowerType(*parameter.type, &paramOrdinals_),
                                              .local = local,
                                              .span = parameter.span,
                                              .ownership = parameter.ownership});
    }
    if (method.returnType != nullptr)
      resolved.returnType = std::make_unique<Type>(lowerType(*method.returnType, &paramOrdinals_));
    paramOrdinals_.clear();
    if (method.body.has_value())
    {
      currentReturnTypeName_ = method.returnType != nullptr ? renderTypeName(*method.returnType) : std::string{};
      ++functionBodyDepth_;
      closureBodyDepth_ = 0;
      resolved.body = resolveBlock(*method.body, false, true);
      --functionBodyDepth_;
      currentReturnTypeName_.clear();
    }
    scopes_.pop_back();
    return resolved;
  }

  auto Resolver::resolveTrait(const syntax::TraitDeclaration &declaration) -> Trait
  {
    Trait resolved{.name = declaration.name,
                   .supertraits = declaration.supertraits,
                   .autoTrait = declaration.autoTrait,
                   .span = declaration.span};
    const uint32_t previousOrigin = currentOrigin_;
    currentOrigin_ = declaration.originModule;
    for (const auto &method : declaration.methods)
      resolved.methods.push_back(resolveTraitMethod(const_cast<syntax::TraitMethodDeclaration &>(method)));
    currentOrigin_ = previousOrigin;
    return resolved;
  }

  auto Resolver::resolveImpl(const syntax::ImplDeclaration &declaration) -> Impl
  {
    Impl resolved{.traitName = declaration.traitName, .span = declaration.span};
    for (const auto &parameter : declaration.genericParameters)
      resolved.genericParameters.push_back(parameter.name);
    if (declaration.targetType != nullptr)
      resolved.targetType = std::make_unique<Type>(lowerType(*declaration.targetType));
    const uint32_t previousOrigin = currentOrigin_;
    currentOrigin_ = declaration.originModule;
    for (const auto &method : declaration.methods)
      resolved.methods.push_back(resolveTraitMethod(const_cast<syntax::TraitMethodDeclaration &>(method)));
    currentOrigin_ = previousOrigin;
    return resolved;
  }

  auto Resolver::resolveConstDeclaration(const syntax::ConstDeclaration &declaration, DefId id) -> ConstDeclaration
  {
    ConstDeclaration resolved{
      .id = id,
      .name = declaration.name,
      .span = declaration.span,
      .bodyKind = declaration.bodyKind == syntax::ConstBodyKind::Native   ? ConstDeclarationBodyKind::Native
                  : declaration.bodyKind == syntax::ConstBodyKind::Delete ? ConstDeclarationBodyKind::Delete
                                                                          : ConstDeclarationBodyKind::Expression};
    for (const auto &parameter : declaration.parameters)
      resolved.typeParameters.push_back(parameter.name);
    // Bare identifiers appearing as top-level pattern arguments introduce
    // type parameters implicitly, but only for prefix-less (primary)
    // declarations: `const<T> name<...>` declares its parameters explicitly.
    // Names that already denote a concrete type (builtins, structs, enums)
    // are never introduced as parameters.
    if (declaration.parameters.empty())
    {
      constexpr std::array<std::string_view, 7> BuiltinNames{"i64", "u8",   "bool", "unit",
                                                             "string", "isize", "usize"};
      for (const auto &argument : declaration.patternArguments)
      {
        if (const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(argument.get()); named != nullptr)
        {
          const bool concrete =
              std::find(BuiltinNames.begin(), BuiltinNames.end(), named->name) != BuiltinNames.end() ||
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
    if (declaration.body != nullptr)
      resolved.body = cloneConstExpr(*declaration.body);
    return resolved;
  }

  auto Resolver::resolveEnum(const syntax::EnumDeclaration &enumeration, EnumId id) -> Enum
  {
    Enum resolved{
      .id = id, .name = enumeration.name, .span = enumeration.span, .genericParameters = enumeration.genericParameters};
    std::unordered_set<std::string> names;
    for (const auto &variant : enumeration.variants)
    {
      if (!names.insert(variant.name).second)
        throw ResolutionError(std::format("duplicate variant `{}` in enum `{}`", variant.name, enumeration.name),
                              variant.span);
      EnumVariant lowered{.name = variant.name, .span = variant.span};
      if (variant.payloadType != nullptr)
        lowered.payloadType = std::make_unique<Type>(lowerType(*variant.payloadType));
      resolved.variants.push_back(std::move(lowered));
    }
    return resolved;
  }

  auto Resolver::resolveStruct(const syntax::StructDeclaration &structure, StructId id) -> Struct
  {
    Struct resolved{.id = id,
                    .name = structure.name,
                    .span = structure.span,
                    .genericParameters = structure.genericParameters,
                    .derivedTraits = structure.derivedTraits,
                    .reprC = structure.reprC};
    std::unordered_set<std::string> names;
    for (const auto &field : structure.fields)
    {
      if (!names.insert(field.name).second)
        throw ResolutionError(std::format("duplicate field `{}` in struct `{}`", field.name, structure.name),
                              field.span);
      resolved.fields.push_back(StructField{.name = field.name, .type = lowerType(*field.type), .span = field.span});
    }
    return resolved;
  }

  auto Resolver::resolveFunction(syntax::FunctionDeclaration &function, DefId id) -> Function
  {
    scopes_.clear();
    scopes_.emplace_back();
    loops_.clear();
    localMutability_.clear();
    currentFunction_ = id;
    nextLoop_ = 0;

    constParameters_.clear();
    Function resolved{.id = id,
                      .name = function.name,
                      .span = function.span,
                      .originModule = function.originModule,
                      .constFunction = function.constFunction,
                      .exported = function.exported,
                      .nativeFunction = function.nativeFunction,
                      .externC = function.externC, .variadic = function.variadic};
    const uint32_t previousOrigin = currentOrigin_;
    currentOrigin_ = function.originModule;
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
        if (parameter.kind == syntax::GenericParameterKind::Pack)
          resolved.packParameters.push_back(parameter.name);
        else if (parameter.kind == syntax::GenericParameterKind::TypeConstructor)
          resolved.constructorParameters.push_back(parameter.name);
        else if (parameter.kind == syntax::GenericParameterKind::VariadicTypeConstructor)
          resolved.variadicConstructorParameters.push_back(parameter.name);
        else
          resolved.genericParameters.push_back(parameter.name);
        if (!parameter.traitBounds.empty())
          resolved.traitBounds.emplace_back(parameter.name, parameter.traitBounds);
      }
    }
    resolved.parameters.reserve(function.parameters.size());
    paramOrdinals_.clear();
    for (size_t index = 0; index < function.parameters.size(); ++index)
    {
      const auto &parameter = function.parameters[index];
      const LocalId local = declareLocal(parameter.name, parameter.span);
      // A1.2 place-through-ref: a `T ref mut` parameter is inherently writable
      // through its member/index places (`s.field := ...`, `arr[i] := ...` even
      // without an explicit `*`); its ref mutability is enforced by the checker.
      const bool mutableView = parameter.type != nullptr &&
                               parameter.type->kind == syntax::TypeSyntaxKind::ScopedReference &&
                               dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(parameter.type.get())->isMutable;
      localMutability_.emplace(local.value, mutableView);
      paramOrdinals_.emplace(parameter.name, static_cast<uint32_t>(index));
      resolved.parameters.push_back(Parameter{.name = parameter.name,
                                              .typeName = renderTypeName(*parameter.type),
                                              .type = lowerType(*parameter.type, &paramOrdinals_),
                                              .local = local,
                                              .span = parameter.span,
                                              .ownership = parameter.ownership});
    }
    if (function.returnType != nullptr)
    {
      resolved.returnTypeName = renderTypeName(*function.returnType);
      resolved.returnType = std::make_unique<Type>(lowerType(*function.returnType, &paramOrdinals_));
    }
    paramOrdinals_.clear();
    if (function.whereClause != nullptr)
      resolved.whereClause = resolveExpression(*function.whereClause);
    currentReturnTypeName_ = resolved.returnTypeName.value_or(std::string{});
    ++functionBodyDepth_;
    closureBodyDepth_ = 0;
    resolved.body = resolveBlock(function.body, false, true);
    --functionBodyDepth_;
    currentReturnTypeName_.clear();
    currentFunction_.reset();
    return resolved;
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
    const auto visible = visibleNames_.find(currentOrigin_);
    if (visible != visibleNames_.end() && !visible->second.contains(expression.name))
      throw ResolutionError(
          "E-name-not-visible",
          std::format("name `{}` is not visible in this module{}", expression.name, suggestionFor(expression.name)),
          expression.span);
    if (const auto function = functions_.find(expression.name); function != functions_.end())
    {
      return ResolvedName{.kind = ResolvedNameKind::Function, .id = function->second.front().value};
    }
    throw ResolutionError("E-unresolved-name",
                          std::format("unresolved name `{}`{}", expression.name, suggestionFor(expression.name)),
                          expression.span);
  }

  auto Resolver::suggestionFor(const std::string &name) const -> std::string
  {
    std::unordered_set<std::string> candidates;
    for (const auto &[candidate, unused] : functions_)
      candidates.insert(candidate);
    for (const auto &[candidate, unused] : structs_)
      candidates.insert(candidate);
    for (const auto &[candidate, unused] : enums_)
      candidates.insert(candidate);
    for (const auto &[candidate, unused] : traits_)
      candidates.insert(candidate);
    for (const auto &[candidate, unused] : opaqueTypes_)
      candidates.insert(candidate);
    const auto visible = visibleNames_.find(currentOrigin_);
    if (visible != visibleNames_.end())
      candidates.insert(visible->second.begin(), visible->second.end());
    const auto closest = closestName(name, candidates, 2);
    return closest ? std::format("; did you mean `{}`?", *closest) : std::string{};
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
} // namespace NG::hir
