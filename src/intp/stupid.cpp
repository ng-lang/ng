
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
            runtime_env_set_state(nativeEnv, NATIVE_MODULE_CONTEXT_KEY, module);
            bind_native_arg_slots(nativeEnv, args);
            return handler(self, nativeEnv, args);
          });
    }
  }

  auto current_native_module(const NGEnv &env) -> RuntimeRef<StorageCell>
  {
    auto state = runtime_env_get_state(env, NATIVE_MODULE_CONTEXT_KEY);
    return state ? std::static_pointer_cast<StorageCell>(state) : nullptr;
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

  static auto stripGenericTypeSuffix(const Str &typeName) -> Str
  {
    auto genericStart = typeName.find('<');
    if (genericStart == Str::npos)
    {
      return typeName;
    }
    return typeName.substr(0, genericStart);
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
      runtime_copy_storage_cell(symbols->objectSlots[name], value);
      return;
    }
    symbols->objectSlots[name] = clone_global_slot(name, value);
  }

  static void assign_global_binding(const NGSymbols &symbols, const Str &name, const RuntimeRef<StorageCell> &value)
  {
    if (symbols && symbols->objectSlots.contains(name))
    {
      runtime_copy_storage_cell(symbols->objectSlots[name], value);
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

  static auto clone_global_slot(Str name, const RuntimeRef<StorageCell> &source) -> RuntimeRef<StorageCell>
  {
    return clone_argument_slot(std::move(name), source, StorageClass::GLOBAL);
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
    if (name == "self" && frame.receiver)
    {
      return frame.receiver;
    }
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
    sync_storage_cell(cell, value);
    if (publishGlobals && cell && is_module_frame(frames) && cell->ownerScopeId == root_scope_id(scopeIds))
    {
      publish_global_binding(symbols, cell->name, cell);
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

  struct ExpressionVisitor : public DummyVisitor
  {

    RuntimeRef<StorageCell> slot = nullptr;

    RuntimeRef<Vec<RuntimeRef<StorageCell>>> collection = nullptr;

    NGSymbols symbols = nullptr;
    RuntimeRef<Vec<CallFrame>> activeFrames = nullptr;
    RuntimeRef<Vec<uint64_t>> activeScopes = nullptr;
    bool publishGlobals = false;

    bool moved = false;

    explicit ExpressionVisitor(NGSymbols symbols, RuntimeRef<Vec<CallFrame>> activeFrames = nullptr,
                               RuntimeRef<Vec<uint64_t>> activeScopes = nullptr, bool publishGlobals = false)
        : symbols(std::move(symbols)), activeFrames(std::move(activeFrames)), activeScopes(std::move(activeScopes)),
          publishGlobals(publishGlobals)
    {
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
        if (runtime_is_reference_value(receiverSlot))
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
        if (runtime_is_reference_value(primarySlot))
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
      FunctionPathVisitor fpVis{};
      funCallExpr->primaryExpression->accept(&fpVis);
      NGArgs callArgs;

      for (auto &param : funCallExpr->arguments)
      {
        ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
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

      if (!symbols || !symbols->functions.contains(fpVis.path))
      {
        throw RuntimeException("No such function: " + fpVis.path, funCallExpr->pos);
      }

      set_result(symbols->functions.at(fpVis.path)(dummy, make_runtime_env(symbols), callArgs));
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
      Vec<RuntimeRef<StorageCell>> slots;

      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      for (const auto &element : array->elements)
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

      set_result(make_runtime_array_cell(slots));
    }

    void visit(IndexAccessorExpression *index) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};

      index->primary->accept(&vis);

      auto primarySlot = vis.result_slot();
      if (runtime_is_reference_value(primarySlot))
      {
        primarySlot = runtime_reference_target(primarySlot);
      }
      ensure_usable_cell(primarySlot);

      index->accessor->accept(&vis);

      auto accessorSlot = vis.result_slot();

      set_result(runtime_index_read(primarySlot, accessorSlot));
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
      if (runtime_is_reference_value(primarySlot))
      {
        primarySlot = runtime_reference_target(primarySlot);
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
      if (runtime_is_reference_value(receiverSlot))
      {
        receiverSlot = runtime_reference_target(receiverSlot);
      }
      ensure_usable_cell(receiverSlot);

      NGArgs callArgs;
      for (const auto &argument : idAccExpr->arguments)
      {
        argument->accept(&vis);
        callArgs.push_back(vis.result_slot("arg." + std::to_string(callArgs.size())));
      }

      set_result(runtime_value_respond_slot(receiverSlot, repr, make_runtime_env(symbols), callArgs));
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
        if (runtime_is_reference_value(targetSlot))
        {
          targetSlot = runtime_reference_target(targetSlot);
        }
        ensure_usable_cell(targetSlot);

        const auto memberName = idAcc->accessor->repr();
        if (runtime_is_structural_value(targetSlot))
        {
          if (auto memberSlot = structural_member_slot_or_create(targetSlot, memberName))
          {
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
      if (runtime_is_reference_value(value))
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
      if (runtime_is_tuple_value(resultSlot))
      {
        collection = makert<Vec<RuntimeRef<StorageCell>>>(runtime_tuple_slots(resultSlot));
        set_result(resultSlot);
        return;
      }
      if (runtime_is_array_value(resultSlot))
      {
        collection = makert<Vec<RuntimeRef<StorageCell>>>(runtime_array_slots(resultSlot));
        set_result(resultSlot);
        return;
      }
      throw RuntimeException("Invalid spread expression, expect array or tuple, but got: " +
                             spreadExpression->expression->repr());
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

    explicit StatementVisitor(NGSymbols symbols, RuntimeRef<StorageCell> returnSlot = nullptr,
                              RuntimeRef<Vec<CallFrame>> activeFrames = nullptr,
                              RuntimeRef<Vec<uint64_t>> activeScopes = nullptr, bool publishGlobals = false,
                              Str currentFunctionName = {}, size_t currentFunctionParamCount = 0)
        : symbols(std::move(symbols)), returnSlot(std::move(returnSlot)), activeFrames(std::move(activeFrames)),
          activeScopes(std::move(activeScopes)), publishGlobals(publishGlobals),
          currentFunctionName(std::move(currentFunctionName)), currentFunctionParamCount(currentFunctionParamCount)
    {
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
            FunctionPathVisitor fpVis{};
            tailCall->primaryExpression->accept(&fpVis);
            if (fpVis.path == currentFunctionName)
            {
              Vec<RuntimeRef<StorageCell>> slotValues;
              for (auto &&arg : tailCall->arguments)
              {
                ExpressionVisitor argVis{symbols, activeFrames, activeScopes, publishGlobals};
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
        ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
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
      StatementVisitor stmtVis{symbols, returnSlot, activeFrames, activeScopes, publishGlobals, currentFunctionName,
                               currentFunctionParamCount};
      if (ifStmt->evaluatedCondition.has_value())
      {
        if (ifStmt->evaluatedCondition.value())
        {
          ifStmt->consequence->accept(&stmtVis);
        }
        else if (ifStmt->alternative != nullptr)
        {
          ifStmt->alternative->accept(&stmtVis);
        }
        return;
      }

      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
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
    }

    void visit(ValDefStatement *valDef) override
    {
      ExpressionVisitor vis{symbols, activeFrames, activeScopes, publishGlobals};
      valDef->value->accept(&vis);

      define_binding(valDef->name, vis.result_slot(valDef->name));
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
            define_binding(binding->name, items.at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
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
            define_binding(binding->name, items.at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
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
      if (runtime_is_reference_value(scrutineeSlot))
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
      for (auto &&binding : loopStatement->bindings)
      {
        binding.target->accept(&vis);
        switch (binding.type)
        {
        case LoopBindingType::LOOP_ASSIGN:
          define_binding(binding.name, vis.result_slot(binding.name));
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
            assign_binding(loopStatement->bindings[i].name, slot);
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

    Vec<Str> modulePaths;
    RuntimeRef<Vec<CallFrame>> activeFrames = makert<Vec<CallFrame>>();

    explicit Stupid(Vec<Str> modulePaths, bool loadingPrelude = false)
        : modulePaths(modulePaths)
    {
      gcRootProviderId = register_gc_root_provider([frames = activeFrames, symbols = symbols]() {
        auto roots = enumerate_symbol_roots(symbols);
        auto frameRoots = enumerate_call_frame_roots(*frames);
        roots.cells.insert(roots.cells.end(), frameRoots.begin(), frameRoots.end());
        return roots;
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
      if (auto target = get_module_registry().queryModuleById("std.prelude"); target)
      {
        auto targetModule = target->runtimeModule;
        define_global_module(symbols, target->moduleName, target->runtimeModule);
        importInto(symbols, {"*"}, targetModule);
      }
      else
      {
        // do actual import
        auto importPrelude = makeast<ImportDecl>();
        importPrelude->module = "prelude";
        importPrelude->modulePath.push_back("std");
        importPrelude->modulePath.push_back("prelude");
        importPrelude->imports.push_back("*");
        importPrelude->accept(this);
      }
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

      Set<Str> definedSymbols = {};
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
        if (!definedSymbols.contains(exp) && exp != "*")
        {
          throw RuntimeException("Export undefined symbol: " + exp);
        }
      }
      symbols->exports = mod->exports;
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
    }

    void visit(ImportDecl *importDecl) override
    {
      if (!symbols->modules.contains(importDecl->module))
      {
        // load module
        NG::module::FileBasedExternalModuleLoader loader{this->modulePaths};
        auto &&moduleInfo = loader.load(importDecl->modulePath);
        if (auto target = get_module_registry().queryModuleById(moduleInfo->moduleId); target)
        {
          define_global_module(symbols, importDecl->module, target->runtimeModule);
        }
        else
        {
          auto &&ast = moduleInfo->moduleAst;
          bool loadingPrelude = moduleInfo->moduleId == "std.prelude";
          Stupid stupid{modulePaths, loadingPrelude};
          ast->accept(&stupid);
          auto runtimeModule = stupid.asModule();
          if (get_native_registry().contains(moduleInfo->moduleId))
          {
            auto &n = get_native_registry()[moduleInfo->moduleId];
            bind_native_library_handlers(runtimeModule, n);
          }
          moduleInfo->runtimeModule = runtimeModule;
          get_module_registry().addModuleInfo(moduleInfo);
          define_global_module(symbols, importDecl->module, moduleInfo->runtimeModule);
        }
      }
      RuntimeRef<StorageCell> targetModule = symbols->modules.at(importDecl->module);

      importInto(symbols, importDecl->imports, targetModule);

      if (!importDecl->alias.empty())
      {
        define_global_binding(symbols, importDecl->alias, targetModule);
      }
    }

    static void importInto(const NGSymbols &symbols, Vec<Str> declaredImports,
                           const RuntimeRef<StorageCell> &fromModule)
    {
      Set<Str> imports = resolveImports(declaredImports, fromModule);
      std::copy(imports.begin(), imports.end(), std::back_inserter(symbols->imported));
      auto functions = runtime_module_functions(fromModule);
      auto types = runtime_module_types(fromModule);
      auto objectSlots = runtime_module_object_slots(fromModule);
      auto nativeFunctions = runtime_module_native_functions(fromModule);

      for (auto &&imp : imports)
      {
        if (functions.contains(imp))
        {
          define_global_function(symbols, imp, functions[imp]);
        }
        if (types.contains(imp))
        {
          define_global_type(symbols, imp, types[imp]);
        }
        if (objectSlots.contains(imp))
        {
          auto slot = objectSlots[imp];
          define_global_binding(symbols, imp, slot);
        }
        if (nativeFunctions.contains(imp))
        {
          define_global_function(symbols, imp, nativeFunctions[imp]);
        }
      }
    }

    static auto resolveImports(const Vec<Str> &imports, const RuntimeRef<StorageCell> &targetModule) -> Set<Str>
    {
      bool importAll = (std::ranges::find(imports, "*") != end(imports));
      auto exports = runtime_module_exports(targetModule);
      auto imported = runtime_module_imports(targetModule);
      auto functions = runtime_module_functions(targetModule);
      auto types = runtime_module_types(targetModule);
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
      if (funDef->native)
      {
        return;
      }

      auto functionInvoker =
          [funDef, frames = activeFrames](const NGSelf &dummy, const NGEnv &env,
                                          const NGArgs &args) -> RuntimeRef<StorageCell>
      {
        // Determine if there's a pack parameter and at which position
        int packIndex = -1;
        for (size_t g = 0; g < funDef->genericParams.size(); ++g)
        {
          if (funDef->genericParams[g]->isPack)
          {
            packIndex = static_cast<int>(g);
            break;
          }
        }
        auto callSymbols = runtime_symbols_from_env(env);
        auto scopeIds = make_scope_chain();
        CallFrame callFrame{};
        callFrame.functionName = funDef->funName;
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
          RuntimeRef<Vec<CallFrame>> frames;
          ~FrameGuard()
          {
            if (frames && !frames->empty())
            {
              frames->pop_back();
            }
          }
        } frameGuard{frames};
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
            auto slot = clone_argument_slot(funDef->params[i]->paramName, args[i], StorageClass::FRAME);
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
          }
          else if (funDef->params[i]->value != nullptr)
          {
            ExpressionVisitor vis{callSymbols, frames, scopeIds};
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
          StatementVisitor vis{callSymbols, frames->back().returnSlot, frames, scopeIds, false, funDef->funName,
                               funDef->params.size()};
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
        return clone_runtime_storage_cell(frames->back().returnSlot, StorageClass::TEMPORARY);
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
        type->layout.fields.push_back(FieldLayout{.name = property->propertyName});
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
            RuntimeRef<Vec<CallFrame>> frames;
            ~FrameGuard()
            {
              if (frames && !frames->empty())
              {
                frames->pop_back();
              }
            }
          } frameGuard{frames};
          for (size_t i = 0; i < memFn->params.size(); ++i)
          {
            RuntimeRef<StorageCell> paramSlot;
            if (args.size() > i)
            {
              paramSlot = args[i];
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
            auto slot = clone_argument_slot(memFn->params[i]->paramName, paramSlot, StorageClass::FRAME);
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
          }

          clear_storage_cell(frames->back().returnSlot);
          StatementVisitor vis{callSymbols, frames->back().returnSlot, frames, scopeIds, false, memFn->funName,
                               memFn->params.size()};
          memFn->body->accept(&vis);
          return clone_runtime_storage_cell(frames->back().returnSlot, StorageClass::TEMPORARY);
        };
      }

      define_global_type(symbols, type->name, type);
    }

    void visit(TypeAliasDef *typeAliasDef) override
    {
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

} // namespace NG::intp
