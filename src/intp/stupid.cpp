
#include <algorithm>
#include <ast.hpp>
#include <intp/intp.hpp>
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <module.hpp>
#include <runtime/native_marshaling.hpp>
#include <runtime/value_access.hpp>
#include <runtime/array_layout_access.hpp>
#include <runtime/index_layout_access.hpp>
#include <runtime/tagged_layout_access.hpp>
#include <runtime/struct_layout_access.hpp>
#include <runtime/tuple_layout_access.hpp>
#include <runtime/value_ops.hpp>
#include <token.hpp>
#include <visitor.hpp>

#include <functional>
#include <iterator>

using namespace NG;
using namespace NG::ast;

namespace NG::runtime
{
  using namespace NG::runtime::native;
  static const Str NATIVE_MODULE_CONTEXT_KEY = "$$native_module$$";

  struct NativeModuleContext
  {
    RuntimeRef<StorageCell> module;
    explicit NativeModuleContext(RuntimeRef<StorageCell> module) : module(std::move(module)) {}
  };

  auto get_native_registry() -> Map<Str, Map<Str, NGCallable>> &
  {
    static Map<Str, Map<Str, NGCallable>> natives;
    return natives;
  }

  void register_native_library(Str moduleId, Map<Str, NGCallable> handlers)
  {
    get_native_registry().insert_or_assign(moduleId, handlers);
  }

  void bind_native_library_handlers(const RuntimeRef<StorageCell> &module, const Map<Str, NGCallable> &handlers)
  {
    if (!runtime_is_module_value(module))
    {
      throw RuntimeException("Cannot bind native handlers to non-module value");
    }
    for (const auto &[name, handler] : handlers)
    {
      runtime_module_set_native_function(
          module, name, [module, handler](const NGSelf &self, const NGEnv &env, const NGArgs &args) -> RuntimeRef<StorageCell> {
            auto nativeEnv = fork_runtime_env(env);
            runtime_env_set_state(nativeEnv, NATIVE_MODULE_CONTEXT_KEY, std::make_shared<NativeModuleContext>(module));
            bind_native_arg_slots(nativeEnv, args);
            return handler(self, nativeEnv, args);
          });
    }
  }

  auto current_native_module(const NGEnv &env) -> RuntimeRef<StorageCell>
  {
    auto state = runtime_env_get_state(env, NATIVE_MODULE_CONTEXT_KEY);
    auto context = state ? std::static_pointer_cast<NativeModuleContext>(state) : nullptr;
    return context ? context->module : nullptr;
  }
} // namespace NG::runtime

namespace NG::intp
{

  using namespace NG::runtime;
  using namespace NG::runtime::ops;
  using NG::module::FileBasedExternalModuleLoader;
  using NG::module::get_module_registry;
  using NG::module::standard_library_base_path;

  void ISummarizable::summary() {}

  ISummarizable::~ISummarizable() = default;

  static auto evaluateExprSlot(TokenType optr, const RuntimeRef<StorageCell> &leftParam,
                               const RuntimeRef<StorageCell> &rightParam) -> RuntimeRef<StorageCell>
  {
    switch (optr)
    {
    case TokenType::PLUS:
      return value_add(leftParam, rightParam);
    case TokenType::MINUS:
      return value_subtract(leftParam, rightParam);
    case TokenType::TIMES:
      return value_multiply(leftParam, rightParam);
    case TokenType::DIVIDE:
      return value_divide(leftParam, rightParam);
    case TokenType::MODULUS:
      return value_modulus(leftParam, rightParam);
    case TokenType::EQUAL:
      return make_runtime_boolean(value_equals(leftParam, rightParam));
    case TokenType::NOT_EQUAL:
      return make_runtime_boolean(!value_equals(leftParam, rightParam));
    case TokenType::LE:
      return make_runtime_boolean(!value_greater_than(leftParam, rightParam));
    case TokenType::LT:
      return make_runtime_boolean(value_less_than(leftParam, rightParam));
    case TokenType::GE:
      return make_runtime_boolean(!value_less_than(leftParam, rightParam));
    case TokenType::GT:
      return make_runtime_boolean(value_greater_than(leftParam, rightParam));
    case TokenType::RSHIFT:
      return value_rshift(leftParam, rightParam);
    case TokenType::LSHIFT:
      return value_lshift(leftParam, rightParam);
    case TokenType::BIND:
      throw RuntimeException("Operator = is not supported in expressions, perhaps you mean ':='?");
    default:
      throw RuntimeException("Unsupported binary operator");
    }
  }

  struct RuntimeTraitInfo
  {
    Vec<TraitDef *> superTraits;
    Map<Str, FunctionDef *> methods;
    Map<Str, FunctionDef *> defaultMethods;
    Map<Str, FunctionDef *> allDefaultMethods;
    Map<Str, Str> allDefaultOrigins;
    Map<Str, Str> allMethodOrigins;
  };

  static constexpr const char *COPY_TRAIT_NAME = "Copy";
  static constexpr const char *CLONE_TRAIT_NAME = "Clone";
  static constexpr const char *DROP_TRAIT_NAME = "Drop";

  static void install_builtin_lifecycle_traits(const NGSymbols &symbols,
                                               Map<Str, RuntimeTraitInfo> &runtimeTraits)
  {
    if (symbols)
    {
      symbols->traitNames.insert(COPY_TRAIT_NAME);
      symbols->traitNames.insert(CLONE_TRAIT_NAME);
      symbols->traitNames.insert(DROP_TRAIT_NAME);
    }
    runtimeTraits.try_emplace(COPY_TRAIT_NAME);
    runtimeTraits.try_emplace(CLONE_TRAIT_NAME);
    runtimeTraits.try_emplace(DROP_TRAIT_NAME);
  }

  static auto resolve_trait_closure(const Str &traitName, const Map<Str, TraitDef *> &traitDefs,
                                    Map<Str, RuntimeTraitInfo> &traits, Set<Str> &visiting,
                                    Set<Str> &visited) -> RuntimeTraitInfo &
  {
    if (visited.contains(traitName))
    {
      return traits[traitName];
    }
    if (!visiting.insert(traitName).second)
    {
      throw RuntimeException("Cyclic trait inheritance involving " + traitName);
    }
    auto traitDefIt = traitDefs.find(traitName);
    if (traitDefIt == traitDefs.end())
    {
      throw RuntimeException("Unknown trait: " + traitName);
    }
    auto &info = traits[traitName];
    info = RuntimeTraitInfo{};
    auto *traitDef = traitDefIt->second;
    for (const auto &superTraitAnnotation : traitDef->superTraits)
    {
      auto superName = superTraitAnnotation->repr();
      auto &superInfo = resolve_trait_closure(superName, traitDefs, traits, visiting, visited);
      info.superTraits.push_back(traitDefs.at(superName));
      for (auto &[methodName, method] : superInfo.methods)
      {
        info.methods[methodName] = method;
        info.allMethodOrigins[methodName] =
            superInfo.allMethodOrigins.contains(methodName) ? superInfo.allMethodOrigins[methodName] : superName;
      }
      for (auto &[methodName, method] : superInfo.allDefaultMethods)
      {
        if (info.allDefaultMethods.contains(methodName))
        {
          throw RuntimeException("Conflicting default trait method " + methodName + " inherited by " + traitName);
        }
        info.allDefaultMethods[methodName] = method;
        info.allDefaultOrigins[methodName] =
            superInfo.allDefaultOrigins.contains(methodName) ? superInfo.allDefaultOrigins[methodName] : superName;
      }
    }
    for (const auto &method : traitDef->methods)
    {
      info.methods[method->funName] = method.get();
      info.allMethodOrigins[method->funName] = traitName;
      if (method->body)
      {
        info.defaultMethods[method->funName] = method.get();
        info.allDefaultMethods[method->funName] = method.get();
        info.allDefaultOrigins[method->funName] = traitName;
      }
      else
      {
        info.allDefaultMethods.erase(method->funName);
        info.allDefaultOrigins.erase(method->funName);
      }
    }
    visiting.erase(traitName);
    visited.insert(traitName);
    return info;
  }

  static auto stripGenericTypeSuffix(const Str &typeName) -> Str
  {
    auto genericStart = typeName.find('<');
    if (genericStart == Str::npos)
    {
      return typeName;
    }
    return typeName.substr(0, genericStart);
  }

  static auto is_self_type_annotation(const TypeAnnotation *annotation) -> bool
  {
    return annotation && annotation->name == "Self" && annotation->genericArgs.empty();
  }

  static auto is_ref_self_type_annotation(const TypeAnnotation *annotation) -> bool
  {
    return annotation && annotation->name == "ref" && annotation->genericArgs.size() == 1 &&
           is_self_type_annotation(annotation->genericArgs[0].get());
  }

  static auto is_explicit_receiver_param(const Param *param) -> bool
  {
    return param && (is_self_type_annotation(param->annotatedType.get()) ||
                     is_ref_self_type_annotation(param->annotatedType.get()));
  }

  static auto is_variadic_param(const Param *param) -> bool
  {
    return param && param->annotatedType && param->annotatedType->name.size() > 3 &&
           param->annotatedType->name.ends_with("...");
  }

  static auto lookup_global_slot(const NGSymbols &symbols, const Str &name) -> RuntimeRef<StorageCell>
  {
    if (symbols && symbols->objectSlots.contains(name))
    {
      return symbols->objectSlots.at(name);
    }
    return nullptr;
  }

  static auto clone_global_slot(Str name, const RuntimeRef<StorageCell> &source) -> RuntimeRef<StorageCell>;
  static void transfer_reference_ownership(const RuntimeRef<StorageCell> &target,
                                           const RuntimeRef<StorageCell> &source);
  static void drop_storage_cell_if_needed(const NGSymbols &symbols, const RuntimeRef<StorageCell> &cell);

  static void define_global_binding(const NGSymbols &symbols, const Str &name, const RuntimeRef<StorageCell> &value)
  {
    if (!symbols || symbols->objectSlots.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbols->objectSlots[name] = clone_global_slot(name, value);
  }

  static void define_global_function(const NGSymbols &symbols, const Str &name, NGCallable value)
  {
    if (!symbols || symbols->functions.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbols->functions[name] = std::move(value);
  }

  static void define_global_type(const NGSymbols &symbols, const Str &name, const RuntimeRef<NGType> &type)
  {
    if (!symbols || symbols->types.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbols->types[name] = type;
  }

  static void define_global_variant_type(const NGSymbols &symbols, const Str &name, const RuntimeRef<NGType> &type)
  {
    if (!symbols)
    {
      throw RuntimeException("Invalid symbols for variant type registration: " + name);
    }
    if (symbols->variantTypes.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbols->variantTypes[name] = type;
    symbols->types.insert_or_assign(name, type);
  }

  static void define_global_module(const NGSymbols &symbols, const Str &name, const RuntimeRef<StorageCell> &module)
  {
    if (!symbols || symbols->modules.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbols->modules[name] = module;
  }

  static auto resolveRuntimeType(const NGSymbols &symbols, const Str &typeName) -> RuntimeRef<NGType>
  {
    if (symbols && symbols->types.contains(typeName))
    {
      return symbols->types.at(typeName);
    }

    auto baseName = stripGenericTypeSuffix(typeName);
    if (baseName != typeName && symbols && symbols->types.contains(baseName))
    {
      return symbols->types.at(baseName);
    }

    return nullptr;
  }

  static auto resolveRuntimeVariantType(const NGSymbols &symbols, const Str &variantName) -> RuntimeRef<NGType>
  {
    if (symbols && symbols->variantTypes.contains(variantName))
    {
      return symbols->variantTypes.at(variantName);
    }
    if (symbols && symbols->types.contains(variantName))
    {
      auto type = symbols->types.at(variantName);
      if (type && !type->variantName.empty())
      {
        return type;
      }
    }
    return nullptr;
  }

  static void publish_global_binding(const NGSymbols &symbols, const Str &name, const RuntimeRef<StorageCell> &value)
  {
    if (!symbols)
    {
      throw RuntimeException("Invalid assignment to " + name);
    }
    if (symbols->objectSlots.contains(name))
    {
      drop_storage_cell_if_needed(symbols, symbols->objectSlots[name]);
      runtime_copy_storage_cell(symbols->objectSlots[name], value);
      transfer_reference_ownership(symbols->objectSlots[name], value);
      return;
    }
    symbols->objectSlots[name] = clone_global_slot(name, value);
  }

  static void assign_global_binding(const NGSymbols &symbols, const Str &name, const RuntimeRef<StorageCell> &value)
  {
    if (symbols && symbols->objectSlots.contains(name))
    {
      drop_storage_cell_if_needed(symbols, symbols->objectSlots[name]);
      runtime_copy_storage_cell(symbols->objectSlots[name], value);
      transfer_reference_ownership(symbols->objectSlots[name], value);
      return;
    }
    throw RuntimeException("Invalid assignment to " + name);
  }

  static auto is_concrete_layout(const TypeLayout &layout) -> bool
  {
    return layout.kind != LayoutKind::DYNAMIC || layout.size != 0 || !layout.fields.empty() || !layout.variants.empty() ||
           layout.name == "unit";
  }

  static auto concrete_layout_for_type_name(const NGSymbols &symbols, const Str &typeName)
      -> std::optional<TypeLayout>
  {
    auto runtimeType = resolveRuntimeType(symbols, typeName);
    if (!runtimeType)
    {
      return std::nullopt;
    }

    auto layout = runtimeType->layout;
    if (layout.name.empty())
    {
      layout.name = runtimeType->name.empty() ? typeName : runtimeType->name;
    }
    if (layout.kind == LayoutKind::REFERENCE && layout.size == 0)
    {
      layout = buffer_runtime::make_reference_layout(layout.name);
    }

    if (!is_concrete_layout(layout))
    {
      return std::nullopt;
    }
    return layout;
  }

  static auto concrete_layout_for_annotation(const NGSymbols &symbols, TypeAnnotation *annotation)
      -> std::optional<TypeLayout>
  {
    if (!annotation)
    {
      return std::nullopt;
    }
    return concrete_layout_for_type_name(symbols, annotation->repr());
  }

  static auto build_inline_type_layout(const NGSymbols &symbols, const Str &typeName,
                                       const Vec<ASTRef<PropertyDef>> &properties) -> std::optional<TypeLayout>
  {
    LayoutRegistry registry;
    Vec<FieldSpec> fieldSpecs;
    fieldSpecs.reserve(properties.size());

    for (const auto &property : properties)
    {
      auto fieldLayout = concrete_layout_for_annotation(symbols, property->typeAnnotation.get());
      if (!fieldLayout)
      {
        return std::nullopt;
      }
      auto layoutId = registry.register_layout(*fieldLayout);
      fieldSpecs.push_back(FieldSpec{.name = property->propertyName, .layoutId = layoutId});
    }

    return buffer_runtime::make_inline_layout(typeName, fieldSpecs, registry);
  }

  static auto build_tagged_union_type_layout(const NGSymbols &symbols, const TaggedUnionDef *taggedUnion)
      -> std::optional<TypeLayout>
  {
    LayoutRegistry registry;
    Vec<VariantSpec> variants;
    variants.reserve(taggedUnion->variants.size());

    for (const auto &variant : taggedUnion->variants)
    {
      VariantSpec spec{.name = variant.variantName};
      spec.fields.reserve(variant.payloadTypes.size());
      for (size_t i = 0; i < variant.payloadTypes.size(); ++i)
      {
        auto fieldLayout = concrete_layout_for_annotation(symbols, variant.payloadTypes[i].get());
        if (!fieldLayout)
        {
          return std::nullopt;
        }
        auto layoutId = registry.register_layout(*fieldLayout);
        Str fieldName = i < variant.payloadNames.size() && !variant.payloadNames[i].empty()
                            ? variant.payloadNames[i]
                            : std::to_string(i);
        spec.fields.push_back(FieldSpec{.name = std::move(fieldName), .layoutId = layoutId});
      }
      variants.push_back(std::move(spec));
    }

    return buffer_runtime::make_tagged_union_layout(taggedUnion->typeName, variants, registry);
  }

  static auto runtime_symbols_from_env(const NGEnv &env) -> NGSymbols
  {
    return env && env->symbols ? env->symbols : makert<RuntimeSymbolTable>();
  }

  static auto make_named_storage_cell(Str name, const RuntimeRef<StorageCell> &value,
                                      StorageClass storageClass = StorageClass::FRAME) -> RuntimeRef<StorageCell>
  {
    return clone_runtime_storage_cell(value, storageClass, std::move(name));
  }

  static auto clone_argument_slot(Str name, const RuntimeRef<StorageCell> &source,
                                  StorageClass storageClass = StorageClass::TEMPORARY) -> RuntimeRef<StorageCell>
  {
    return clone_runtime_storage_cell(source, storageClass, std::move(name));
  }

  static auto maybe_wrap_trait_object_ref(const NGSymbols &symbols, const RuntimeRef<StorageCell> &value,
                                          const TypeAnnotation *annotation, Str name = {}) -> RuntimeRef<StorageCell>
  {
    if (!annotation || annotation->name != "ref" || annotation->genericArgs.size() != 1)
    {
      return value;
    }
    auto traitName = annotation->genericArgs[0]->repr();
    if (!symbols || !symbols->traitNames.contains(traitName))
    {
      return value;
    }
    auto targetRef = value;
    if (runtime_is_trait_object_ref(targetRef))
    {
      return targetRef;
    }
    if (!runtime_is_reference_value(targetRef))
    {
      return value;
    }
    return make_runtime_trait_object_ref(targetRef, traitName, std::move(name));
  }

  static auto clone_parameter_slot(const Param *param, const RuntimeRef<StorageCell> &source,
                                   const NGSymbols &symbols,
                                   StorageClass storageClass = StorageClass::FRAME) -> RuntimeRef<StorageCell>
  {
    auto value = maybe_wrap_trait_object_ref(symbols, source, param ? param->annotatedType.get() : nullptr,
                                             param ? param->paramName : Str{});
    return clone_argument_slot(param ? param->paramName : Str{}, value, storageClass);
  }

  static auto clone_global_slot(Str name, const RuntimeRef<StorageCell> &source) -> RuntimeRef<StorageCell>
  {
    return clone_argument_slot(std::move(name), source, StorageClass::GLOBAL);
  }

  static void transfer_reference_ownership(const RuntimeRef<StorageCell> &target,
                                           const RuntimeRef<StorageCell> &source)
  {
    (void)target;
    (void)source;
  }

  static auto next_scope_id() -> uint64_t
  {
    static uint64_t nextId = 1;
    return nextId++;
  }

  static auto make_scope_chain() -> RuntimeRef<Vec<uint64_t>>
  {
    return makert<Vec<uint64_t>>(Vec<uint64_t>{next_scope_id()});
  }

  static auto fork_scope_chain(const RuntimeRef<Vec<uint64_t>> &scopeIds) -> RuntimeRef<Vec<uint64_t>>
  {
    auto forked = makert<Vec<uint64_t>>(scopeIds ? *scopeIds : Vec<uint64_t>{});
    forked->push_back(next_scope_id());
    return forked;
  }

  static auto current_scope_id(const RuntimeRef<Vec<uint64_t>> &scopeIds) -> uint64_t
  {
    return scopeIds && !scopeIds->empty() ? scopeIds->back() : 0;
  }

  static auto root_scope_id(const RuntimeRef<Vec<uint64_t>> &scopeIds) -> uint64_t
  {
    return scopeIds && !scopeIds->empty() ? scopeIds->front() : 0;
  }

  static auto find_frame_binding_slot(const RuntimeRef<Vec<CallFrame>> &frames, const RuntimeRef<Vec<uint64_t>> &scopeIds,
                                      const Str &name) -> RuntimeRef<StorageCell>
  {
    if (!frames || frames->empty() || !scopeIds || scopeIds->empty())
    {
      return nullptr;
    }

    auto matchesScope = [&name](const RuntimeRef<StorageCell> &slot, uint64_t scopeId) {
      return slot && slot->name == name && slot->ownerScopeId == scopeId;
    };

    auto &frame = frames->back();
    for (auto itScope = scopeIds->rbegin(); itScope != scopeIds->rend(); ++itScope)
    {
      for (auto it = frame.locals.rbegin(); it != frame.locals.rend(); ++it)
      {
        if (matchesScope(*it, *itScope))
        {
          return *it;
        }
      }
      for (auto it = frame.params.rbegin(); it != frame.params.rend(); ++it)
      {
        if (matchesScope(*it, *itScope))
        {
          return *it;
        }
      }
      if (matchesScope(frame.receiver, *itScope))
      {
        return frame.receiver;
      }
    }
    if (name == "self" && frame.receiver)
    {
      return frame.receiver;
    }
    for (auto it = frame.params.rbegin(); it != frame.params.rend(); ++it)
    {
      if (*it && (*it)->name == name)
      {
        return *it;
      }
    }
    return nullptr;
  }

  static auto find_current_scope_binding_slot(const RuntimeRef<Vec<CallFrame>> &frames,
                                              const RuntimeRef<Vec<uint64_t>> &scopeIds,
                                              const Str &name) -> RuntimeRef<StorageCell>
  {
    if (!frames || frames->empty() || !scopeIds || scopeIds->empty())
    {
      return nullptr;
    }

    auto matchesCurrentScope = [&name, scopeId = current_scope_id(scopeIds)](const RuntimeRef<StorageCell> &slot) {
      return slot && slot->name == name && slot->ownerScopeId == scopeId;
    };

    auto &frame = frames->back();
    for (auto it = frame.locals.rbegin(); it != frame.locals.rend(); ++it)
    {
      if (matchesCurrentScope(*it))
      {
        return *it;
      }
    }
    for (auto it = frame.params.rbegin(); it != frame.params.rend(); ++it)
    {
      if (matchesCurrentScope(*it))
      {
        return *it;
      }
    }
    if (name == "self" && frame.receiver)
    {
      return frame.receiver;
    }
    if (matchesCurrentScope(frame.receiver))
    {
      return frame.receiver;
    }
    return nullptr;
  }

  static auto find_frame_receiver(const RuntimeRef<Vec<CallFrame>> &frames, const RuntimeRef<Vec<uint64_t>> &scopeIds)
      -> RuntimeRef<StorageCell>
  {
    if (!frames || frames->empty() || !scopeIds || scopeIds->empty())
    {
      return nullptr;
    }

    auto receiver = frames->back().receiver;
    if (!receiver)
    {
      return nullptr;
    }

    return receiver;
  }

  static auto is_module_frame(const RuntimeRef<Vec<CallFrame>> &frames) -> bool
  {
    return frames && !frames->empty() && frames->back().functionName == "<module>";
  }

  static void sync_storage_cell(const RuntimeRef<StorageCell> &cell, const RuntimeRef<StorageCell> &value)
  {
    runtime_copy_storage_cell(cell, value);
  }

  static void sync_storage_cell_with_annotation(const RuntimeRef<StorageCell> &cell,
                                                const RuntimeRef<StorageCell> &value,
                                                const NGSymbols &symbols,
                                                const TypeAnnotation *annotation)
  {
    sync_storage_cell(cell, maybe_wrap_trait_object_ref(symbols, value, annotation, cell ? cell->name : Str{}));
  }

  static auto move_storage_cell_into_temporary(const RuntimeRef<StorageCell> &source, Str name = "move")
      -> RuntimeRef<StorageCell>
  {
    ensure_usable_cell(source);
    auto slot = make_storage_cell(source ? source->layout : TypeLayout{}, StorageClass::TEMPORARY, std::move(name),
                                  source ? source->runtimeType : nullptr);
    runtime_copy_storage_cell(slot, source);
    mark_moved_storage_cell(source);
    return slot;
  }

  static void sync_binding_slot(const NGSymbols &symbols, const RuntimeRef<Vec<CallFrame>> &frames,
                                const RuntimeRef<Vec<uint64_t>> &scopeIds, bool publishGlobals,
                                const RuntimeRef<StorageCell> &cell,
                                const RuntimeRef<StorageCell> &value)
  {
    drop_storage_cell_if_needed(symbols, cell);
    sync_storage_cell(cell, value);
    if (publishGlobals && cell && is_module_frame(frames) && cell->ownerScopeId == root_scope_id(scopeIds))
    {
      publish_global_binding(symbols, cell->name, cell);
    }
  }

  static auto drop_target_for_cell(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
  {
    if (!cell || runtime_cell_is_moved(cell) || !runtime_cell_has_value(cell))
    {
      return nullptr;
    }
    if (runtime_is_trait_object_ref(cell))
    {
      return nullptr;
    }
    if (runtime_is_reference_value(cell))
    {
      return nullptr;
    }
    return cell;
  }

  static void drop_storage_cell_if_needed(const NGSymbols &symbols, const RuntimeRef<StorageCell> &cell)
  {
    auto target = drop_target_for_cell(cell);
    if (!target || runtime_cell_is_moved(target) || !runtime_cell_has_value(target) ||
        !target->dropArmed || target->lifecycleDropped || target->dropInProgress)
    {
      return;
    }
    auto type = runtime_value_type(target);
    auto dropChildren = [&symbols](const RuntimeRef<StorageCell> &parent) {
      for (const auto &slot : runtime_cell_slot_refs(parent))
      {
        drop_storage_cell_if_needed(symbols, slot);
      }
      for (const auto &[_, slot] : runtime_cell_named_slot_refs(parent))
      {
        drop_storage_cell_if_needed(symbols, slot);
      }
    };
    if (!type || !type->memberFunctions.contains("Drop::drop"))
    {
      if (type && type->dropCellHandler)
      {
        type->dropCellHandler(target);
      }
      target->lifecycleDropped = true;
      target->dropArmed = false;
      dropChildren(target);
      return;
    }

    target->dropInProgress = true;
    try
    {
      (void)runtime_value_respond_slot(target, "Drop::drop", make_runtime_env(symbols), {});
      target->lifecycleDropped = true;
      target->dropArmed = false;
      dropChildren(target);
      target->dropInProgress = false;
    }
    catch (...)
    {
      target->dropInProgress = false;
      throw;
    }
  }

  static void drop_scope_cells(const NGSymbols &symbols, const RuntimeRef<Vec<CallFrame>> &frames,
                               uint64_t scopeId)
  {
    if (!frames || frames->empty())
    {
      return;
    }
    auto &frame = frames->back();
    for (auto it = frame.locals.rbegin(); it != frame.locals.rend(); ++it)
    {
      if (*it && (*it)->ownerScopeId == scopeId)
      {
        drop_storage_cell_if_needed(symbols, *it);
      }
    }
  }

  static void drop_frame_cells(const NGSymbols &symbols, const CallFrame &frame)
  {
    for (auto it = frame.locals.rbegin(); it != frame.locals.rend(); ++it)
    {
      drop_storage_cell_if_needed(symbols, *it);
    }
    for (auto it = frame.params.rbegin(); it != frame.params.rend(); ++it)
    {
      drop_storage_cell_if_needed(symbols, *it);
    }
  }

  static void drop_current_frame_cells_and_pop(const NGSymbols &symbols, const RuntimeRef<Vec<CallFrame>> &frames)
  {
    if (!frames || frames->empty())
    {
      return;
    }
    try
    {
      drop_frame_cells(symbols, frames->back());
    }
    catch (...)
    {
      frames->pop_back();
      throw;
    }
    frames->pop_back();
  }

  static void drop_scope_cells_noexcept(const NGSymbols &symbols, const RuntimeRef<Vec<CallFrame>> &frames,
                                        uint64_t scopeId) noexcept
  {
    try
    {
      drop_scope_cells(symbols, frames, scopeId);
    }
    catch (...)
    {
      // Destructors cannot propagate Drop failures safely during stack unwinding.
    }
  }

  static void drop_frame_cells_noexcept(const NGSymbols &symbols, const RuntimeRef<Vec<CallFrame>> &frames) noexcept
  {
    try
    {
      drop_current_frame_cells_and_pop(symbols, frames);
    }
    catch (...)
    {
      // Destructors cannot propagate Drop failures safely during stack unwinding.
    }
  }

  static auto has_returned(const RuntimeRef<StorageCell> &returnSlot) -> bool
  {
    return returnSlot && runtime_cell_has_value(returnSlot);
  }

  static auto enumerate_call_frame_roots(const Vec<CallFrame> &frames) -> Vec<RuntimeRef<StorageCell>>
  {
    Vec<RuntimeRef<StorageCell>> roots;
    for (const auto &frame : frames)
    {
      auto appendCell = [&roots](const RuntimeRef<StorageCell> &cell) {
        if (cell)
        {
          roots.push_back(cell);
        }
      };

      appendCell(frame.receiver);
      appendCell(frame.returnSlot);
      for (const auto &cell : frame.params) appendCell(cell);
      for (const auto &cell : frame.locals) appendCell(cell);
      for (const auto &cell : frame.temporaries) appendCell(cell);
    }
    return roots;
  }

  static void materialize_root_frame_bindings(const NGSymbols &symbols, const CallFrame &frame,
                                              uint64_t rootScopeId)
  {
    if (!symbols)
    {
      return;
    }
    for (const auto &slot : frame.locals)
    {
      if (slot && slot->ownerScopeId == rootScopeId)
      {
        ensure_usable_cell(slot);
        publish_global_binding(symbols, slot->name, slot);
      }
    }
  }

  static void define_scope_binding(const NGSymbols &symbols, const RuntimeRef<Vec<CallFrame>> &frames,
                                   const RuntimeRef<Vec<uint64_t>> &scopeIds, bool publishGlobals, const Str &name,
                                   const RuntimeRef<StorageCell> &value)
  {
    if (!frames || frames->empty())
    {
      define_global_binding(symbols, name, value);
      return;
    }
    if (find_current_scope_binding_slot(frames, scopeIds, name))
    {
      throw RuntimeException("Redefine " + name);
    }
    auto slot = make_named_storage_cell(name, value);
    transfer_reference_ownership(slot, value);
    slot->ownerScopeId = current_scope_id(scopeIds);
    frames->back().locals.push_back(slot);
    if (publishGlobals && is_module_frame(frames) && current_scope_id(scopeIds) == root_scope_id(scopeIds))
    {
      publish_global_binding(symbols, name, slot);
    }
  }

  struct FunctionPathVisitor : public DummyVisitor
  {

    Str path;

    void visit(IdExpression *idExpr) override { this->path = idExpr->id; }
  };

  static constexpr const char *ACTIVE_GENERIC_INSTANCE_ENV_KEY = "ng.stupid.active_generic_instance";

  struct ResolvedFunctionCall
  {
    Str basePath;
    Str targetPath;
  };

  static auto resolve_function_call(FunCallExpression *funCallExpr, const Str &activeGenericInstanceName)
      -> ResolvedFunctionCall
  {
    FunctionPathVisitor fpVis{};
    funCallExpr->primaryExpression->accept(&fpVis);

    Str targetPath = fpVis.path;
    if (!activeGenericInstanceName.empty())
    {
      if (auto it = funCallExpr->mangledCalleeNameByInstance.find(activeGenericInstanceName);
          it != funCallExpr->mangledCalleeNameByInstance.end())
      {
        targetPath = it->second;
      }
    }
    else if (!funCallExpr->mangledCalleeName.empty())
    {
      targetPath = funCallExpr->mangledCalleeName;
    }

    return {.basePath = fpVis.path, .targetPath = targetPath};
  }

  static auto infer_active_generic_instance(const RuntimeRef<Vec<CallFrame>> &frames) -> Str
  {
    if (!frames || frames->empty())
    {
      return {};
    }
    const auto &functionName = frames->back().functionName;
    return functionName.starts_with("$NG") ? functionName : Str{};
  }

  struct ExpressionVisitor : public DummyVisitor
  {

    RuntimeRef<StorageCell> slot = nullptr;

    RuntimeRef<Vec<RuntimeRef<StorageCell>>> collection = nullptr;

    NGSymbols symbols = nullptr;
    RuntimeRef<Vec<CallFrame>> activeFrames = nullptr;
    RuntimeRef<Vec<uint64_t>> activeScopes = nullptr;
    bool publishGlobals = false;
    Str activeGenericInstanceName;

    bool moved = false;

    explicit ExpressionVisitor(NGSymbols symbols, RuntimeRef<Vec<CallFrame>> activeFrames = nullptr,
                               RuntimeRef<Vec<uint64_t>> activeScopes = nullptr, bool publishGlobals = false,
                               Str activeGenericInstanceName = {})
        : symbols(std::move(symbols)), activeFrames(std::move(activeFrames)), activeScopes(std::move(activeScopes)),
          publishGlobals(publishGlobals), activeGenericInstanceName(std::move(activeGenericInstanceName))
    {
      if (this->activeGenericInstanceName.empty())
      {
        this->activeGenericInstanceName = infer_active_generic_instance(this->activeFrames);
      }
    }

    [[nodiscard]] auto child_expression_visitor() const -> ExpressionVisitor
    {
      return ExpressionVisitor{symbols, activeFrames, activeScopes, publishGlobals, activeGenericInstanceName};
    }

    void push_shadow_binding(const Str &name, const RuntimeRef<StorageCell> &value,
                             const RuntimeRef<Vec<uint64_t>> &scopeIds) const
    {
      if (!activeFrames || activeFrames->empty())
      {
        throw RuntimeException("Fold expression requires an active call frame");
      }
      auto shadow = clone_argument_slot(name, value);
      shadow->ownerScopeId = current_scope_id(scopeIds);
      activeFrames->back().locals.push_back(shadow);
    }

    [[nodiscard]] auto call_function_by_name(const Str &name, const NGArgs &args, const TokenPosition &pos) const
        -> RuntimeRef<StorageCell>
    {
      if (!symbols || !symbols->functions.contains(name))
      {
        throw RuntimeException("No such function: " + name, pos);
      }
      auto dummy = unit_cell();
      return symbols->functions.at(name)(dummy, make_runtime_env(symbols), args);
    }

    [[nodiscard]] auto sequence_slots(const RuntimeRef<StorageCell> &sequence) const -> Vec<RuntimeRef<StorageCell>>
    {
      try
      {
        return runtime_builtin_sequence_slots(sequence);
      }
      catch (const RuntimeException &ex)
      {
        if (Str{ex.what()}.find("Expected Sequence-compatible runtime value") == Str::npos)
        {
          throw;
        }
      }

      auto env = make_runtime_env(symbols);
      auto sizeSlot = runtime_value_respond_slot(sequence, "size", env, {});
      auto count = read_numeric_cell_as<int64_t>(sizeSlot);
      if (count < 0)
      {
        throw RuntimeException("Sequence size cannot be negative");
      }
      Vec<RuntimeRef<StorageCell>> result;
      result.reserve(static_cast<size_t>(count));
      for (int64_t i = 0; i < count; ++i)
      {
        result.push_back(runtime_value_respond_slot(sequence, "get", env,
                                                    {numeral_cell_from_value<int32_t>(static_cast<int32_t>(i))}));
      }
      return result;
    }

    [[nodiscard]] auto fold_sequence_slot(const ASTRef<Expression> &expression) -> RuntimeRef<StorageCell>
    {
      if (auto id = dynamic_ast_cast<IdExpression>(expression))
      {
        if (auto slot = lookup_binding_slot(id->id))
        {
          return slot;
        }
      }
      ExpressionVisitor sequenceVis{symbols, activeFrames, activeScopes, publishGlobals, activeGenericInstanceName};
      expression->accept(&sequenceVis);
      return sequenceVis.result_slot();
    }

    void set_result(const RuntimeRef<StorageCell> &value, bool isMoved = false)
    {
      moved = isMoved;
      slot = value;
    }

    [[nodiscard]] auto result_slot(Str name = {}) const -> RuntimeRef<StorageCell>
    {
      if (slot)
      {
        return moved ? slot : clone_runtime_storage_cell(slot, StorageClass::TEMPORARY, std::move(name));
      }
      auto result = unit_cell();
      result->name = std::move(name);
      return result;
    }

    auto lookup_binding_slot(const Str &name) const -> RuntimeRef<StorageCell>
    {
      if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
      {
        return slot;
      }
      if (auto receiver = find_frame_receiver(activeFrames, activeScopes); runtime_is_structural_value(receiver))
      {
        if (auto index = runtime_structural_field_index(receiver, name))
        {
          return runtime_structural_field_slot(receiver, *index);
        }
        if (auto slot = runtime_structural_property_slot(receiver, name))
        {
          return slot;
        }
      }
      return lookup_global_slot(symbols, name);
    }

    void write_binding(const Str &name, const RuntimeRef<StorageCell> &value) const
    {
      if (!activeFrames || activeFrames->empty())
      {
        assign_global_binding(symbols, name, value);
        return;
      }
      if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
      {
        sync_binding_slot(symbols, activeFrames, activeScopes, publishGlobals, slot, value);
        return;
      }
        if (auto receiver = find_frame_receiver(activeFrames, activeScopes); runtime_is_structural_value(receiver))
        {
          if (runtime_structural_field_index(receiver, name).has_value() || runtime_structural_property_slot(receiver, name))
          {
             runtime_structural_write_member(receiver, name, value);
             return;
         }
      }
      assign_global_binding(symbols, name, value);
    }

    [[nodiscard]] auto place_slot(Expression *expr) -> RuntimeRef<StorageCell>
    {
      if (auto *idExpr = dynamic_cast<IdExpression *>(expr))
      {
        return lookup_binding_slot(idExpr->id);
      }
      if (auto *unaryExpr = dynamic_cast<UnaryExpression *>(expr);
          unaryExpr && unaryExpr->optr && unaryExpr->optr->type == TokenType::TIMES)
      {
        ExpressionVisitor refVisitor{symbols, activeFrames, activeScopes, publishGlobals};
        unaryExpr->operand->accept(&refVisitor);
        auto refSlot = refVisitor.result_slot();
        if (!runtime_is_reference_value(refSlot))
        {
          return nullptr;
        }
        return runtime_reference_target(refSlot);
      }
      if (auto *idAcc = dynamic_cast<IdAccessorExpression *>(expr))
      {
        if (!idAcc->arguments.empty())
        {
          return nullptr;
        }
        auto receiverSlot = place_slot(idAcc->primaryExpression.get());
        if (!receiverSlot)
        {
          ExpressionVisitor receiverVisitor{symbols, activeFrames, activeScopes, publishGlobals};
          idAcc->primaryExpression->accept(&receiverVisitor);
          receiverSlot = receiverVisitor.result_slot();
        }
        if (runtime_is_reference_value(receiverSlot) && !runtime_is_trait_object_ref(receiverSlot))
        {
          receiverSlot = runtime_reference_target(receiverSlot);
        }
        ensure_usable_cell(receiverSlot);

        const auto memberName = idAcc->accessor->repr();
        if (runtime_is_structural_value(receiverSlot))
        {
          return structural_member_slot(receiverSlot, memberName);
        }
        if (auto receiverType = runtime_value_type(receiverSlot);
            receiverType && receiverType->layout.kind == LayoutKind::TAGGED_UNION)
        {
          return tagged_member_slot(receiverSlot, memberName);
        }
        if (runtime_is_tuple_value(receiverSlot))
        {
          return tuple_read_member_slot(receiverSlot, memberName);
        }
        return nullptr;
      }
      if (auto *indexExpr = dynamic_cast<IndexAccessorExpression *>(expr))
      {
        auto primarySlot = place_slot(indexExpr->primary.get());
        if (!primarySlot)
        {
          ExpressionVisitor primaryVisitor{symbols, activeFrames, activeScopes, publishGlobals};
          indexExpr->primary->accept(&primaryVisitor);
          primarySlot = primaryVisitor.result_slot();
        }
        if (runtime_is_reference_value(primarySlot) && !runtime_is_trait_object_ref(primarySlot))
        {
          primarySlot = runtime_reference_target(primarySlot);
        }
        ensure_usable_cell(primarySlot);

        ExpressionVisitor indexVisitor{symbols, activeFrames, activeScopes, publishGlobals};
        indexExpr->accessor->accept(&indexVisitor);
        return runtime_index_slot(primarySlot, indexVisitor.result_slot());
      }
      return nullptr;
    }

    [[nodiscard]] auto maybeReference(Expression *expr) -> RuntimeRef<StorageCell>
    {
      auto slot = place_slot(expr);
      return slot ? make_runtime_reference_cell(slot, expr->repr()) : nullptr;
    }

    [[nodiscard]] auto makeReference(Expression *expr) -> RuntimeRef<StorageCell>
    {
      if (auto reference = maybeReference(expr))
      {
        return reference;
      }
      throw RuntimeException("Unsupported reference target: " + expr->repr());
    }

#pragma region Visit numeral literals

    void visit(IntegralValue<int8_t> *intVal) override
    {
      set_result(numeral_cell_from_value<int8_t>(intVal->value));
    }
    void visit(IntegralValue<uint8_t> *intVal) override
    {
      set_result(numeral_cell_from_value<uint8_t>(intVal->value));
    }
    void visit(IntegralValue<int16_t> *intVal) override
    {
      set_result(numeral_cell_from_value<int16_t>(intVal->value));
    }
    void visit(IntegralValue<uint16_t> *intVal) override
    {
      set_result(numeral_cell_from_value<uint16_t>(intVal->value));
    }
    void visit(IntegralValue<int32_t> *intVal) override
    {
      set_result(numeral_cell_from_value<int32_t>(intVal->value));
    }
    void visit(IntegralValue<uint32_t> *intVal) override
    {
      set_result(numeral_cell_from_value<uint32_t>(intVal->value));
    }
    void visit(IntegralValue<int64_t> *intVal) override
    {
      set_result(numeral_cell_from_value<int64_t>(intVal->value));
    }
    void visit(IntegralValue<uint64_t> *intVal) override
    {
      set_result(numeral_cell_from_value<uint64_t>(intVal->value));
    }

    // void visit(FloatingPointValue<float16_t> *floatVal) override
    // {
    //     object = std::make_shared<FloatingPointValue<float16_t>>(floatVal->value);
    // }

    void visit(FloatingPointValue<float /*float32_t*/> *floatVal) override
    {
      set_result(numeral_cell_from_value<float>(floatVal->value));
    }

    void visit(FloatingPointValue<double /*float64_t*/> *floatVal) override
    {
      set_result(numeral_cell_from_value<double>(floatVal->value));
    }

    // void visit(FloatingPointValue<float128_t> *floatVal) override
    // {
    //     object = std::make_shared<FloatingPointValue<float128_t>>(floatVal->value);
    // }

#pragma endregion

    void visit(StringValue *strVal) override
    {
      set_result(make_runtime_string(strVal->value));
    }

    void visit(BooleanValue *boolVal) override
    {
      set_result(make_runtime_boolean(boolVal->value));
    }

    void visit(TupleLiteral *tuple) override
    {
      Vec<RuntimeRef<StorageCell>> slots;

      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      for (const auto &element : tuple->elements)
      {
        element->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(element); spread)
        {
          auto collection = vis.collection;
          for (size_t i = 0; i < collection->size(); ++i)
          {
            auto slot = collection->at(i);
            slots.push_back(clone_argument_slot(std::to_string(slots.size()), slot));
          }
        }
        else
        {
          slots.push_back(vis.result_slot(std::to_string(slots.size())));
        }
      }

      set_result(make_runtime_tuple_cell(slots));
    }

    void visit(FunCallExpression *funCallExpr) override
    {
      auto foldIt = std::find_if(funCallExpr->arguments.begin(), funCallExpr->arguments.end(), [](const auto &arg) {
        return dynamic_ast_cast<PostfixFoldExpression>(arg) != nullptr;
      });
      if (foldIt != funCallExpr->arguments.end())
      {
        auto foldIndex = static_cast<size_t>(std::distance(funCallExpr->arguments.begin(), foldIt));
        if (funCallExpr->arguments.size() != 2 || (foldIndex != 0 && foldIndex != 1))
        {
          throw RuntimeException("Fold call expects `op(xs..., init)` or `op(init, xs...)`", funCallExpr->pos);
        }
        auto fold = dynamic_ast_cast<PostfixFoldExpression>(*foldIt);
        if (fold->filter)
        {
          throw RuntimeException("Filter marker `?...` is only supported in array literals", fold->pos);
        }
        auto resolvedCall = resolve_function_call(funCallExpr, activeGenericInstanceName);
        auto items = sequence_slots(fold_sequence_slot(fold->expression));

        auto initIndex = foldIndex == 0 ? 1UZ : 0UZ;
        ExpressionVisitor initVis{symbols, activeFrames, activeScopes, publishGlobals, activeGenericInstanceName};
        funCallExpr->arguments[initIndex]->accept(&initVis);
        auto accumulator = initVis.result_slot("fold.acc");
        if (foldIndex == 0)
        {
          for (auto it = items.rbegin(); it != items.rend(); ++it)
          {
            NGArgs args{clone_argument_slot("fold.item", *it), clone_argument_slot("fold.acc", accumulator)};
            accumulator = call_function_by_name(resolvedCall.targetPath, args, funCallExpr->pos);
          }
        }
        else
        {
          for (const auto &item : items)
          {
            NGArgs args{clone_argument_slot("fold.acc", accumulator), clone_argument_slot("fold.item", item)};
            accumulator = call_function_by_name(resolvedCall.targetPath, args, funCallExpr->pos);
          }
        }
        set_result(accumulator);
        return;
      }

      auto resolvedCall = resolve_function_call(funCallExpr, activeGenericInstanceName);
      NGArgs callArgs;

      for (auto &param : funCallExpr->arguments)
      {
        auto vis = child_expression_visitor();
        param->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(param))
        {
          auto collection = vis.collection;
          for (auto &&item : *collection)
          {
            callArgs.push_back(clone_argument_slot("arg." + std::to_string(callArgs.size()), item));
          }
        }
        else
        {
          callArgs.push_back(vis.result_slot("arg." + std::to_string(callArgs.size())));
        }
      }

      auto dummy = unit_cell();

      if (!symbols)
      {
        throw RuntimeException("No such function: " + resolvedCall.targetPath, funCallExpr->pos);
      }

      auto target = resolvedCall.targetPath;
      if (symbols->functions.contains(target))
      {
        set_result(symbols->functions.at(target)(dummy, make_runtime_env(symbols), callArgs));
        return;
      }
      if (!target.empty() && target.starts_with("$NG") && symbols->functions.contains(resolvedCall.basePath))
      {
        auto env = make_runtime_env(symbols);
        runtime_env_set_state(env, ACTIVE_GENERIC_INSTANCE_ENV_KEY, std::make_shared<Str>(target));
        set_result(symbols->functions.at(resolvedCall.basePath)(dummy, env, callArgs));
        return;
      }
      throw RuntimeException("No such function: " + target, funCallExpr->pos);
    }

    void visit(UnaryExpression *unoExpr) override
    {
      switch (unoExpr->optr->type)
      {
      case TokenType::MINUS:
      {
        ExpressionVisitor operandVisitor{symbols, activeFrames, activeScopes, publishGlobals};
        unoExpr->operand->accept(&operandVisitor);
        set_result(negate_numeric_cell(operandVisitor.result_slot()));
        return;
      }
      case TokenType::NOT:
      {
        ExpressionVisitor operandVisitor{symbols, activeFrames, activeScopes, publishGlobals};
        unoExpr->operand->accept(&operandVisitor);
        set_result(make_runtime_boolean(!runtime_value_bool(operandVisitor.result_slot())));
        return;
      }
      case TokenType::KEYWORD_REF:
      case TokenType::AMPERSAND:
        set_result(makeReference(unoExpr->operand.get()));
        return;
      case TokenType::TIMES:
      {
        ExpressionVisitor operandVisitor{symbols, activeFrames, activeScopes, publishGlobals};
        unoExpr->operand->accept(&operandVisitor);
        auto operandSlot = operandVisitor.result_slot();
        if (!runtime_is_reference_value(operandSlot))
        {
          throw RuntimeException("Cannot dereference non-reference value");
        }
        auto targetSlot = runtime_reference_target(operandSlot);
        if (!targetSlot)
        {
          throw RuntimeException("Cannot dereference dangling reference");
        }
        ensure_usable_cell(targetSlot);
        set_result(targetSlot);
        return;
      }
      case TokenType::KEYWORD_MOVE:
      {
        if (auto idExpr = dynamic_ast_cast<IdExpression>(unoExpr->operand))
        {
          RuntimeRef<StorageCell> movedSlot = nullptr;
          if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, idExpr->id))
          {
            movedSlot = move_storage_cell_into_temporary(slot, "move:" + idExpr->id);
          }
          else if (auto slot = lookup_global_slot(symbols, idExpr->id))
          {
            movedSlot = move_storage_cell_into_temporary(slot, "move:" + idExpr->id);
          }
          else
          {
            throw RuntimeException("Cannot move undefined binding " + idExpr->id);
          }
          set_result(movedSlot, true);
          return;
        }
        if (auto deref = dynamic_ast_cast<UnaryExpression>(unoExpr->operand);
            deref && deref->optr && deref->optr->type == TokenType::TIMES)
        {
          ExpressionVisitor operandVisitor{symbols, activeFrames, activeScopes, publishGlobals};
          deref->operand->accept(&operandVisitor);
          auto targetSlot = runtime_reference_target(operandVisitor.result_slot());
          if (!targetSlot)
          {
            throw RuntimeException("Cannot move from non-reference dereference");
          }
          auto movedSlot = move_storage_cell_into_temporary(targetSlot, "move:deref");
          set_result(movedSlot, true);
          return;
        }
        if (auto targetSlot = place_slot(unoExpr->operand.get()))
        {
          auto movedSlot = move_storage_cell_into_temporary(targetSlot, "move:place");
          set_result(movedSlot, true);
          return;
        }
        ExpressionVisitor operandVisitor{symbols, activeFrames, activeScopes, publishGlobals};
        unoExpr->operand->accept(&operandVisitor);
        set_result(operandVisitor.slot, operandVisitor.moved);
        return;
      }
      case TokenType::QUERY:
        throw NotImplementedException("Operator QUERY (?) not implemented yet");
      default:
        throw RuntimeException("Unsupported Operator");
      }
    }

    void visit(BinaryExpression *binExpr) override
    {
      ExpressionVisitor leftVisitor{symbols, activeFrames, activeScopes, publishGlobals};
      ExpressionVisitor rightVisitor{symbols, activeFrames, activeScopes, publishGlobals};
      binExpr->left->accept(&leftVisitor);
      binExpr->right->accept(&rightVisitor);

      set_result(evaluateExprSlot(binExpr->optr->type, leftVisitor.result_slot(), rightVisitor.result_slot()));
    }

    void visit(IdExpression *idExpr) override
    {
      if (auto bindingSlot = lookup_binding_slot(idExpr->id))
      {
        ensure_usable_cell(bindingSlot);
        set_result(bindingSlot);
        return;
      }
      throw RuntimeException("Undefined binding: " + idExpr->id);
    }

    void visit(ArrayLiteral *array) override
    {
      auto appendFold = [&](const ASTRef<PostfixFoldExpression> &fold, Vec<RuntimeRef<StorageCell>> &resultSlots) {
        auto call = dynamic_ast_cast<FunCallExpression>(fold->expression);
        auto driver = call && call->arguments.size() == 1 ? dynamic_ast_cast<IdExpression>(call->arguments[0]) : nullptr;
        if (!call || !driver)
        {
          throw RuntimeException("Map/filter fold expects a single sequence identifier", fold->pos);
        }
        auto items = sequence_slots(fold_sequence_slot(call->arguments[0]));
        auto originalTopLevelDriver = (!activeFrames || activeFrames->empty())
                                          ? clone_runtime_storage_cell(lookup_global_slot(symbols, driver->id),
                                                                       StorageClass::TEMPORARY, driver->id)
                                          : nullptr;
        for (const auto &item : items)
        {
          auto foldScopes = fork_scope_chain(activeScopes);
          ExpressionVisitor bodyVis{symbols, activeFrames, foldScopes, publishGlobals, activeGenericInstanceName};
          if (activeFrames && !activeFrames->empty())
          {
            bodyVis.push_shadow_binding(driver->id, item, foldScopes);
          }
          else
          {
            publish_global_binding(symbols, driver->id, item);
          }
          fold->expression->accept(&bodyVis);
          if (fold->filter)
          {
            if (runtime_value_bool(bodyVis.result_slot()))
            {
              resultSlots.push_back(clone_argument_slot(std::to_string(resultSlots.size()), item));
            }
          }
          else
          {
            resultSlots.push_back(bodyVis.result_slot(std::to_string(resultSlots.size())));
          }
        }
        if (originalTopLevelDriver)
        {
          publish_global_binding(symbols, driver->id, originalTopLevelDriver);
        }
      };

      if (array->elements.size() == 1)
      {
        if (auto fold = dynamic_ast_cast<PostfixFoldExpression>(array->elements[0]))
        {
          Vec<RuntimeRef<StorageCell>> resultSlots;
          appendFold(fold, resultSlots);
          set_result(make_runtime_array_cell(resultSlots));
          return;
        }
      }
      Vec<RuntimeRef<StorageCell>> slots;

      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      for (const auto &element : array->elements)
      {
        if (auto fold = dynamic_ast_cast<PostfixFoldExpression>(element))
        {
          appendFold(fold, slots);
          continue;
        }
        element->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(element); spread)
        {
          auto collection = vis.collection;
          for (size_t i = 0; i < collection->size(); ++i)
          {
            auto slot = collection->at(i);
            slots.push_back(clone_argument_slot(std::to_string(slots.size()), slot));
          }
        }
        else
        {
          slots.push_back(vis.result_slot(std::to_string(slots.size())));
        }
      }

      set_result(make_runtime_array_cell(slots));
    }

    void visit(IndexAccessorExpression *index) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      index->primary->accept(&vis);

      auto primarySlot = vis.result_slot();
      if (runtime_is_reference_value(primarySlot) && !runtime_is_trait_object_ref(primarySlot))
      {
        primarySlot = runtime_reference_target(primarySlot);
      }
      if (runtime_is_trait_object_ref(primarySlot))
      {
        primarySlot = runtime_trait_object_target(primarySlot);
      }
      ensure_usable_cell(primarySlot);

      if (auto range = dynamic_ast_cast<RangeExpression>(index->accessor))
      {
        auto slots = runtime_is_tuple_value(primarySlot) ? runtime_tuple_slots(primarySlot)
                                                         : runtime_array_slots(primarySlot);
        auto resolveBound = [&](const ASTRef<Expression> &bound, size_t defaultValue) -> size_t {
          if (!bound)
          {
            return defaultValue;
          }
          if (auto fromEnd = dynamic_ast_cast<FromEndIndexExpression>(bound))
          {
            ExpressionVisitor boundVis{symbols, activeFrames, activeScopes, publishGlobals};
            fromEnd->index->accept(&boundVis);
            auto offset = read_numeric_cell_as<int32_t>(boundVis.result_slot());
            if (offset < 0 || static_cast<size_t>(offset) > slots.size())
            {
              throw RuntimeException("Index out of bounds: ^" + std::to_string(offset));
            }
            return slots.size() - static_cast<size_t>(offset);
          }
          ExpressionVisitor boundVis{symbols, activeFrames, activeScopes, publishGlobals};
          bound->accept(&boundVis);
          auto value = read_numeric_cell_as<int32_t>(boundVis.result_slot());
          if (value < 0)
          {
            return 0;
          }
          return std::min(static_cast<size_t>(value), slots.size());
        };
        auto start = resolveBound(range->start, 0);
        auto end = resolveBound(range->end, slots.size());
        if (range->inclusive && end < slots.size())
        {
          ++end;
        }
        if (start > end)
        {
          start = end;
        }
        Vec<RuntimeRef<StorageCell>> resultSlots;
        for (size_t i = start; i < end; ++i)
        {
          resultSlots.push_back(clone_runtime_storage_cell(slots[i], StorageClass::TEMPORARY));
        }
        set_result(runtime_is_tuple_value(primarySlot) ? make_runtime_tuple_cell(resultSlots)
                                                       : make_runtime_span_cell(resultSlots));
        return;
      }

      index->accessor->accept(&vis);

      auto accessorSlot = vis.result_slot();

      set_result(runtime_index_read(primarySlot, accessorSlot));
    }

    void visit(RangeExpression *range) override
    {
      if (!range->start || !range->end)
      {
        throw RuntimeException("Open range expressions are only supported inside index access");
      }
      ExpressionVisitor startVis{symbols, activeFrames, activeScopes, publishGlobals};
      range->start->accept(&startVis);
      ExpressionVisitor endVis{symbols, activeFrames, activeScopes, publishGlobals};
      range->end->accept(&endVis);
      set_result(make_runtime_range_cell(startVis.result_slot(), endVis.result_slot(), range->inclusive));
    }

    void visit(FromEndIndexExpression *fromEnd) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      fromEnd->index->accept(&vis);
      set_result(make_runtime_from_end_index(read_numeric_cell_as<int32_t>(vis.result_slot())));
    }

    void visit(IndexAssignmentExpression *index) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      auto primarySlot = place_slot(index->primary.get());
      if (!primarySlot)
      {
        index->primary->accept(&vis);
        primarySlot = vis.result_slot();
      }
      if (runtime_is_reference_value(primarySlot) && !runtime_is_trait_object_ref(primarySlot))
      {
        primarySlot = runtime_reference_target(primarySlot);
      }
      if (runtime_is_trait_object_ref(primarySlot))
      {
        primarySlot = runtime_trait_object_target(primarySlot);
      }
      ensure_usable_cell(primarySlot);

      index->accessor->accept(&vis);

      auto accessorSlot = vis.result_slot();

      index->value->accept(&vis);

      auto valueSlot = vis.result_slot();

      set_result(runtime_index_write(primarySlot, accessorSlot, valueSlot));
    }

    void visit(IdAccessorExpression *idAccExpr) override
    {
      const Str &repr = idAccExpr->accessor->repr();

      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      auto receiverRef = maybeReference(idAccExpr->primaryExpression.get());
      auto receiverSlot = runtime_reference_target(receiverRef);
      if (!receiverSlot)
      {
        idAccExpr->primaryExpression->accept(&vis);
        receiverSlot = vis.result_slot();
      }
      while (runtime_is_reference_value(receiverSlot) && !runtime_is_trait_object_ref(receiverSlot))
      {
        auto target = runtime_reference_target(receiverSlot);
        if (!target)
        {
          break;
        }
        receiverSlot = target;
      }
      ensure_usable_cell(receiverSlot);

      NGArgs callArgs;
      for (const auto &argument : idAccExpr->arguments)
      {
        auto argVis = child_expression_visitor();
        argument->accept(&argVis);
        if (dynamic_ast_cast<SpreadExpression>(argument))
        {
          auto collection = argVis.collection;
          for (auto &&item : *collection)
          {
            callArgs.push_back(clone_argument_slot("arg." + std::to_string(callArgs.size()), item));
          }
        }
        else
        {
          callArgs.push_back(argVis.result_slot("arg." + std::to_string(callArgs.size())));
        }
      }

      set_result(runtime_value_respond_slot(receiverSlot, repr, make_runtime_env(symbols), callArgs));
    }

    void visit(QualifiedTraitCallExpression *qualifiedCall) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      RuntimeRef<StorageCell> receiverSlot;
      size_t firstRegularArg = 0;
      if (qualifiedCall->receiver)
      {
        auto receiverRef = maybeReference(qualifiedCall->receiver.get());
        receiverSlot = runtime_reference_target(receiverRef);
        if (!receiverSlot)
        {
          qualifiedCall->receiver->accept(&vis);
          receiverSlot = vis.result_slot();
        }
      }
      else
      {
        if (qualifiedCall->arguments.empty())
        {
          throw RuntimeException("Trait-qualified call requires a receiver argument", qualifiedCall->pos);
        }
        auto receiverRef = maybeReference(qualifiedCall->arguments.front().get());
        receiverSlot = runtime_reference_target(receiverRef);
        if (!receiverSlot)
        {
          qualifiedCall->arguments.front()->accept(&vis);
          receiverSlot = vis.result_slot();
        }
        firstRegularArg = 1;
      }
      while (runtime_is_reference_value(receiverSlot) && !runtime_is_trait_object_ref(receiverSlot))
      {
        auto target = runtime_reference_target(receiverSlot);
        if (!target)
        {
          break;
        }
        receiverSlot = target;
      }
      ensure_usable_cell(receiverSlot);

      NGArgs callArgs;
      for (size_t i = firstRegularArg; i < qualifiedCall->arguments.size(); ++i)
      {
        auto argVis = child_expression_visitor();
        qualifiedCall->arguments[i]->accept(&argVis);
        if (dynamic_ast_cast<SpreadExpression>(qualifiedCall->arguments[i]))
        {
          auto collection = argVis.collection;
          for (auto &&item : *collection)
          {
            callArgs.push_back(clone_argument_slot("arg." + std::to_string(callArgs.size()), item));
          }
        }
        else
        {
          callArgs.push_back(argVis.result_slot("arg." + std::to_string(callArgs.size())));
        }
      }

      set_result(runtime_value_respond_slot(receiverSlot, qualifiedCall->traitName + "::" + qualifiedCall->methodName,
                                            make_runtime_env(symbols), callArgs));
    }

    void visit(NewObjectExpression *newObj) override
    {
      Str typeName = newObj->targetType ? newObj->targetType->repr() : newObj->typeName;
      if (auto ngType = resolveRuntimeType(symbols, typeName))
      {
        Vec<RuntimeRef<StorageCell>> fields(ngType->properties.size());
        Map<Str, RuntimeRef<StorageCell>> dynamicProperties;

        if (activeFrames)
        {
          CallFrame objectFrame{};
          objectFrame.functionName = "<object-init>";
          activeFrames->push_back(objectFrame);
        }
        struct ObjectFrameGuard
        {
          RuntimeRef<Vec<CallFrame>> frames;
          ~ObjectFrameGuard()
          {
            if (frames && !frames->empty())
            {
              frames->pop_back();
            }
          }
        } objectFrameGuard{activeFrames};
        auto objectScopes = make_scope_chain();
        ExpressionVisitor visitor{symbols, activeFrames, objectScopes, false};

      for (auto &&[name, expr] : newObj->properties)
      {
        expr->accept(&visitor);
        auto resultSlot = visitor.result_slot(name);

          define_scope_binding(symbols, activeFrames, objectScopes, false, name, resultSlot);

          auto it = std::find(ngType->properties.begin(), ngType->properties.end(), name);
          if (it != ngType->properties.end())
          {
            fields[static_cast<size_t>(std::distance(ngType->properties.begin(), it))] = resultSlot;
          }
          else
          {
            dynamicProperties.insert_or_assign(name, resultSlot);
          }
        }

        set_result(allocate_heap_cell(make_runtime_structural_cell(ngType, fields, dynamicProperties), "heap:" + typeName));
        return;
      }

      auto variantType = resolveRuntimeVariantType(symbols, typeName);
      if (!variantType)
      {
        throw RuntimeException("Unknown type for object: " + typeName);
      }

      Vec<RuntimeRef<StorageCell>> payload;
      payload.reserve(variantType->properties.size());

      for (const auto &property : variantType->properties)
      {
        auto it = newObj->properties.find(property);
        if (it == newObj->properties.end())
        {
          throw RuntimeException("Missing payload property '" + property + "' for variant " + typeName);
        }
        ExpressionVisitor visitor{symbols, activeFrames, activeScopes, publishGlobals};
        it->second->accept(&visitor);
        payload.push_back(visitor.result_slot(property));
      }

      set_result(allocate_heap_cell(make_runtime_tagged_cell(variantType, payload), "heap:" + typeName));
    }

    void visit(AssignmentExpression *assignmentExpr) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      assignmentExpr->value->accept(&vis);
      auto resultSlot = vis.result_slot();
      if (auto idexpr = dynamic_ast_cast<IdExpression>(assignmentExpr->target); idexpr)
      {
        write_binding(idexpr->id, resultSlot);
        set_result(resultSlot);
      }
      else if (auto deref = dynamic_ast_cast<UnaryExpression>(assignmentExpr->target);
               deref && deref->optr && deref->optr->type == TokenType::TIMES)
      {
        ExpressionVisitor targetVisitor{symbols, activeFrames, activeScopes, publishGlobals};
        deref->operand->accept(&targetVisitor);
        auto targetSlot = targetVisitor.result_slot();
        if (!runtime_is_reference_value(targetSlot))
        {
          throw RuntimeException("Left-hand side of dereference assignment is not a reference");
        }
        if (auto referenceTarget = runtime_reference_target(targetSlot))
        {
          runtime_copy_storage_cell(referenceTarget, resultSlot);
        }
        else
        {
          runtime_write_reference(targetSlot, resultSlot);
        }
        set_result(resultSlot);
      }
      else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(assignmentExpr->target); idAcc)
      {
        auto targetSlot = place_slot(idAcc->primaryExpression.get());
        if (!targetSlot)
        {
          idAcc->primaryExpression->accept(&vis);
          targetSlot = vis.result_slot();
        }
        if (runtime_is_reference_value(targetSlot) && !runtime_is_trait_object_ref(targetSlot))
        {
          targetSlot = runtime_reference_target(targetSlot);
        }
        ensure_usable_cell(targetSlot);

        const auto memberName = idAcc->accessor->repr();
        if (runtime_is_structural_value(targetSlot))
        {
          if (auto memberSlot = structural_member_slot_or_create(targetSlot, memberName))
          {
            drop_storage_cell_if_needed(symbols, memberSlot);
            runtime_copy_storage_cell(memberSlot, resultSlot);
            set_result(resultSlot);
            return;
          }
        }
        if (runtime_is_tuple_value(targetSlot))
        {
          auto indexSlot = numeral_cell_from_value<int32_t>(std::stoi(memberName));
          runtime_index_write(targetSlot, indexSlot, resultSlot);
          set_result(resultSlot);
          return;
        }
        throw RuntimeException("Left-hand side of member assignment is not assignable: " +
                               idAcc->primaryExpression->repr());
      }
      else
      {
        throw RuntimeException("Invalid assignment: " + assignmentExpr->repr());
      }
      moved = false;
    }

    void visit(TypeCheckingExpression *typeCheckExpr) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      typeCheckExpr->value->accept(&vis);
      auto value = vis.result_slot();
      if (runtime_is_reference_value(value) && !runtime_is_trait_object_ref(value))
      {
        value = runtime_read_reference(value);
      }
      bool result = false;

      if (auto anno = dynamic_ast_cast<TypeAnnotation>(typeCheckExpr->type); anno)
      {
        auto name = anno->repr();
        auto targetType = resolveRuntimeType(symbols, name);
        if (targetType)
        {
          auto valueType = runtime_value_type(value);
          bool matches = *valueType == *targetType;
          if (!matches)
          {
            matches = stripGenericTypeSuffix(valueType->name) == stripGenericTypeSuffix(name);
          }
          result = matches;
        }
        else
        {
          // todo: simply fix this, and will migrate to typechecker later.
          result = runtime_value_type(value)->name == anno->name;
        }
      }
      else if (auto idAcccessor = dynamic_ast_cast<IdAccessorExpression>(typeCheckExpr->type); idAcccessor)
      {
        idAcccessor->primaryExpression->accept(&vis);
        auto name = idAcccessor->accessor->repr();
        auto module = vis.result_slot();
        if (!runtime_is_module_value(module))
        {
          throw RuntimeException("Invalid module to locate type: " + name);
        }
        auto targetType = runtime_module_type_named(module, name);
        if (!targetType)
        {
          throw RuntimeException("Invalid type name, cannot find: " + name);
        }
        result = *(runtime_value_type(value)) == *(targetType);
      }
      else
      {
        throw RuntimeException("Invalid target expression for type checking: " + typeCheckExpr->type->repr());
      }
      set_result(make_runtime_boolean(result));
    }

    void visit(TypeOfExpression * /*typeOfExpr*/) override
    {
      throw RuntimeException("typeof(expr) is only supported in compile-time type queries");
    }
    void visit(SpreadExpression *spreadExpression) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      spreadExpression->expression->accept(&vis);
      auto resultSlot = vis.result_slot();
      try
      {
        collection = makert<Vec<RuntimeRef<StorageCell>>>(sequence_slots(resultSlot));
        set_result(resultSlot);
        return;
      }
      catch (const RuntimeException &)
      {
      }
      throw RuntimeException("Invalid spread expression, expect Sequence-compatible value, but got: " +
                             spreadExpression->expression->repr());
    }

    void visit(PostfixFoldExpression *foldExpression) override
    {
      throw RuntimeException("Postfix fold expression is only supported in array literals or fold calls: " +
                             foldExpression->repr());
    }

    void visit(UnitLiteral *unit) override
    {
      set_result(unit_cell());
    }

    void visit(CastExpression *castExpr) override
    {
      ExpressionVisitor exprVis{symbols, activeFrames, activeScopes, publishGlobals};
      castExpr->expression->accept(&exprVis);
      auto value = exprVis.result_slot();

      // Resolve target type name
      Str targetTypeName;
      if (auto anno = dynamic_ast_cast<TypeAnnotation>(castExpr->targetType))
      {
        targetTypeName = anno->repr();
      }
      else
      {
        throw RuntimeException("Invalid cast target type: " + castExpr->targetType->repr());
      }

      // Check if target is a newtype
      if (auto targetType = resolveRuntimeType(symbols, targetTypeName))
      {
        // If the value is already an NGNewType with the same type, unwrap it
        if (runtime_cell_slot_ref(value, 0) && runtime_value_type(value) && runtime_value_type(value)->name == targetTypeName)
        {
          set_result(value);
          return;
        }

        // Wrap into newtype
        auto newType = makert<NGType>();
        newType->name = targetTypeName;
        newType->layout = targetType->layout;
        set_result(make_runtime_newtype_cell(newType, value));
      }
      else
      {
        // For primitive casts or unknown types, just pass through
        set_result(value);
      }
    }
  };

  struct StatementVisitor : public DummyVisitor
  {
    NGSymbols symbols;
    RuntimeRef<StorageCell> returnSlot;
    RuntimeRef<Vec<CallFrame>> activeFrames;
    RuntimeRef<Vec<uint64_t>> activeScopes;
    bool publishGlobals = false;
    Str currentFunctionName;
    size_t currentFunctionParamCount = 0;
    Str activeGenericInstanceName;

    explicit StatementVisitor(NGSymbols symbols, RuntimeRef<StorageCell> returnSlot = nullptr,
                              RuntimeRef<Vec<CallFrame>> activeFrames = nullptr,
                              RuntimeRef<Vec<uint64_t>> activeScopes = nullptr, bool publishGlobals = false,
                              Str currentFunctionName = {}, size_t currentFunctionParamCount = 0,
                              Str activeGenericInstanceName = {})
        : symbols(std::move(symbols)), returnSlot(std::move(returnSlot)), activeFrames(std::move(activeFrames)),
          activeScopes(std::move(activeScopes)), publishGlobals(publishGlobals),
          currentFunctionName(std::move(currentFunctionName)), currentFunctionParamCount(currentFunctionParamCount),
          activeGenericInstanceName(std::move(activeGenericInstanceName))
    {
      if (this->activeGenericInstanceName.empty())
      {
        this->activeGenericInstanceName = infer_active_generic_instance(this->activeFrames);
      }
    }

    [[nodiscard]] auto child_statement_visitor(RuntimeRef<Vec<uint64_t>> scopes = nullptr) const -> StatementVisitor
    {
      return StatementVisitor{symbols,
                              returnSlot,
                              activeFrames,
                              scopes ? std::move(scopes) : activeScopes,
                              publishGlobals,
                              currentFunctionName,
                              currentFunctionParamCount,
                              activeGenericInstanceName};
    }

    [[nodiscard]] auto expression_visitor(RuntimeRef<Vec<uint64_t>> scopes = nullptr) const -> ExpressionVisitor
    {
      return ExpressionVisitor{symbols,
                               activeFrames,
                               scopes ? std::move(scopes) : activeScopes,
                               publishGlobals,
                               activeGenericInstanceName};
    }

    void capture_binding(const Str &name, const RuntimeRef<StorageCell> &value, bool defineNew)
    {
      if (!activeFrames || activeFrames->empty())
      {
        return;
      }

      auto &frame = activeFrames->back();
      if (defineNew)
      {
        if (find_current_scope_binding_slot(activeFrames, activeScopes, name))
        {
          throw RuntimeException("Redefine " + name);
        }
        auto slot = make_named_storage_cell(name, value);
        transfer_reference_ownership(slot, value);
        slot->ownerScopeId = current_scope_id(activeScopes);
        frame.locals.push_back(slot);
        return;
      }

      if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
      {
        sync_binding_slot(symbols, activeFrames, activeScopes, publishGlobals, slot, value);
      }
    }

    void define_binding(const Str &name, const RuntimeRef<StorageCell> &value)
    {
      if (!activeFrames || activeFrames->empty())
      {
        define_global_binding(symbols, name, value);
        return;
      }
      if (find_current_scope_binding_slot(activeFrames, activeScopes, name))
      {
        throw RuntimeException("Redefine " + name);
      }
      auto slot = clone_argument_slot(name, value, StorageClass::FRAME);
      transfer_reference_ownership(slot, value);
      slot->ownerScopeId = current_scope_id(activeScopes);
      activeFrames->back().locals.push_back(slot);
      if (publishGlobals && is_module_frame(activeFrames) && current_scope_id(activeScopes) == root_scope_id(activeScopes))
      {
        publish_global_binding(symbols, name, slot);
      }
    }

    void assign_binding(const Str &name, const RuntimeRef<StorageCell> &value)
    {
      if (activeFrames && !activeFrames->empty())
      {
        if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
        {
          sync_binding_slot(symbols, activeFrames, activeScopes, publishGlobals, slot, value);
          return;
        }
        if (auto receiver = find_frame_receiver(activeFrames, activeScopes); runtime_is_structural_value(receiver))
        {
          if (runtime_structural_field_index(receiver, name).has_value() || runtime_structural_property_slot(receiver, name))
          {
            if (auto oldSlot = structural_member_slot(receiver, name))
            {
              drop_storage_cell_if_needed(symbols, oldSlot);
            }
            runtime_structural_write_member(receiver, name, value);
            return;
          }
        }
      }
      assign_global_binding(symbols, name, value);
    }

    void visit(ReturnStatement *returnStatement) override
    {
      if (returnStatement->expression)
      {
        if (!currentFunctionName.empty())
        {
          if (auto tailCall = dynamic_ast_cast<FunCallExpression>(returnStatement->expression))
          {
            auto resolvedCall = resolve_function_call(tailCall.get(), activeGenericInstanceName);
            if (resolvedCall.targetPath == currentFunctionName || resolvedCall.basePath == currentFunctionName)
            {
              Vec<RuntimeRef<StorageCell>> slotValues;
              for (auto &&arg : tailCall->arguments)
              {
                auto argVis = expression_visitor();
                arg->accept(&argVis);
                if (auto spread = dynamic_ast_cast<SpreadExpression>(arg); spread)
                {
                  auto collection = argVis.collection;
                  slotValues.insert(slotValues.end(), collection->begin(), collection->end());
                }
                else
                {
                  slotValues.push_back(argVis.result_slot("tail." + std::to_string(slotValues.size())));
                }
              }
              if (slotValues.size() == currentFunctionParamCount)
              {
                throw NextIteration{slotValues};
              }
            }
          }
        }
        auto vis = expression_visitor();
        returnStatement->expression->accept(&vis);
        sync_storage_cell(returnSlot, vis.result_slot());
      }
      else
      {
        sync_storage_cell(returnSlot, unit_cell());
      }
    }

    void visit(IfStatement *ifStmt) override
    {
      auto stmtVis = child_statement_visitor();
      std::optional<bool> evaluatedCondition = ifStmt->evaluatedCondition;
      if (!activeGenericInstanceName.empty())
      {
        if (auto it = ifStmt->evaluatedConditionByInstance.find(activeGenericInstanceName);
            it != ifStmt->evaluatedConditionByInstance.end())
        {
          evaluatedCondition = it->second;
        }
      }
      if (evaluatedCondition.has_value())
      {
        if (evaluatedCondition.value())
        {
          ifStmt->consequence->accept(&stmtVis);
        }
        else if (ifStmt->alternative != nullptr)
        {
          ifStmt->alternative->accept(&stmtVis);
        }
        return;
      }

      auto vis = expression_visitor();
      ifStmt->testing->accept(&vis);
      if (runtime_value_bool(vis.result_slot()))
      {
        ifStmt->consequence->accept(&stmtVis);
      }
      else if (ifStmt->alternative != nullptr)
      {
        ifStmt->alternative->accept(&stmtVis);
      }
    }

    void visit(CompoundStatement *stmt) override
    {
      auto blockScopes = fork_scope_chain(activeScopes);
      auto blockScopeId = current_scope_id(blockScopes);
      struct ScopeDropGuard
      {
        NGSymbols symbols;
        RuntimeRef<Vec<CallFrame>> frames;
        uint64_t scopeId = 0;
        bool active = true;
        ~ScopeDropGuard() noexcept
        {
          if (active)
          {
            drop_scope_cells_noexcept(symbols, frames, scopeId);
          }
        }
        void drop_now()
        {
          if (!active)
          {
            return;
          }
          active = false;
          drop_scope_cells(symbols, frames, scopeId);
        }
      } scopeDropGuard{symbols, activeFrames, blockScopeId};
      StatementVisitor vis{symbols, returnSlot, activeFrames, blockScopes, publishGlobals, currentFunctionName,
                           currentFunctionParamCount};
      for (const auto &innerStmt : stmt->statements)
      {
        innerStmt->accept(&vis);
        if (has_returned(returnSlot))
        {
          break;
        }
      }
      scopeDropGuard.drop_now();
    }

    void visit(ValDefStatement *valDef) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      valDef->value->accept(&vis);

      define_binding(valDef->name,
                     maybe_wrap_trait_object_ref(symbols, vis.result_slot(valDef->name),
                                                 valDef->typeAnnotation.get(), valDef->name));
    }

    void visit(ValueBindingStatement *valBind) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      valBind->value->accept(&vis);

      switch (valBind->type)
      {
      // case BindingType::DIRECT:
      // {
      //     if (valBind->bindings.size() != 1)
      //     {
      //         throw RuntimeException("Invalid binding type: direct binding only allow exactly 1.");
      //     }
      //     auto binding = valBind->bindings[0];
      //     if (binding->name.empty())
      //     {
      //         throw RuntimeException("Direct binding requires a name");
      //     }
      //     context->define(binding->name, result);
      // }
      // break;
      case BindingType::TUPLE_UNPACK:
      {
        auto resultSlot = vis.result_slot();
        if (!runtime_is_tuple_value(resultSlot))
        {
          throw RuntimeException("Tuple unpacking requires a tuple value");
        }
        auto items = runtime_tuple_slots(resultSlot);
        for (auto &&binding : valBind->bindings)
        {
          if (!binding->spreadReceiver)
          {
            if (binding->index >= items.size())
            {
              throw RuntimeException("Tuple unpacking arity mismatch");
            }
            define_binding(binding->name, items.at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
            if (binding->index > items.size())
            {
              throw RuntimeException("Tuple unpacking arity mismatch");
            }
            Vec<RuntimeRef<StorageCell>> values;
            for (auto it = items.begin() + binding->index; it != items.end(); ++it)
            {
              values.push_back(clone_argument_slot(binding->name + "." + std::to_string(values.size()), *it));
            }
            define_binding(binding->name, make_runtime_tuple_cell(values));
          }
        }
      }
      break;

      case BindingType::ARRAY_UNPACK:
      {
        auto resultSlot = vis.result_slot();
        if (!runtime_is_array_value(resultSlot))
        {
          throw RuntimeException("Array unpacking requires an array value");
        }
        auto items = runtime_array_slots(resultSlot);
        for (auto &&binding : valBind->bindings)
        {
          if (!binding->spreadReceiver)
          {
            if (binding->index >= items.size())
            {
              throw RuntimeException("Array unpacking arity mismatch");
            }
            define_binding(binding->name, items.at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
            if (binding->index > items.size())
            {
              throw RuntimeException("Array unpacking arity mismatch");
            }
            Vec<RuntimeRef<StorageCell>> values;
            for (auto it = items.begin() + binding->index; it != items.end(); ++it)
            {
              values.push_back(clone_argument_slot(binding->name + "." + std::to_string(values.size()), *it));
            }
            define_binding(binding->name, make_runtime_array_cell(values));
          }
        }
      }
      break;
      default:
        throw RuntimeException("Invalid binding type: unsupported");
        break;
      }
    }

    void visit(SimpleStatement *simpleStmt) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      simpleStmt->expression->accept(&vis);
    }

    void visit(SwitchStatement *switchStmt) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      auto scrutineeSlot = vis.place_slot(switchStmt->scrutinee.get());
      if (!scrutineeSlot)
      {
        switchStmt->scrutinee->accept(&vis);
        scrutineeSlot = vis.result_slot();
      }
      if (runtime_is_reference_value(scrutineeSlot) && !runtime_is_trait_object_ref(scrutineeSlot))
      {
        scrutineeSlot = runtime_reference_target(scrutineeSlot);
      }
      auto scrutineeType = runtime_value_type(scrutineeSlot);
      if (!scrutineeType || scrutineeType->layout.kind != LayoutKind::TAGGED_UNION)
      {
        throw RuntimeException("Switch scrutinee is not a tagged value");
      }

      const CaseClause *otherwise = nullptr;
      for (auto &c : switchStmt->cases)
      {
        if (c.isOtherwise)
        {
          otherwise = &c;
          continue;
        }
        if (c.variantName == scrutineeType->variantName)
        {
          auto caseScopes = fork_scope_chain(activeScopes);
          StatementVisitor caseVis{symbols, returnSlot, activeFrames, caseScopes, publishGlobals, currentFunctionName,
                                   currentFunctionParamCount};
          auto payloadValues = runtime_cell_slot_refs(scrutineeSlot);
          // Bind payload variables
          for (size_t j = 0; j < c.bindings.size() && j < payloadValues.size(); ++j)
          {
            if (!c.bindings[j].empty())
            {
              caseVis.define_binding(c.bindings[j], payloadValues[j]);
            }
          }
          c.body->accept(&caseVis);
          return;
        }
      }

      if (otherwise != nullptr)
      {
        auto caseScopes = fork_scope_chain(activeScopes);
        StatementVisitor caseVis{symbols, returnSlot, activeFrames, caseScopes, publishGlobals, currentFunctionName,
                                 currentFunctionParamCount};
        otherwise->body->accept(&caseVis);
        return;
      }

      throw RuntimeException("No matching case for variant: " + scrutineeType->variantName);
    }

    void visit(LoopStatement *loopStatement) override
    {
      auto loopScopes = fork_scope_chain(activeScopes);
      ExpressionVisitor vis{symbols, activeFrames, loopScopes, publishGlobals};
      StatementVisitor bindingVis{symbols, returnSlot, activeFrames, loopScopes, publishGlobals, currentFunctionName,
                                  currentFunctionParamCount};
      for (auto &&binding : loopStatement->bindings)
      {
        binding.target->accept(&vis);
        switch (binding.type)
        {
        case LoopBindingType::LOOP_ASSIGN:
          bindingVis.define_binding(binding.name, vis.result_slot(binding.name));
          break;
        default:
          throw RuntimeException("Unsupported loop binding");
        }
      }

      StatementVisitor stmtVis{symbols, returnSlot, activeFrames, loopScopes, publishGlobals, currentFunctionName,
                               currentFunctionParamCount};
      bool stopLoop = false;
      while (!stopLoop)
      {
        try
        {
          loopStatement->loopBody->accept(&stmtVis);
          stopLoop = true;
        }
        catch (NextIteration iter)
        {
          int i = 0;
          for (auto &&slot : iter.slotValues)
          {
            stmtVis.assign_binding(loopStatement->bindings[i].name, slot);
            i++;
          }
        }
      }
    }

    void visit(NextStatement *nextStatement) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      try
      {
        Vec<RuntimeRef<StorageCell>> slotValues{};
        for (auto &&expr : nextStatement->expressions)
        {
            expr->accept(&vis);
            if (auto spread = dynamic_ast_cast<SpreadExpression>(expr); spread)
            {
              auto collection = vis.collection;
              slotValues.insert(slotValues.end(), collection->begin(), collection->end());
            }
            else
            {
              slotValues.push_back(vis.result_slot("next." + std::to_string(slotValues.size())));
            }
        }
        throw NextIteration{slotValues};
      }
      catch (StopIteration)
      {
        // do nothing
      }
    }
  };

  struct Stupid : public Interpreter,
                  DummyVisitor // NOLINT(cppcoreguidelines-special-member-functions)
  {
    NGSymbols symbols = makert<RuntimeSymbolTable>();
    size_t gcRootProviderId = 0;
    size_t gcFinalizerId = 0;

    Vec<Str> modulePaths;
    RuntimeRef<Vec<CallFrame>> activeFrames = makert<Vec<CallFrame>>();
    Map<Str, TraitDef *> traitDefs;
    Map<Str, RuntimeTraitInfo> runtimeTraits;
    Set<Str> exportedImportNames;
    bool loadingPreludeModule = false;

    explicit Stupid(Vec<Str> modulePaths, bool loadingPrelude = false)
        : modulePaths(modulePaths), loadingPreludeModule(loadingPrelude)
    {
      gcRootProviderId = register_gc_root_provider([frames = activeFrames, symbols = symbols]() {
        auto roots = enumerate_symbol_roots(symbols);
        auto frameRoots = enumerate_call_frame_roots(*frames);
        roots.cells.insert(roots.cells.end(), frameRoots.begin(), frameRoots.end());
        return roots;
      });
      gcFinalizerId = register_gc_finalizer([symbols = symbols](const RuntimeRef<StorageCell> &cell) {
        drop_storage_cell_if_needed(symbols, cell);
      });
      if (!loadingPrelude)
      {
        loadPrelude();
      }
    }

    RuntimeRef<StorageCell> asModule() { return make_runtime_module(symbols); }

    void loadPrelude()
    {
      if (symbols->modules.contains("prelude"))
      {
        return;
      }
      auto importPrelude = makeast<ImportDecl>();
      importPrelude->module = "prelude";
      importPrelude->modulePath.push_back("std");
      importPrelude->modulePath.push_back("prelude");
      importPrelude->imports.push_back("*");
      importPrelude->accept(this);
      // load std.prelude by default.
    }

    void appendModulePath(Str path)
    {
      if (std::ranges::find(modulePaths, path) == std::end(modulePaths))
      {
        modulePaths.push_back(path);
      }
    }
    void visit(CompileUnit *compileUnit) override
    {
      if (!compileUnit->path.empty())
      {
        appendModulePath(compileUnit->path);
      }
      compileUnit->module->accept(this);
    }

    void visit(Module *mod) override
    {
      for (auto &&import : mod->imports)
      {
        import->accept(this);
      }

      install_builtin_lifecycle_traits(symbols, runtimeTraits);
      Set<Str> definedSymbols = {};
      for (auto &&def : mod->definitions)
      {
        if (auto traitDef = dynamic_ast_cast<TraitDef>(def))
        {
          traitDefs[traitDef->traitName] = traitDef.get();
          symbols->traitNames.insert(traitDef->traitName);
        }
      }
      Set<Str> visitingTraits;
      Set<Str> visitedTraits;
      for (auto &&[traitName, _traitDef] : traitDefs)
      {
        if (runtimeTraits.contains(traitName))
        {
          continue;
        }
        resolve_trait_closure(traitName, traitDefs, runtimeTraits, visitingTraits, visitedTraits);
      }
      for (auto &&defs : mod->definitions)
      {
        for (auto &&name : defs->names())
        {
          definedSymbols.insert(name);
        }
        defs->accept(this);
      }

      for (auto &&exp : mod->exports)
      {
        if (!definedSymbols.contains(exp) && std::ranges::find(symbols->imported, exp) == symbols->imported.end() &&
            exp != "*")
        {
          throw RuntimeException("Export undefined symbol: " + exp);
        }
      }
      symbols->exports = mod->exports;
      for (const auto &name : exportedImportNames)
      {
        if (std::ranges::find(symbols->exports, name) == symbols->exports.end())
        {
          symbols->exports.push_back(name);
        }
      }
      CallFrame topLevelFrame{};
      topLevelFrame.functionName = "<module>";
      activeFrames->push_back(topLevelFrame);
      auto moduleScopes = make_scope_chain();
      struct FrameGuard
      {
        RuntimeRef<Vec<CallFrame>> frames;
        ~FrameGuard()
        {
          if (frames && !frames->empty())
          {
            frames->pop_back();
          }
        }
      } frameGuard{activeFrames};
      StatementVisitor vis{symbols, nullptr, activeFrames, moduleScopes, true};

      for (const auto &stmt : mod->statements)
      {
        stmt->accept(&vis);
      }
      materialize_root_frame_bindings(symbols, activeFrames->back(), root_scope_id(moduleScopes));
      for (auto &slot : activeFrames->back().locals)
      {
        if (slot && slot->ownerScopeId == root_scope_id(moduleScopes))
        {
          slot->lifecycleDropped = true;
        }
      }
    }

    void visit(ImportDecl *importDecl) override
    {
      if (!symbols->modules.contains(importDecl->module))
      {
        auto moduleId = NG::module::canonical_module_id(importDecl->modulePath);
        if (moduleId.empty())
        {
          moduleId = importDecl->module;
        }
        auto registeredModule = get_module_registry().queryModuleById(moduleId);
        const bool registeredNativeOnly =
            registeredModule && registeredModule->artifact &&
            registeredModule->artifact->format == NG::module::ModuleFormat::Native &&
            !registeredModule->moduleAst;
        const bool registeredNativeStubWithoutSourceFunctions =
            registeredModule && registeredModule->moduleAst && registeredModule->runtimeModule &&
            registeredModule->artifact && registeredModule->artifact->format == NG::module::ModuleFormat::Native &&
            runtime_module_functions(registeredModule->runtimeModule).empty();
        if (registeredModule && registeredModule->runtimeModule && !registeredNativeOnly &&
            !registeredNativeStubWithoutSourceFunctions)
        {
          define_global_module(symbols, importDecl->module, registeredModule->runtimeModule);
        }
      }
      if (!symbols->modules.contains(importDecl->module))
      {
        auto moduleId = NG::module::canonical_module_id(importDecl->modulePath);
        if (moduleId.empty())
        {
          moduleId = importDecl->module;
        }
        try
        {
          NG::module::FileBasedExternalModuleLoader loader{this->modulePaths};
          auto &&moduleInfo = loader.load(importDecl->modulePath);
          const bool nativeStubWithoutSourceFunctions =
              moduleInfo->moduleAst && moduleInfo->runtimeModule && moduleInfo->artifact &&
              moduleInfo->artifact->format == NG::module::ModuleFormat::Native &&
              runtime_module_functions(moduleInfo->runtimeModule).empty();
          if (moduleInfo->runtimeModule && !nativeStubWithoutSourceFunctions)
          {
            define_global_module(symbols, importDecl->module, moduleInfo->runtimeModule);
          }
          else
          {
            auto &&ast = moduleInfo->moduleAst;
            bool loadingPrelude = loadingPreludeModule || moduleInfo->moduleId == "std.prelude";
            Stupid stupid{modulePaths, loadingPrelude};
            ast->accept(&stupid);
            auto runtimeModule = stupid.asModule();
            if (get_native_registry().contains(moduleInfo->moduleId))
            {
              auto &n = get_native_registry()[moduleInfo->moduleId];
              bind_native_library_handlers(runtimeModule, n);
              auto boundNativeFunctions = runtime_module_native_functions(runtimeModule);
              for (const auto &[name, handler] : boundNativeFunctions)
              {
                if (!stupid.symbols->functions.contains(name))
                {
                  define_global_function(stupid.symbols, name, handler);
                }
              }
            }
            moduleInfo->runtimeModule = runtimeModule;
            get_module_registry().addModuleInfo(moduleInfo);
            define_global_module(symbols, importDecl->module, moduleInfo->runtimeModule);
          }
        }
        catch (const RuntimeException &)
        {
          if (auto target = get_module_registry().queryModuleById(moduleId);
              target && target->runtimeModule && target->artifact &&
              target->artifact->format == NG::module::ModuleFormat::Native && !target->moduleAst)
          {
            define_global_module(symbols, importDecl->module, target->runtimeModule);
          }
          else
          {
            throw;
          }
        }
      }
      RuntimeRef<StorageCell> targetModule = symbols->modules.at(importDecl->module);
      auto moduleId = NG::module::canonical_module_id(importDecl->modulePath);
      if (moduleId.empty())
      {
        moduleId = importDecl->module;
      }

      auto importedNames = importInto(symbols, importDecl->imports, targetModule, moduleId);
      if (importDecl->exported)
      {
        exportedImportNames.insert(importedNames.begin(), importedNames.end());
      }

      if (!importDecl->alias.empty())
      {
        define_global_binding(symbols, importDecl->alias, targetModule);
      }
    }

    static auto importInto(const NGSymbols &symbols, Vec<Str> declaredImports,
                           const RuntimeRef<StorageCell> &fromModule, const Str &moduleId) -> Set<Str>
    {
      Set<Str> imports = resolveImports(declaredImports, fromModule);
      std::copy(imports.begin(), imports.end(), std::back_inserter(symbols->imported));
      auto functions = runtime_module_functions(fromModule);
      auto types = runtime_module_types(fromModule);
      auto traitNames = runtime_module_trait_names(fromModule);
      auto objectSlots = runtime_module_object_slots(fromModule);
      auto nativeFunctions = runtime_module_native_functions(fromModule);
      auto originByName = runtime_module_import_origins(fromModule);
      symbols->traitNames.insert(traitNames.begin(), traitNames.end());
      auto moduleSymbols = makert<RuntimeSymbolTable>();
      moduleSymbols->functions = functions;
      for (const auto &[name, handler] : nativeFunctions)
      {
        moduleSymbols->functions.insert_or_assign(name, handler);
      }
      moduleSymbols->types = types;
      moduleSymbols->traitNames = traitNames;
      moduleSymbols->objectSlots = objectSlots;

      for (auto &&imp : imports)
      {
        auto origin = originByName.contains(imp) ? originByName.at(imp) : moduleId;
        auto shouldBind = [&](bool exists) {
          if (!exists)
          {
            symbols->importOrigins.insert_or_assign(imp, origin);
            return true;
          }
          if (auto existingOrigin = symbols->importOrigins.find(imp);
              existingOrigin != symbols->importOrigins.end() && existingOrigin->second == origin)
          {
            return false;
          }
          throw RuntimeException("Import conflict for symbol: " + imp);
        };
        if (functions.contains(imp))
        {
          if (shouldBind(symbols->functions.contains(imp)))
          {
            auto function = functions[imp];
            define_global_function(symbols, imp,
                                   [function = std::move(function), moduleSymbols](const NGSelf &self,
                                                                                    const NGEnv &,
                                                                                    const NGArgs &args) {
                                     return function(self, make_runtime_env(moduleSymbols), args);
                                   });
          }
        }
        if (types.contains(imp))
        {
          if (shouldBind(symbols->types.contains(imp)))
          {
            define_global_type(symbols, imp, types[imp]);
          }
        }
        if (traitNames.contains(imp))
        {
          symbols->traitNames.insert(imp);
        }
        if (objectSlots.contains(imp))
        {
          if (shouldBind(symbols->objectSlots.contains(imp)))
          {
            auto slot = objectSlots[imp];
            define_global_binding(symbols, imp, slot);
          }
        }
        if (nativeFunctions.contains(imp))
        {
          if (shouldBind(symbols->functions.contains(imp)))
          {
            define_global_function(symbols, imp, nativeFunctions[imp]);
          }
        }
      }
      return imports;
    }

    static auto resolveImports(const Vec<Str> &imports, const RuntimeRef<StorageCell> &targetModule) -> Set<Str>
    {
      bool importAll = (std::ranges::find(imports, "*") != end(imports));
      auto exports = runtime_module_exports(targetModule);
      auto imported = runtime_module_imports(targetModule);
      auto functions = runtime_module_functions(targetModule);
      auto types = runtime_module_types(targetModule);
      auto traitNames = runtime_module_trait_names(targetModule);
      auto objectSlots = runtime_module_object_slots(targetModule);
      auto nativeFunctions = runtime_module_native_functions(targetModule);

      bool exportsAll = exports.contains("*");
      Set<Str> exported{};
      if (exportsAll)
      {
        for (auto &&[fnName, _ignored] : functions)
        {
          if (!imported.contains(fnName) || exports.contains(fnName))
          {
            exported.insert(fnName);
          }
        }
        for (auto &&[typeName, _ignored] : types)
        {
          if (!imported.contains(typeName) || exports.contains(typeName))
          {
            exported.insert(typeName);
          }
        }
        for (auto &&traitName : traitNames)
        {
          if (!imported.contains(traitName) || exports.contains(traitName))
          {
            exported.insert(traitName);
          }
        }
        for (auto &&[objName, _ignored] : objectSlots)
        {
          if (!imported.contains(objName) || exports.contains(objName))
          {
            exported.insert(objName);
          }
        }
        for (auto &&[fnName, _ignored] : nativeFunctions)
        {
          if (!imported.contains(fnName) || exports.contains(fnName))
          {
            exported.insert(fnName);
          }
        }
      }
      else
      {
        exported.insert(begin(exports), end(exports));
      }

      if (importAll)
      {
        return exported;
      }

      for (auto &&imp : imports)
      {
        if (!exported.contains(imp))
        {
          throw RuntimeException("Cannot found symbol " + imp + " in module");
        }
      }
      return Set<Str>{begin(imports), end(imports)};
    }

    // virtual void visit(Definition *def);
    // virtual void visit(Param *param);
    void visit(FunctionDef *funDef) override
    {
      if (funDef->native || funDef->deleted)
      {
        return;
      }

      auto functionInvoker =
          [funDef, frames = activeFrames](const NGSelf &dummy, const NGEnv &env,
                                          const NGArgs &args) -> RuntimeRef<StorageCell>
      {
        // Determine if there's a variadic value parameter and at which parameter position.
        int packIndex = -1;
        for (size_t i = 0; i < funDef->params.size(); ++i)
        {
          if (is_variadic_param(funDef->params[i].get()))
          {
            packIndex = static_cast<int>(i);
            break;
          }
        }
        auto callSymbols = runtime_symbols_from_env(env);
        auto activeGenericInstance = Str{};
        if (auto state = std::static_pointer_cast<Str>(runtime_env_get_state(env, ACTIVE_GENERIC_INSTANCE_ENV_KEY)))
        {
          activeGenericInstance = *state;
        }
        auto scopeIds = make_scope_chain();
        CallFrame callFrame{};
        callFrame.functionName = activeGenericInstance.empty() ? funDef->funName : activeGenericInstance;
        callFrame.receiver = dummy;
        if (callFrame.receiver)
        {
          callFrame.receiver->name = "self";
          callFrame.receiver->ownerScopeId = current_scope_id(scopeIds);
        }
        auto returnTypeName = funDef->returnType ? funDef->returnType->repr() : "unit";
        auto returnRuntimeType = resolveRuntimeType(callSymbols, returnTypeName);
        callFrame.returnSlot =
            make_storage_cell(TypeLayout{.name = returnTypeName}, StorageClass::FRAME, "ret", returnRuntimeType);
        callFrame.returnSlot->ownerScopeId = current_scope_id(scopeIds);
        frames->push_back(callFrame);
        struct FrameGuard
        {
          NGSymbols symbols;
          RuntimeRef<Vec<CallFrame>> frames;
          bool active = true;
          ~FrameGuard() noexcept
          {
            if (active)
            {
              drop_frame_cells_noexcept(symbols, frames);
            }
          }
          void drop_now()
          {
            if (!active)
            {
              return;
            }
            active = false;
            drop_current_frame_cells_and_pop(symbols, frames);
          }
        } frameGuard{callSymbols, frames};
        for (size_t i = 0; i < funDef->params.size(); ++i)
        {
          if (static_cast<int>(i) == packIndex)
          {
            // Pack remaining args into a tuple
            Vec<RuntimeRef<StorageCell>> packItems;
            for (size_t j = i; j < args.size(); ++j)
            {
              packItems.push_back(clone_argument_slot(funDef->params[i]->paramName + "." + std::to_string(j - i), args[j]));
            }
            auto slot = make_named_storage_cell(funDef->params[i]->paramName,
                                                make_runtime_tuple_cell(packItems), StorageClass::FRAME);
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
            break; // pack parameter is always the last one
          }
          else if (args.size() > i)
          {
            auto slot = clone_parameter_slot(funDef->params[i].get(), args[i], callSymbols, StorageClass::FRAME);
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
          }
          else if (funDef->params[i]->value != nullptr)
          {
            ExpressionVisitor vis{callSymbols, frames, scopeIds, false, activeGenericInstance};
            funDef->params[i]->value->accept(&vis);
            auto slot = make_named_storage_cell(funDef->params[i]->paramName,
                                                vis.result_slot(funDef->params[i]->paramName), StorageClass::FRAME);
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
          }
          else
          {
            throw RuntimeException("Missing argument for parameter '" + funDef->params[i]->paramName +
                                   "' in function '" + funDef->funName + "'");
          }
        }
        bool tailRecur = true;
        while (tailRecur)
        {
          clear_storage_cell(frames->back().returnSlot);
          try
          {
            StatementVisitor vis{callSymbols, frames->back().returnSlot, frames, scopeIds, false,
                                 frames->back().functionName, funDef->params.size(), activeGenericInstance};
            funDef->body->accept(&vis);
            tailRecur = false;
          }
          catch (NextIteration nextIter)
          {
            tailRecur = true;
            if (packIndex >= 0)
            {
              // For pack parameters: the next values need to be rebound correctly.
              // Non-pack params take individual values from the front;
              // the pack parameter gets the remaining values packed into a tuple.
              for (size_t i = 0; i < funDef->params.size(); ++i)
              {
                if (static_cast<int>(i) == packIndex)
                {
                  // Collect all remaining slot values for the pack
                  Vec<RuntimeRef<StorageCell>> packItems;
                  for (size_t j = i; j < nextIter.slotValues.size(); ++j)
                  {
                    packItems.push_back(nextIter.slotValues[j]);
                   }
                    sync_storage_cell(frames->back().params[i], make_runtime_tuple_cell(packItems));
                  }
                  else if (i < nextIter.slotValues.size())
                  {
                    sync_storage_cell(frames->back().params[i], nextIter.slotValues[i]);
                  }
                }
            }
            else
             {
                for (size_t i = 0; i < nextIter.slotValues.size(); i++)
                {
                  sync_storage_cell(frames->back().params[i], nextIter.slotValues[i]);
                }
             }
          }
        }
        auto result = clone_runtime_storage_cell(frames->back().returnSlot, StorageClass::TEMPORARY);
        frameGuard.drop_now();
        return result;
      };

      define_global_function(symbols, funDef->funName, functionInvoker);
    }

    void visit(Statement *stmt) override
    {
      StatementVisitor vis{symbols, nullptr, activeFrames, make_scope_chain(), true};
      stmt->accept(&vis);
    }

    void visit(ValDef *valDef) override
    {
      StatementVisitor vis{symbols, nullptr, activeFrames, make_scope_chain(), true};

      valDef->body->accept(&vis);
    }

    void visit(TypeDef *typeDef) override
    {
      auto type = makert<NGType>();

      type->name = typeDef->typeName;
      type->layout = build_inline_type_layout(symbols, typeDef->typeName, typeDef->properties)
                         .value_or(TypeLayout{.name = typeDef->typeName, .kind = LayoutKind::DYNAMIC});

      for (const auto &property : typeDef->properties)
      {
        type->properties.push_back(property->propertyName);
      }

      for (const auto &memFn : typeDef->memberFunctions)
      {
        type->memberFunctions[memFn->funName] =
            [memFn, frames = activeFrames](const NGSelf &dummy, const NGEnv &env,
                                           const NGArgs &args) -> RuntimeRef<StorageCell>
        {
          auto callSymbols = runtime_symbols_from_env(env);
          auto scopeIds = make_scope_chain();
          CallFrame callFrame{};
          callFrame.functionName = memFn->funName;
          callFrame.receiver = dummy;
          if (callFrame.receiver)
          {
            callFrame.receiver->name = "self";
            callFrame.receiver->ownerScopeId = current_scope_id(scopeIds);
          }
          auto returnTypeName = memFn->returnType ? memFn->returnType->repr() : "unit";
          auto returnRuntimeType = resolveRuntimeType(callSymbols, returnTypeName);
          callFrame.returnSlot =
              make_storage_cell(TypeLayout{.name = returnTypeName}, StorageClass::FRAME, "ret", returnRuntimeType);
          callFrame.returnSlot->ownerScopeId = current_scope_id(scopeIds);
          frames->push_back(callFrame);
          struct FrameGuard
          {
            NGSymbols symbols;
            RuntimeRef<Vec<CallFrame>> frames;
            bool active = true;
            ~FrameGuard() noexcept
            {
              if (active)
              {
                drop_frame_cells_noexcept(symbols, frames);
              }
            }
            void drop_now()
            {
              if (!active)
              {
                return;
              }
              active = false;
              drop_current_frame_cells_and_pop(symbols, frames);
            }
          } frameGuard{callSymbols, frames};
          NGArgs effectiveArgs = args;
          if (!memFn->params.empty() && is_explicit_receiver_param(memFn->params.front().get()))
          {
            if (is_ref_self_type_annotation(memFn->params.front()->annotatedType.get()))
            {
              effectiveArgs.insert(effectiveArgs.begin(), make_runtime_reference_cell(dummy, "self"));
            }
            else
            {
              effectiveArgs.insert(effectiveArgs.begin(), dummy);
            }
          }
          for (size_t i = 0; i < memFn->params.size(); ++i)
          {
            RuntimeRef<StorageCell> paramSlot;
            if (effectiveArgs.size() > i)
            {
              paramSlot = effectiveArgs[i];
            }
            else if (memFn->params[i]->value != nullptr)
            {
              ExpressionVisitor vis{callSymbols, frames, scopeIds};
              memFn->params[i]->value->accept(&vis);
              paramSlot = vis.result_slot(memFn->params[i]->paramName);
            }
            else
            {
              throw RuntimeException("Missing argument for parameter '" + memFn->params[i]->paramName +
                                     "' in member function '" + memFn->funName + "'");
            }
            auto slot = clone_parameter_slot(memFn->params[i].get(), paramSlot, callSymbols, StorageClass::FRAME);
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
          }

          clear_storage_cell(frames->back().returnSlot);
          StatementVisitor vis{callSymbols, frames->back().returnSlot, frames, scopeIds, false, memFn->funName,
                               memFn->params.size()};
          memFn->body->accept(&vis);
          auto result = clone_runtime_storage_cell(frames->back().returnSlot, StorageClass::TEMPORARY);
          frameGuard.drop_now();
          return result;
        };
      }

      const bool derivesClone = std::ranges::any_of(typeDef->derivedTraits, [](const auto &trait) {
        return trait && trait->repr() == CLONE_TRAIT_NAME;
      });
      if (derivesClone)
      {
        NGCallable cloneMember = [](const NGSelf &self, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
          if (!self)
          {
            return unit_cell();
          }
          return clone_runtime_storage_cell(self, StorageClass::TEMPORARY, "clone");
        };
        type->memberFunctions[CLONE_TRAIT_NAME + Str{"::clone"}] = cloneMember;
        if (!type->memberFunctions.contains("clone"))
        {
          type->memberFunctions["clone"] = std::move(cloneMember);
        }
      }

      define_global_type(symbols, type->name, type);
    }

    void visit(TraitDef *traitDef) override
    {
      if (symbols)
      {
        symbols->traitNames.insert(traitDef->traitName);
      }
    }

    void visit(ImplDef *implDef) override
    {
      auto type = resolveRuntimeType(symbols, implDef->targetType->repr());
      if (!type)
      {
        throw RuntimeException("Cannot implement trait for unknown type: " + implDef->targetType->repr(), implDef->pos);
      }
      auto traitName = stripGenericTypeSuffix(implDef->trait->repr());
      auto traitIt = runtimeTraits.find(traitName);
      if (traitIt == runtimeTraits.end())
      {
        if (!symbols || !symbols->traitNames.contains(traitName))
        {
          throw RuntimeException("Cannot implement unknown trait: " + traitName, implDef->pos);
        }
        traitIt = runtimeTraits.try_emplace(traitName).first;
      }
      const auto &traitInfo = traitIt->second;

      Map<Str, FunctionDef *> providedMethods;
      for (const auto &method : implDef->methods)
      {
        providedMethods[method->funName] = method.get();
      }

      auto registerImplMethod = [&](const Str &memberName, FunctionDef *method) {
        type->memberFunctions[memberName] =
            [method, frames = activeFrames](const NGSelf &dummy, const NGEnv &env,
                                            const NGArgs &args) -> RuntimeRef<StorageCell>
      {
        auto callSymbols = runtime_symbols_from_env(env);
        auto scopeIds = make_scope_chain();
        CallFrame callFrame{};
        callFrame.functionName = method->funName;
        callFrame.receiver = dummy;
        if (callFrame.receiver)
        {
          callFrame.receiver->name = "self";
          callFrame.receiver->ownerScopeId = current_scope_id(scopeIds);
        }
        auto returnTypeName = method->returnType ? method->returnType->repr() : "unit";
        auto returnRuntimeType = resolveRuntimeType(callSymbols, returnTypeName);
        callFrame.returnSlot =
            make_storage_cell(TypeLayout{.name = returnTypeName}, StorageClass::FRAME, "ret", returnRuntimeType);
        callFrame.returnSlot->ownerScopeId = current_scope_id(scopeIds);
        frames->push_back(callFrame);
        struct FrameGuard
        {
          NGSymbols symbols;
          RuntimeRef<Vec<CallFrame>> frames;
          bool active = true;
          ~FrameGuard() noexcept
          {
            if (active)
            {
              drop_frame_cells_noexcept(symbols, frames);
            }
          }
          void drop_now()
          {
            if (!active)
            {
              return;
            }
            active = false;
            drop_current_frame_cells_and_pop(symbols, frames);
          }
        } frameGuard{callSymbols, frames};

        NGArgs effectiveArgs = args;
        if (!method->params.empty() && is_explicit_receiver_param(method->params.front().get()))
        {
          if (is_ref_self_type_annotation(method->params.front()->annotatedType.get()))
          {
            effectiveArgs.insert(effectiveArgs.begin(), make_runtime_reference_cell(dummy, "self"));
          }
          else
          {
            effectiveArgs.insert(effectiveArgs.begin(), dummy);
          }
        }
        for (size_t i = 0; i < method->params.size(); ++i)
        {
          RuntimeRef<StorageCell> paramSlot;
          if (effectiveArgs.size() > i)
          {
            paramSlot = effectiveArgs[i];
          }
          else if (method->params[i]->value != nullptr)
          {
            ExpressionVisitor vis{callSymbols, frames, scopeIds};
            method->params[i]->value->accept(&vis);
            paramSlot = vis.result_slot(method->params[i]->paramName);
          }
          else
          {
            throw RuntimeException("Missing argument for parameter '" + method->params[i]->paramName +
                                   "' in impl method '" + method->funName + "'");
          }
          auto slot = clone_parameter_slot(method->params[i].get(), paramSlot, callSymbols, StorageClass::FRAME);
          slot->ownerScopeId = current_scope_id(scopeIds);
          frames->back().params.push_back(slot);
        }

        clear_storage_cell(frames->back().returnSlot);
        StatementVisitor vis{callSymbols, frames->back().returnSlot, frames, scopeIds, false, method->funName,
                             method->params.size()};
        method->body->accept(&vis);
        auto result = clone_runtime_storage_cell(frames->back().returnSlot, StorageClass::TEMPORARY);
        frameGuard.drop_now();
        return result;
      };
    };

      for (const auto &method : implDef->methods)
      {
        registerImplMethod(traitName + "::" + method->funName, method.get());
        if (!type->memberFunctions.contains(method->funName))
        {
          registerImplMethod(method->funName, method.get());
        }
      }

      for (const auto &[methodName, defaultMethod] : traitInfo.allDefaultMethods)
      {
        if (providedMethods.contains(methodName))
        {
          continue;
        }
        auto originTraitName =
            traitInfo.allDefaultOrigins.contains(methodName) ? traitInfo.allDefaultOrigins.at(methodName) : traitName;
        registerImplMethod(originTraitName + "::" + methodName, defaultMethod);
        if (originTraitName != traitName)
        {
          registerImplMethod(traitName + "::" + methodName, defaultMethod);
        }
        if (!type->memberFunctions.contains(methodName))
        {
          registerImplMethod(methodName, defaultMethod);
        }
      }
    }

    void visit(ConstDef * /*constDef*/) override {}

    void visit(TypeAliasDef *typeAliasDef) override
    {
      if (typeAliasDef->specializationPattern || typeAliasDef->deleted || typeAliasDef->abstract)
      {
        return;
      }
      if (typeAliasDef->nativeOpaque)
      {
        auto nativeType = makert<NGType>();
        nativeType->name = typeAliasDef->aliasName;
        nativeType->layout = buffer_runtime::make_native_handle_layout(typeAliasDef->aliasName);
        define_global_type(symbols, typeAliasDef->aliasName, nativeType);
        return;
      }
      // Type alias is transparent — just register the underlying type under the alias name
      // The type checker resolves aliases; at runtime we store the underlying type directly
      auto underlyingType = makert<NGType>();
      underlyingType->name = typeAliasDef->aliasName;
      underlyingType->layout = concrete_layout_for_annotation(symbols, typeAliasDef->underlyingType.get())
                                   .value_or(TypeLayout{.name = typeAliasDef->aliasName, .kind = LayoutKind::DYNAMIC});
      underlyingType->layout.name = typeAliasDef->aliasName;
      define_global_type(symbols, typeAliasDef->aliasName, underlyingType);
    }

    void visit(NewTypeDef *newTypeDef) override
    {
      // Create a new nominal type for the newtype
      auto newType = makert<NGType>();
      newType->name = newTypeDef->typeName;
      newType->layout = concrete_layout_for_annotation(symbols, newTypeDef->wrappedType.get())
                            .value_or(TypeLayout{.name = newTypeDef->typeName, .kind = LayoutKind::INLINE_VALUE});
      newType->layout.name = newTypeDef->typeName;
      define_global_type(symbols, newTypeDef->typeName, newType);
    }

    void visit(TaggedUnionDef *taggedUnion) override
    {
      auto type = makert<NGType>();
      type->name = taggedUnion->typeName;
      type->layout = build_tagged_union_type_layout(symbols, taggedUnion)
                         .value_or(TypeLayout{.name = taggedUnion->typeName, .kind = LayoutKind::TAGGED_UNION});
      define_global_type(symbols, taggedUnion->typeName, type);

      // Register each variant as a constructor function
      for (int32_t i = 0; i < static_cast<int32_t>(taggedUnion->variants.size()); ++i)
      {
        const auto &variant = taggedUnion->variants[i];
        Str unionName = taggedUnion->typeName;
        Str variantName = variant.variantName;
        int32_t variantIndex = i;
        Vec<Str> payloadNames = variant.payloadNames;

        auto variantType = makert<NGType>();
        variantType->name = unionName;
        variantType->layout = type->layout;
        variantType->properties = payloadNames;
        variantType->variantName = variantName;
        variantType->variantIndex = variantIndex;
        define_global_variant_type(symbols, variantName, variantType);

        define_global_function(symbols, variantName,
          [unionName, variantName, variantIndex, payloadNames](const NGSelf &self, const NGEnv &env,
                                                                 const NGArgs &args) -> RuntimeRef<StorageCell>
          {
            return make_runtime_tagged_cell(unionName, variantName, variantIndex, args, payloadNames);
          });
      }
    }

    void summary() override {}

    ~Stupid() override
    {
      unregister_gc_finalizer(gcFinalizerId);
      unregister_gc_root_provider(gcRootProviderId);
    }
  };

  auto stupid() -> Interpreter *
  {
    NG::library::prelude::do_register();
    NG::library::imgui::do_register();

    return new Stupid(Vec<Str>{
      "",
      NG::module::standard_library_base_path(),
    }); // NOLINT(cppcoreguidelines-owning-memory)
  }

  auto eval_const_function(ast::FunctionDef *target,
                           const Vec<ast::FunctionDef *> &constFunctions,
                           const Vec<RuntimeRef<StorageCell>> &args,
                           Vec<Str> modulePaths) -> RuntimeRef<StorageCell>
  {
    if (!target)
    {
      throw RuntimeException("Cannot evaluate null const function");
    }
    if (modulePaths.empty())
    {
      modulePaths.push_back("");
      modulePaths.push_back(NG::module::standard_library_base_path());
    }
    Stupid runner{std::move(modulePaths)};
    for (auto *fn : constFunctions)
    {
      if (fn && fn->constEval)
      {
        fn->accept(&runner);
      }
    }
    if (!runner.symbols->functions.contains(target->funName))
    {
      target->accept(&runner);
    }
    if (!runner.symbols->functions.contains(target->funName))
    {
      throw RuntimeException("Const function is not callable: " + target->funName);
    }
    return runner.symbols->functions.at(target->funName)(unit_cell(), make_runtime_env(runner.symbols), args);
  }

} // namespace NG::intp
