// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck_register.cpp — registration: check(), traits, impls, ABI.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

auto Checker::check(const hir::Module &module, const const_eval::ConstNativeHost &host) -> TypeCheckResult
{
  module_ = &module;
  constNativeHost_ = host;
  reprCStructIds_.clear();
  for (const auto &structure : module.structs)
    if (structure.reprC)
      reprCStructIds_.insert(structure.id.value);
  {
    std::unordered_map<std::string, syntax::SourceSpan> typeNames;
    const auto declareName = [&](const std::string &name, syntax::SourceSpan span)
    {
      if (!typeNames.emplace(name, span).second)
        throw TypeError(std::format("duplicate type declaration `{}`", name), span);
    };
    for (const auto &structure : module.structs)
      declareName(structure.name, structure.span);
    for (const auto &enumeration : module.enums)
      declareName(enumeration.name, enumeration.span);
    for (const auto &opaque : module.opaqueTypes)
      declareName(opaque.name, opaque.span);
  }
  for (const auto &opaque : module.opaqueTypes)
    static_cast<void>(interner_.declareOpaqueType(opaque));
  for (const auto &structure : module.structs)
    static_cast<void>(interner_.declareStruct(structure.id, structure.name));
  for (const auto &enumeration : module.enums)
  {
    static_cast<void>(interner_.declareEnum(enumeration.id, enumeration.name, enumeration.genericParameters));
    interner_.registerEnumTemplate(enumeration);
  }
  for (const auto &structure : module.structs)
    interner_.registerStructTemplate(structure);
  for (const auto &structure : module.structs)
  {
    if (structure.reprC)
      validateReprCStruct(structure);
    if (!structure.genericParameters.empty())
      continue;
    std::vector<std::string> fields;
    std::vector<TypeId> types;
    for (const auto &field : structure.fields)
    {
      fields.push_back(field.name);
      const TypeId fieldType = interner_.resolve(field.type);
      if (interner_.descriptor(fieldType).kind == TypeKind::Reference)
        throw TypeError("references cannot be stored in struct fields", field.span);
      if (interner_.descriptor(fieldType).kind == TypeKind::Span)
        throw TypeError("spans cannot be stored in struct fields (INC-21 views are scoped)", field.span);
      if (interner_.descriptor(fieldType).kind == TypeKind::Trait)
        throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`", interner_.display(fieldType),
                                    interner_.display(fieldType)),
                        field.span);
      types.push_back(fieldType);
    }
    interner_.defineStruct(structure.id, std::move(fields), std::move(types));
  }
  for (const auto &enumeration : module.enums)
  {
    std::vector<std::string> variants;
    std::vector<TypeId> payloads;
    std::vector<bool> hasPayload;
    for (const auto &variant : enumeration.variants)
    {
      variants.push_back(variant.name);
      hasPayload.push_back(variant.payloadType != nullptr);
      payloads.push_back(variant.payloadType != nullptr && enumeration.genericParameters.empty()
                              ? interner_.resolve(*variant.payloadType)
                              : builtin::Unit);
    }
    interner_.defineEnum(enumeration.id, std::move(variants), std::move(payloads), std::move(hasPayload));
  }
  registerTraits();
  registerImpls();
  for (const auto &structure : module.structs)
  {
    if (structure.derivedTraits.empty())
      continue;
    const TypeId target = interner_.typeForStruct(structure.id);
    std::unordered_set<std::string> seen;
    for (const auto &traitName : structure.derivedTraits)
    {
      if (!seen.insert(traitName).second)
        throw TypeError(std::format("duplicate derive for trait `{}` on type `{}`", traitName, structure.name),
                        structure.span);
      if (!traits_.contains(traitName))
        throw TypeError(std::format("derive: unknown trait `{}`; import a module that declares it", traitName),
                        structure.span);
      for (const auto &impl : impls_)
        if (impl.target == target && impl.traitName == traitName)
          throw TypeError(std::format("derive conflicts with explicit impl for trait `{}` on type `{}`",
                                      traitName, structure.name),
                          structure.span);
      impls_.push_back(ImplInfo{.traitName = traitName, .target = target, .methods = {}});
      if (traitName == "Clone")
        derivedCloneTypes_.insert(target.value);
      if (traitName == "Copy")
        derivedCopyTypes_.insert(target.value);
      if (traitName == "Eq")
      {
        // Validate: every field must be Eq-comparable (primitives, Eq structs, or
        // tuples/Eq structs recursively).
        const auto &descriptor = interner_.descriptor(target);
        for (size_t i = 0; i < descriptor.elements.size(); ++i)
        {
          const auto &fieldType = descriptor.elements[i];
          const auto &fd = interner_.descriptor(fieldType);
          if (fd.kind == TypeKind::Struct || fd.kind == TypeKind::Enum)
          {
            if (!derivedEqTypes_.contains(fieldType.value))
              throw TypeError(
                  std::format("derive(Eq) on `{}`: field `{}` type `{}` does not derive Eq", structure.name,
                              descriptor.fieldNames[i], interner_.display(fieldType)),
                  structure.span);
          }
          else if (!isIntegerBuiltin(fieldType) && fieldType != builtin::Bool && fieldType != builtin::String)
          {
            throw TypeError(
                std::format("derive(Eq) on `{}`: field `{}` type `{}` is not Eq-comparable", structure.name,
                            descriptor.fieldNames[i], interner_.display(fieldType)),
                structure.span);
          }
        }
        derivedEqTypes_.insert(target.value);
      }
      if (traitName == "Ord")
      {
        // DEC-E: every field must be orderable - integers, bool, or string
        // (bytewise), or a nested Ord struct/enum. Floats are rejected because
        // NaN breaks totality.
        const auto &descriptor = interner_.descriptor(target);
        for (size_t i = 0; i < descriptor.elements.size(); ++i)
        {
          const auto &fieldType = descriptor.elements[i];
          const auto &fd = interner_.descriptor(fieldType);
          if (fieldType == builtin::F32 || fieldType == builtin::F64)
            throw TypeError(
                std::format("derive(Ord) on `{}`: field `{}` has float type `{}`; floats have no total order "
                            "(NaN) and are excluded from v1",
                            structure.name, descriptor.fieldNames[i], interner_.display(fieldType)),
                structure.span);
          if (fd.kind == TypeKind::Struct || fd.kind == TypeKind::Enum)
          {
            if (!derivedOrdTypes_.contains(fieldType.value))
              throw TypeError(
                  std::format("derive(Ord) on `{}`: field `{}` type `{}` does not derive Ord", structure.name,
                              descriptor.fieldNames[i], interner_.display(fieldType)),
                  structure.span);
          }
          else if (!isIntegerBuiltin(fieldType) && fieldType != builtin::Bool && fieldType != builtin::String)
          {
            throw TypeError(
                std::format("derive(Ord) on `{}`: field `{}` type `{}` is not orderable", structure.name,
                            descriptor.fieldNames[i], interner_.display(fieldType)),
                structure.span);
          }
        }
        derivedOrdTypes_.insert(target.value);
      }
      if (traitName == "Show")
      {
        // Validate: every field must be a string, a scalar with a native
        // `toString` overload (i64/i32/u64/f64/bool in the prelude), or a
        // Show-derived struct. Resolution order: nested structs must be
        // declared (and derive Show) before the struct that contains them.
        const auto &descriptor = interner_.descriptor(target);
        for (size_t i = 0; i < descriptor.elements.size(); ++i)
        {
          const auto &fieldType = descriptor.elements[i];
          const auto &fd = interner_.descriptor(fieldType);
          if (fd.kind == TypeKind::Struct || fd.kind == TypeKind::Enum)
          {
            if (fd.kind == TypeKind::Enum || !derivedShowTypes_.contains(fieldType.value))
              throw TypeError(std::format("derive(Show) on `{}`: field `{}` type `{}` does not derive Show",
                                          structure.name, descriptor.fieldNames[i], interner_.display(fieldType)),
                              structure.span);
          }
          else if (fieldType != builtin::String && !showToStringTargets_.contains(fieldType.value))
          {
            hir::DefId toStringTarget{};
            bool found = false;
            for (const auto &candidate : module_->functions)
            {
              if (!candidate.nativeFunction || candidate.name != "toString" || candidate.parameters.size() != 1)
                continue;
              if (interner_.resolve(candidate.parameters[0].type) == fieldType)
              {
                toStringTarget = candidate.id;
                found = true;
                break;
              }
            }
            if (!found)
              throw TypeError(std::format("derive(Show) on `{}`: field `{}` type `{}` is not showable "
                                          "(no toString overload)",
                                          structure.name, descriptor.fieldNames[i], interner_.display(fieldType)),
                              structure.span);
            showToStringTargets_.emplace(fieldType.value, toStringTarget);
          }
        }
        derivedShowTypes_.insert(target.value);
      }
    }
  }
  for (const auto &function : module.functions)
  {
    if (function.name.starts_with("impl$") || function.name.starts_with("default$"))
      continue;
    FunctionTypeIds signature;
    std::unordered_map<std::string, TypeId> genericBindings;
    TypeInterner::ConstParamBindings constBindings;
    for (size_t index = 0; index < function.genericParameters.size(); ++index)
    {
      const auto parameter =
          interner_.internTypeParameter(function.genericParameters[index], static_cast<uint32_t>(index));
      signature.genericParameters.push_back(parameter);
      signature.genericParameterNames.push_back(function.genericParameters[index]);
      genericBindings.emplace(function.genericParameters[index], parameter);
    }
    for (size_t index = 0; index < function.packParameters.size(); ++index)
    {
      const auto parameter = interner_.internTypeParameter(
          function.packParameters[index], static_cast<uint32_t>(function.genericParameters.size() + index));
      signature.packParameters.push_back(parameter);
      signature.packParameterNames.push_back(function.packParameters[index]);
      genericBindings.emplace(function.packParameters[index], parameter);
    }
    for (size_t index = 0; index < function.constructorParameters.size(); ++index)
    {
      const auto parameter =
          interner_.internTypeConstructor(function.constructorParameters[index], static_cast<uint32_t>(index));
      signature.constructorParameters.push_back(parameter);
      signature.constructorParameterNames.push_back(function.constructorParameters[index]);
      genericBindings.emplace(function.constructorParameters[index], parameter);
    }
    for (size_t index = 0; index < function.variadicConstructorParameters.size(); ++index)
    {
      const auto parameter = interner_.internTypeConstructor(
          function.variadicConstructorParameters[index],
          static_cast<uint32_t>(function.constructorParameters.size() + index), true);
      signature.constructorParameters.push_back(parameter);
      signature.constructorParameterNames.push_back(function.variadicConstructorParameters[index]);
      genericBindings.emplace(function.variadicConstructorParameters[index], parameter);
    }
    for (const auto kind : function.genericParameterOrder)
      if (kind != syntax::GenericParameterKind::Pack)
        signature.explicitParameterOrder.push_back(kind);
    for (const auto &[parameterName, bounds] : function.traitBounds)
    {
      for (const auto &traitName : bounds)
        if (!traits_.contains(traitName))
          throw TypeError(std::format("unknown trait bound `{}` on `{}`", traitName, parameterName), function.span);
    }
    for (size_t index = 0; index < function.constParameters.size(); ++index)
    {
      const auto &parameter = function.constParameters[index];
      const TypeId type = interner_.resolve(parameter.type);
      if (type != builtin::I64)
        throw TypeError(std::format("const generic parameter `{}` must be i64, got {}", parameter.name,
                                    interner_.display(type)),
                        parameter.span);
      signature.constParameters.push_back(type);
      signature.constParameterNames.push_back(parameter.name);
      constBindings.emplace(parameter.name, static_cast<uint32_t>(index));
    }
    FunctionType displaySignature;
    for (const auto &parameter : function.parameters)
    {
      const TypeId type = interner_.resolveInScope(parameter.type, genericBindings, constBindings);
      if (interner_.descriptor(type).kind == TypeKind::Trait)
        throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`", interner_.display(type),
                                    interner_.display(type)),
                        parameter.span);
      signature.parameters.push_back(type);
      displaySignature.parameters.push_back(interner_.display(type));
    }
    signature.returnType = function.returnType != nullptr
                                ? interner_.resolveInScope(*function.returnType, genericBindings, constBindings)
                                : builtin::Unit;
    if (interner_.descriptor(signature.returnType).kind == TypeKind::Trait)
      throw TypeError(std::format("trait `{}` is not a value type; use `ref<{}>`",
                                  interner_.display(signature.returnType),
                                  interner_.display(signature.returnType)),
                      function.span);
    displaySignature.returnType = interner_.display(signature.returnType);
    if (function.externC)
      validateExternSignature(function, signature);
    signatures_.emplace(function.id.value, signature);
    functionTypeIds_.emplace(function.id.value, signature);
    functionTypes_.emplace(function.id.value, std::move(displaySignature));
    // INC-14 S2c/S4: templates are model-checked only and never lowered --
    // concrete instantiations carry the real lowered code. This covers
    // higher-order templates (closure-typed parameters, whose parameter calls
    // are unresolved at model time) and generic templates (type parameters and
    // friends), whose model bodies cannot reference a lowered closure.
    if (isGeneric(signature))
      placeholderFunctions_.insert(function.id.value);
    for (const auto &parameter : signature.parameters)
      if (interner_.descriptor(parameter).kind == TypeKind::Closure)
      {
        placeholderFunctions_.insert(function.id.value);
        break;
      }
  }
  for (const auto &declaration : module.consts)
    checkConstDeclaration(declaration);
  for (const auto &function : module.functions)
  {
    if (function.constFunction)
      constFunctions_.insert(function.id.value);
  }
  interpreter_ = std::make_unique<const_eval::ConstInterpreter>(
      module, constFunctions_, interner_.constInterner(),
      [this](const hir::Expression &node) { return evaluateConstApplication(node); }, constNativeHost_,
      [this](hir::DefId id) -> const hir::Function *
      {
        if (id.value < module_->functions.size())
          return &module_->functions[id.value];
        for (const auto &instance : instances_)
          if (instance.id == id)
            return &instance;
        return nullptr;
      });
  for (const auto &function : module.functions)
    checkFunction(function);
  return TypeCheckResult{.expressionTypes = std::move(expressionTypes_),
                          .expressionTypeIds = std::move(expressionTypeIds_),
                          .localTypes = std::move(localDisplayTypes_),
                          .localTypeIds = std::move(localTypeIds_),
                          .functionTypes = std::move(functionTypes_),
                          .functionTypeIds = std::move(functionTypeIds_),
                          .callTargets = std::move(callTargets_),
                          .callCaptures = std::move(callCaptures_),
                          .callPackArgCounts = std::move(callPackArgCounts_),
                          .callPackTupleTypes = std::move(callPackTupleTypes_),
                          .callSpreadPositions = std::move(callSpreadPositions_),
                          .arrayListSpreadCalls = std::move(listSpreadCalls_),
                          .callFoldSpreadPositions = std::move(callFoldSpreadPositions_),
                          .callFoldAccumulatorPositions = std::move(callFoldAccumulatorPositions_),
                          .inPlaceMutationCalls = std::move(inPlaceMutationCalls_),
                          .returnDrops = std::move(returnDrops_),
                          .fallthroughDrops = std::move(fallthroughDrops_),
                          .blockDrops = std::move(blockDrops_),
                          .placeholderFunctions = std::move(placeholderFunctions_),
                          .methodReceiverMutable = std::move(methodReceiverMutable_),
                          .methodReceiverRefTypes = std::move(methodReceiverRefTypes_),
                          .constIfSelections = std::move(constIfSelections_),
                          .instances = std::vector<hir::Function>{std::make_move_iterator(instances_.begin()),
                                                                  std::make_move_iterator(instances_.end())},
                          .deferredMethodFunctions = std::move(deferredMethodFunctions_),
                          .derivedCloneCalls = std::move(derivedCloneCalls_),
                           .boxCloneCalls = std::move(boxCloneCalls_),
                          .derivedEqTypes = std::move(derivedEqTypes_),
                          .derivedOrdTypes = std::move(derivedOrdTypes_),
                          .derivedShowTypes = std::move(derivedShowTypes_),
                          .derivedShowCalls = std::move(derivedShowCalls_),
                          .showToStringTargets = std::move(showToStringTargets_),
                          .traitViewCoercions = std::move(traitViewCoercions_),
                          .unionCoercions = std::move(unionCoercions_),
                          .traitViewCalls = std::move(traitViewCalls_),
                          .traitViewTables = std::move(traitViewTables_),
                          .typeDescriptors = interner_.descriptors()};
}

void Checker::registerTraits()
{
  for (const auto &trait : module_->traits)
  {
    TraitInfo info{.name = trait.name,
                   .supertraits = trait.supertraits,
                   .autoTrait = trait.autoTrait,
                   .selfParameter = interner_.internTypeParameter("Self", 0)};
    for (const auto &method : trait.methods)
      info.methodOrder.push_back(method.name);
    static_cast<void>(interner_.declareTraitType(trait.name));
    const std::unordered_map<std::string, TypeId> selfBindings{{"Self", info.selfParameter}};
    for (size_t index = 0; index < trait.methods.size(); ++index)
    {
      const auto &method = trait.methods[index];
      const std::vector<hir::Parameter> &sourceParameters =
          method.body.has_value() ? module_->functions.at(trait.methodIds.at(index).value).parameters
                                  : method.parameters;
      std::vector<TypeId> parameters;
      for (const auto &parameter : sourceParameters)
        parameters.push_back(interner_.resolveInScope(parameter.type, selfBindings));
      if (parameters.empty() || interner_.descriptor(parameters.front()).kind != TypeKind::Reference ||
          interner_.descriptor(parameters.front()).element != info.selfParameter)
        throw TypeError(std::format("trait method `{}` must take `self: Self ref`", method.name), method.span);
      info.methodParameters.emplace(method.name, parameters);
      const hir::Type *sourceReturnType = method.returnType.get();
      if (method.body.has_value())
        sourceReturnType = module_->functions.at(trait.methodIds.at(index).value).returnType.get();
      info.methodReturns.emplace(method.name, sourceReturnType != nullptr
                                                  ? interner_.resolveInScope(*sourceReturnType, selfBindings)
                                                  : builtin::Unit);
      if (method.body.has_value())
      {
        info.methodDefaults.emplace(method.name, trait.methodIds.at(index));
        FunctionTypeIds defaultSignature;
        defaultSignature.parameters = parameters;
        defaultSignature.returnType = info.methodReturns.at(method.name);
        defaultSignature.genericParameters.push_back(info.selfParameter);
        defaultSignature.genericParameterNames.push_back("Self");
        placeholderFunctions_.insert(trait.methodIds.at(index).value);
        defaultMethodTraits_.emplace(trait.methodIds.at(index).value, trait.name);
        FunctionType displaySignature;
        for (const auto parameter : parameters)
          displaySignature.parameters.push_back(interner_.display(parameter));
        displaySignature.returnType = interner_.display(defaultSignature.returnType);
        signatures_.emplace(trait.methodIds.at(index).value, std::move(defaultSignature));
        functionTypes_.emplace(trait.methodIds.at(index).value, std::move(displaySignature));
      }
    }
    traits_.emplace(trait.name, std::move(info));
  }
}

void Checker::collectSupertraits(const std::string &traitName, std::vector<const TraitInfo *> &out,
                                  std::unordered_set<std::string> &seen) const
{
  const auto found = traits_.find(traitName);
  if (found == traits_.end() || !seen.insert(traitName).second)
    return;
  out.push_back(&found->second);
  for (const auto &supertrait : found->second.supertraits)
    collectSupertraits(supertrait, out, seen);
}

void Checker::resolveImplMethodSignature(hir::DefId id, const hir::Function &function, TypeId target)
{
  FunctionTypeIds signature;
  const std::unordered_map<std::string, TypeId> bindings{{"Self", target}};
  for (const auto &parameter : function.parameters)
    signature.parameters.push_back(interner_.resolveInScope(parameter.type, bindings));
  if (signature.parameters.empty() ||
      interner_.descriptor(signature.parameters.front()).kind != TypeKind::Reference ||
      interner_.descriptor(signature.parameters.front()).element != target)
    throw TypeError(std::format("trait method `{}` must take `self: Self ref`", function.name), function.span);
  signature.returnType =
      function.returnType != nullptr ? interner_.resolveInScope(*function.returnType, bindings) : builtin::Unit;
  FunctionType displaySignature;
  for (const auto parameter : signature.parameters)
    displaySignature.parameters.push_back(interner_.display(parameter));
  displaySignature.returnType = interner_.display(signature.returnType);
  functionTypeIds_.emplace(id.value, signature);
  signatures_.emplace(id.value, std::move(signature));
  functionTypes_.emplace(id.value, std::move(displaySignature));
}

void Checker::collectDropMovedFields(const hir::Block &block, uint32_t selfLocal,
                                      std::unordered_set<std::string> &fields)
{
  for (const auto &statement : block.statements)
    collectDropMovedFields(statement, selfLocal, fields);
  if (block.tailExpression != nullptr)
    collectDropMovedFields(*block.tailExpression, selfLocal, fields);
}

void Checker::collectDropMovedFields(const hir::Statement &statement, uint32_t selfLocal,
                                      std::unordered_set<std::string> &fields)
{
  switch (statement.kind)
  {
  case hir::StatementKind::Let:
  case hir::StatementKind::Return:
    if (statement.expression != nullptr)
      collectDropMovedFields(*statement.expression, selfLocal, fields);
    return;
  case hir::StatementKind::Assign:
    if (statement.expression != nullptr)
      collectDropMovedFields(*statement.expression, selfLocal, fields);
    if (statement.assignmentTarget != nullptr)
      collectDropMovedFields(*statement.assignmentTarget, selfLocal, fields);
    return;
  case hir::StatementKind::Expression:
    if (statement.expression != nullptr)
      collectDropMovedFields(*statement.expression, selfLocal, fields);
    return;
  case hir::StatementKind::If:
  case hir::StatementKind::ConstIf:
    if (statement.expression != nullptr)
      collectDropMovedFields(*statement.expression, selfLocal, fields);
    if (statement.consequence != nullptr)
      collectDropMovedFields(*statement.consequence, selfLocal, fields);
    if (statement.alternative != nullptr)
      collectDropMovedFields(*statement.alternative, selfLocal, fields);
    return;
  case hir::StatementKind::Loop:
    if (statement.body != nullptr)
      collectDropMovedFields(*statement.body, selfLocal, fields);
    for (const auto &argument : statement.arguments)
      collectDropMovedFields(*argument, selfLocal, fields);
    return;
  case hir::StatementKind::Next:
    for (const auto &argument : statement.arguments)
      collectDropMovedFields(*argument, selfLocal, fields);
    return;
  case hir::StatementKind::While:
    if (statement.expression != nullptr)
      collectDropMovedFields(*statement.expression, selfLocal, fields);
    if (statement.body != nullptr)
      collectDropMovedFields(*statement.body, selfLocal, fields);
    return;
  case hir::StatementKind::Continue:
  case hir::StatementKind::Break:
    return;
  case hir::StatementKind::Switch:
    if (statement.expression != nullptr)
      collectDropMovedFields(*statement.expression, selfLocal, fields);
    for (const auto &switchCase : statement.switchCases)
      if (switchCase.body != nullptr)
        collectDropMovedFields(*switchCase.body, selfLocal, fields);
    return;
  }
}

void Checker::collectDropMovedFields(const hir::Expression &expression, uint32_t selfLocal,
                                      std::unordered_set<std::string> &fields)
{
  if (expression.kind == hir::ExpressionKind::Prefix && expression.text == "move" && !expression.operands.empty())
  {
    const auto &moved = *expression.operands[0];
    if (moved.kind == hir::ExpressionKind::Member && !moved.operands.empty())
    {
      const hir::Expression *root = moved.operands[0].get();
      while (root->kind == hir::ExpressionKind::Grouped && !root->operands.empty())
        root = root->operands[0].get();
      const hir::Expression *selfNode = root;
      if (root->kind == hir::ExpressionKind::Prefix && root->text == "*" && !root->operands.empty())
        selfNode = root->operands[0].get();
      if (selfNode->kind == hir::ExpressionKind::ResolvedName && selfNode->resolvedName.has_value() &&
          selfNode->resolvedName->kind == hir::ResolvedNameKind::Local && selfNode->resolvedName->id == selfLocal)
      {
        fields.insert(moved.text);
        return;
      }
    }
  }
  for (const auto &child : expression.operands)
    collectDropMovedFields(*child, selfLocal, fields);
}

auto Checker::dropEdge(uint32_t local, TypeId type, syntax::SourceSpan span) -> std::pair<uint32_t, uint32_t>
{
  const ImplInfo *impl = nullptr;
  Substitution genericSubstitution;
  for (const auto &candidate : impls_)
  {
    if (candidate.traitName != "Drop")
      continue;
    if (!candidate.generic && candidate.target.value == type.value)
    {
      impl = &candidate;
      break;
    }
    if (candidate.generic && matchImplPattern(candidate, type, genericSubstitution))
    {
      impl = &candidate;
      break;
    }
  }
  if (impl == nullptr)
    throw TypeError(std::format("no Drop impl registered for {}", interner_.display(type)), span);
  if (const auto moved = dropMovedFields_.find(type.value); moved != dropMovedFields_.end())
  {
    const auto &descriptor = interner_.descriptor(type);
    for (const auto &fieldName : moved->second)
    {
      const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), fieldName);
      if (found == descriptor.fieldNames.end())
        continue;
      const uint32_t field = static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found));
      if (moveState_.isFieldMoved(local, field))
        throw TypeError(std::format("cannot drop a value with field `{}` moved out", fieldName), span);
    }
  }
  if (impl->generic)
  {
    const hir::DefId method = impl->methods.at("drop");
    const hir::DefId instance = instantiateFunction(method, genericSubstitution, 0, span);
    return {local, instance.value};
  }
  return {local, impl->methods.at("drop").value};
}

auto Checker::hasDrop(TypeId type) const -> bool
{
  for (const auto &candidate : impls_)
  {
    if (candidate.traitName != "Drop")
      continue;
    if (!candidate.generic && candidate.target.value == type.value)
      return true;
    if (candidate.generic)
    {
      Substitution substitution;
      if (matchImplPattern(candidate, type, substitution))
        return true;
    }
  }
  return false;
}

void Checker::registerImpls()
{
  for (const auto &impl : module_->impls)
  {
    if (impl.traitName == "Drop")
    {
      if (impl.methods.size() != 1 || impl.methods.front().name != "drop")
        throw TypeError("Drop impl must define exactly one `drop` method", impl.span);
      if (!impl.genericParameters.empty())
      {
        std::vector<TypeId> parameters;
        std::unordered_map<std::string, TypeId> bindings;
        for (size_t index = 0; index < impl.genericParameters.size(); ++index)
        {
          const TypeId parameter =
              interner_.internTypeParameter(impl.genericParameters[index], static_cast<uint32_t>(index));
          parameters.push_back(parameter);
          bindings.emplace(impl.genericParameters[index], parameter);
        }
        const TypeId target = interner_.resolveInScope(*impl.targetType, bindings);
        if (interner_.descriptor(target).kind != TypeKind::Struct)
          throw TypeError("Drop impl target must be a struct type", impl.span);
        const std::string patternDisplay = interner_.display(target);
        for (const auto &existing : impls_)
          if (existing.generic && existing.traitName == "Drop" &&
              existing.targetPatternDisplay == patternDisplay)
            throw TypeError(std::format("duplicate generic impl for trait `Drop` on `{}`", patternDisplay),
                            impl.span);
        resolveImplMethodSignature(impl.methodIds.at(0), module_->functions.at(impl.methodIds.at(0).value), target);
        signatures_.at(impl.methodIds.at(0).value).genericParameters = parameters;
        signatures_.at(impl.methodIds.at(0).value).genericParameterNames = impl.genericParameters;
        placeholderFunctions_.insert(impl.methodIds.at(0).value);
        ImplInfo info{.traitName = "Drop",
                       .generic = true,
                       .typeParameters = parameters,
                       .targetPatternId = target,
                       .targetPatternDisplay = patternDisplay};
        info.methods.emplace("drop", impl.methodIds.at(0));
        impls_.push_back(std::move(info));
        continue;
      }
      const TypeId target = interner_.resolve(*impl.targetType);
      if (interner_.descriptor(target).kind != TypeKind::Struct)
        throw TypeError("Drop impl target must be a struct type", impl.span);
      if (impl.methods.size() != 1 || impl.methods.front().name != "drop")
        throw TypeError("Drop impl must define exactly one `drop` method", impl.span);
      resolveImplMethodSignature(impl.methodIds.at(0), module_->functions.at(impl.methodIds.at(0).value), target);
      {
        const auto &dropFunction = module_->functions.at(impl.methodIds.at(0).value);
        std::unordered_set<std::string> movedFields;
        collectDropMovedFields(dropFunction.body, dropFunction.parameters.front().local.value, movedFields);
        if (!movedFields.empty())
          dropMovedFields_.emplace(target.value,
                                    std::vector<std::string>{movedFields.begin(), movedFields.end()});
      }
      ImplInfo info{.traitName = "Drop", .target = target};
      info.methods.emplace("drop", impl.methodIds.at(0));
      impls_.push_back(std::move(info));
      continue;
    }
    if (!traits_.contains(impl.traitName))
      throw TypeError(std::format("unknown trait `{}` in impl", impl.traitName), impl.span);
    if (!impl.genericParameters.empty())
    {
      std::vector<TypeId> parameters;
      std::unordered_map<std::string, TypeId> bindings;
      for (size_t index = 0; index < impl.genericParameters.size(); ++index)
      {
        const TypeId parameter =
            interner_.internTypeParameter(impl.genericParameters[index], static_cast<uint32_t>(index));
        parameters.push_back(parameter);
        bindings.emplace(impl.genericParameters[index], parameter);
      }
      const TypeId pattern = interner_.resolveInScope(*impl.targetType, bindings);
      const std::string patternDisplay = interner_.display(pattern);
      for (const auto &existing : impls_)
        if (existing.generic && existing.traitName == impl.traitName &&
            existing.targetPatternDisplay == patternDisplay)
          throw TypeError(
              std::format("duplicate generic impl for trait `{}` on `{}`", impl.traitName, patternDisplay),
              impl.span);
      std::vector<const TraitInfo *> closure;
      std::unordered_set<std::string> seen;
      collectSupertraits(impl.traitName, closure, seen);
      std::unordered_map<std::string, const TraitInfo *> required;
      for (const auto *trait : closure)
        for (const auto &[methodName, _] : trait->methodParameters)
          required.emplace(methodName, trait);
      ImplInfo info{.traitName = impl.traitName,
                     .generic = true,
                     .typeParameters = parameters,
                     .targetPatternId = pattern,
                     .targetPatternDisplay = patternDisplay};
      for (size_t index = 0; index < impl.methods.size(); ++index)
      {
        const auto &method = impl.methods[index];
        if (!required.contains(method.name))
          throw TypeError(
              std::format("impl for trait `{}` provides unknown method `{}`", impl.traitName, method.name),
              method.span);
        resolveImplMethodSignature(impl.methodIds.at(index),
                                    module_->functions.at(impl.methodIds.at(index).value), pattern);
        signatures_.at(impl.methodIds.at(index).value).genericParameters = parameters;
        signatures_.at(impl.methodIds.at(index).value).genericParameterNames = impl.genericParameters;
        placeholderFunctions_.insert(impl.methodIds.at(index).value);
        info.methods.emplace(method.name, impl.methodIds.at(index));
      }
      for (const auto &[methodName, owner] : required)
      {
        if (info.methods.contains(methodName))
          continue;
        const auto defaultMethod = owner->methodDefaults.find(methodName);
        if (defaultMethod == owner->methodDefaults.end())
          throw TypeError(std::format("impl for trait `{}` is missing method `{}`", impl.traitName, methodName),
                          impl.span);
        info.methods.emplace(methodName, defaultMethod->second);
      }
      impls_.push_back(std::move(info));
      continue;
    }
    const TypeId target = interner_.resolve(*impl.targetType);
    if (interner_.descriptor(target).kind == TypeKind::TypeParameter)
      throw TypeError("impl target must be a concrete type", impl.span);
    for (const auto &existing : impls_)
      if (existing.traitName == impl.traitName && existing.target == target)
        throw TypeError(std::format("duplicate impl for trait `{}`", impl.traitName), impl.span);
    std::vector<const TraitInfo *> closure;
    std::unordered_set<std::string> seen;
    collectSupertraits(impl.traitName, closure, seen);
    std::unordered_map<std::string, const TraitInfo *> required;
    for (const auto *trait : closure)
      for (const auto &[methodName, _] : trait->methodParameters)
        required.emplace(methodName, trait);
    ImplInfo info{.traitName = impl.traitName, .target = target};
    for (size_t index = 0; index < impl.methods.size(); ++index)
    {
      const auto &method = impl.methods[index];
      if (!required.contains(method.name))
        throw TypeError(
            std::format("impl for trait `{}` provides unknown method `{}`", impl.traitName, method.name),
            method.span);
      resolveImplMethodSignature(impl.methodIds.at(index), module_->functions.at(impl.methodIds.at(index).value),
                                  target);
      info.methods.emplace(method.name, impl.methodIds.at(index));
    }
    for (const auto &[methodName, owner] : required)
    {
      if (info.methods.contains(methodName))
        continue;
      const auto defaultMethod = owner->methodDefaults.find(methodName);
      if (defaultMethod == owner->methodDefaults.end())
        throw TypeError(std::format("impl for trait `{}` is missing method `{}`", impl.traitName, methodName),
                        impl.span);
      info.methods.emplace(methodName, defaultMethod->second);
    }
    impls_.push_back(std::move(info));
    continue;
  }
}

auto Checker::abiSafetyIssue(TypeId type) const -> std::string
{
  if (isIntegerBuiltin(type) || type == builtin::F32 || type == builtin::F64 || type == builtin::Bool)
    return {};
  const auto &descriptor = interner_.descriptor(type);
  if (descriptor.kind == TypeKind::Struct && descriptor.nominalId.has_value() &&
      reprCStructIds_.contains(*descriptor.nominalId))
    return {};
  return std::format("`{}` is not ABI-safe; extern \"C\" signatures accept fixed-width integers, "
                     "floats, bool, and repr(C) structs",
                     interner_.display(type));
}

void Checker::validateExternSignature(const hir::Function &function, const FunctionTypeIds &signature)
{
  for (size_t index = 0; index < function.parameters.size(); ++index)
    if (const auto issue = abiSafetyIssue(signature.parameters[index]); !issue.empty())
      throw TypeError(std::format("extern \"C\" function `{}`: parameter `{}`: {}", function.name,
                                  function.parameters[index].name, issue),
                      function.parameters[index].span);
  if (signature.returnType != builtin::Unit)
  {
    if (const auto issue = abiSafetyIssue(signature.returnType); !issue.empty())
      throw TypeError(std::format("extern \"C\" function `{}`: {}", function.name, issue), function.span);
    if (signature.returnType == builtin::I8 || signature.returnType == builtin::U8 ||
        signature.returnType == builtin::I16 || signature.returnType == builtin::U16)
      throw TypeError(std::format("extern \"C\" function `{}`: sub-word results are deferred to a later B3 slice",
                                  function.name),
                      function.span);
  }
}

void Checker::validateReprCStruct(const hir::Struct &structure)
{
  if (!structure.genericParameters.empty())
    throw TypeError(std::format("repr(C) struct `{}` cannot be generic", structure.name), structure.span);
  for (const auto &field : structure.fields)
  {
    const TypeId type = interner_.resolve(field.type);
    if (isIntegerBuiltin(type) || type == builtin::F32 || type == builtin::F64)
      continue;
    if (type == builtin::Bool)
      throw TypeError(std::format("bool field `{}` in repr(C) struct `{}` is deferred (C `_Bool` layout "
                                  "differs from NG bool)",
                                  field.name, structure.name),
                      field.span);
    if (interner_.descriptor(type).kind == TypeKind::Struct)
      throw TypeError(
          std::format("nested struct field `{}` in repr(C) struct `{}` is deferred to a later B3 slice",
                      field.name, structure.name),
          field.span);
    throw TypeError(std::format("field `{}` of repr(C) struct `{}` is not ABI-safe (`{}`); repr(C) fields "
                                "must be fixed-width integers or floats",
                                field.name, structure.name, interner_.display(type)),
                    field.span);
  }
}

} // namespace NG::typecheck::detail
