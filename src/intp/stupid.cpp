
#include <algorithm>
#include <ast.hpp>
#include <intp/intp.hpp>
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <module.hpp>
#include <runtime/native_marshaling.hpp>
#include <runtime/value_access.hpp>
#include <runtime/array_layout_access.hpp>
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

  void bind_native_library_handlers(const RuntimeRef<NGModule> &module, const Map<Str, NGCallable> &handlers)
  {
    for (const auto &[name, handler] : handlers)
    {
      module->native_functions.insert_or_assign(
          name, [module, handler](const NGSelf &self, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject> {
            auto nativeContext = context ? context->fork() : makert<NGContext>();
            nativeContext->set_runtime_state(NATIVE_MODULE_CONTEXT_KEY, module);
            bind_native_arg_slots(nativeContext, args);
            return handler(self, nativeContext, args);
          });
    }
  }

  auto current_native_module(const RuntimeRef<NGContext> &context) -> RuntimeRef<NGModule>
  {
    if (context == nullptr)
    {
      return nullptr;
    }
    auto state = context->get_runtime_state(NATIVE_MODULE_CONTEXT_KEY);
    return state ? std::static_pointer_cast<NGModule>(state) : nullptr;
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

  static auto evaluateExpr(TokenType optr, const RuntimeRef<NGObject> &leftParam,
                           const RuntimeRef<NGObject> &rightParam) -> RuntimeRef<NGObject>
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
      return NGObject::boolean(value_equals(leftParam, rightParam));
    case TokenType::NOT_EQUAL:
      return NGObject::boolean(!value_equals(leftParam, rightParam));
    case TokenType::LE:
      return NGObject::boolean(!value_greater_than(leftParam, rightParam));
    case TokenType::LT:
      return NGObject::boolean(value_less_than(leftParam, rightParam));
    case TokenType::GE:
      return NGObject::boolean(!value_less_than(leftParam, rightParam));
    case TokenType::GT:
      return NGObject::boolean(value_greater_than(leftParam, rightParam));
    case TokenType::RSHIFT:
      return value_rshift(leftParam, rightParam);
    case TokenType::LSHIFT:
      return value_lshift(leftParam, rightParam);
    //            case TokenType::ASSIGN:
    case TokenType::BIND:
      throw RuntimeException("Operator = is not supported in expressions, perhaps you mean ':='?");
    default:
      throw RuntimeException("Unsupported binary operator");
      break;
    }
    return nullptr;
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

  static auto resolveRuntimeType(const RuntimeRef<NGContext> &context, const Str &typeName) -> RuntimeRef<NGType>
  {
    if (auto exact = context->get_type(typeName))
    {
      return exact;
    }

    auto baseName = stripGenericTypeSuffix(typeName);
    if (baseName != typeName)
    {
      return context->get_type(baseName);
    }

    return nullptr;
  }

  static auto resolveRuntimeVariantType(const RuntimeRef<NGContext> &context, const Str &variantName) -> RuntimeRef<NGType>
  {
    return context->get_variant_type(variantName);
  }

  static auto lookup_global_binding(const RuntimeRef<NGContext> &context, const Str &name) -> RuntimeRef<NGObject>
  {
    if (!context)
    {
      return nullptr;
    }
    auto globals = context->symbol_table();
    if (globals && globals->objectSlots.contains(name))
    {
      return globals->objectSlots.at(name)->boxedValue;
    }
    return nullptr;
  }

  static void publish_global_binding(const RuntimeRef<NGContext> &context, const Str &name, const RuntimeRef<NGObject> &value)
  {
    if (!context)
    {
      throw RuntimeException("Invalid assignment to " + name);
    }
    auto globals = context->symbol_table();
    if (!globals)
    {
      throw RuntimeException("Invalid assignment to " + name);
    }
    if (globals->objectSlots.contains(name))
    {
      runtime_sync_storage_cell(globals->objectSlots[name], value);
    }
    else
    {
      auto slot = make_boxed_storage_cell(value, StorageClass::GLOBAL);
      slot->name = name;
      slot->ownerContext = context.get();
      globals->objectSlots[name] = slot;
    }
    globals->objects[name] = value;
  }

  static void assign_global_binding(const RuntimeRef<NGContext> &context, const Str &name, const RuntimeRef<NGObject> &value)
  {
    if (!context)
    {
      throw RuntimeException("Invalid assignment to " + name);
    }
    auto globals = context->symbol_table();
    if (globals && globals->objectSlots.contains(name))
    {
      runtime_sync_storage_cell(globals->objectSlots[name], value);
      globals->objects[name] = value;
      return;
    }
    throw RuntimeException("Invalid assignment to " + name);
  }

  static auto materialized_layout(const RuntimeRef<NGObject> &value) -> TypeLayout
  {
    return runtime_value_layout(value);
  }

  static auto is_concrete_layout(const TypeLayout &layout) -> bool
  {
    return layout.kind != LayoutKind::DYNAMIC || layout.size != 0 || !layout.fields.empty() || !layout.variants.empty() ||
           layout.name == "unit";
  }

  static auto concrete_layout_for_type_name(const RuntimeRef<NGContext> &context, const Str &typeName)
      -> std::optional<TypeLayout>
  {
    auto runtimeType = resolveRuntimeType(context, typeName);
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

  static auto concrete_layout_for_annotation(const RuntimeRef<NGContext> &context, TypeAnnotation *annotation)
      -> std::optional<TypeLayout>
  {
    if (!annotation)
    {
      return std::nullopt;
    }
    return concrete_layout_for_type_name(context, annotation->repr());
  }

  static auto build_inline_type_layout(const RuntimeRef<NGContext> &context, const Str &typeName,
                                       const Vec<ASTRef<PropertyDef>> &properties) -> std::optional<TypeLayout>
  {
    LayoutRegistry registry;
    Vec<FieldSpec> fieldSpecs;
    fieldSpecs.reserve(properties.size());

    for (const auto &property : properties)
    {
      auto fieldLayout = concrete_layout_for_annotation(context, property->typeAnnotation.get());
      if (!fieldLayout)
      {
        return std::nullopt;
      }
      auto layoutId = registry.register_layout(*fieldLayout);
      fieldSpecs.push_back(FieldSpec{.name = property->propertyName, .layoutId = layoutId});
    }

    return buffer_runtime::make_inline_layout(typeName, fieldSpecs, registry);
  }

  static auto build_tagged_union_type_layout(const RuntimeRef<NGContext> &context, const TaggedUnionDef *taggedUnion)
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
        auto fieldLayout = concrete_layout_for_annotation(context, variant.payloadTypes[i].get());
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

  static auto make_named_boxed_storage_cell(Str name, const RuntimeRef<NGObject> &value,
                                            StorageClass storageClass = StorageClass::FRAME) -> RuntimeRef<StorageCell>
  {
    return make_storage_cell(materialized_layout(value), storageClass, value, std::move(name));
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
    if (matchesCurrentScope(frame.receiver))
    {
      return frame.receiver;
    }
    return nullptr;
  }

  static auto find_frame_receiver(const RuntimeRef<Vec<CallFrame>> &frames, const RuntimeRef<Vec<uint64_t>> &scopeIds)
      -> RuntimeRef<NGObject>
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

    for (auto itScope = scopeIds->rbegin(); itScope != scopeIds->rend(); ++itScope)
    {
      if (receiver->ownerScopeId == *itScope)
      {
        return receiver->boxedValue;
      }
    }
    return nullptr;
  }

  static auto is_module_frame(const RuntimeRef<Vec<CallFrame>> &frames) -> bool
  {
    return frames && !frames->empty() && frames->back().functionName == "<module>";
  }

  static void sync_storage_cell(const RuntimeRef<StorageCell> &cell, const RuntimeRef<NGObject> &value)
  {
    runtime_sync_storage_cell(cell, value);
  }

  static void sync_binding_slot(const RuntimeRef<NGContext> &context, const RuntimeRef<Vec<CallFrame>> &frames,
                                const RuntimeRef<Vec<uint64_t>> &scopeIds, const RuntimeRef<StorageCell> &cell,
                                const RuntimeRef<NGObject> &value)
  {
    sync_storage_cell(cell, value);
    if (context && cell && is_module_frame(frames) && cell->ownerScopeId == root_scope_id(scopeIds))
    {
      publish_global_binding(context, cell->name, value);
    }
  }

  static auto has_returned(const RuntimeRef<StorageCell> &returnSlot) -> bool
  {
    return returnSlot && returnSlot->boxedValue != nullptr;
  }

  static auto enumerate_call_frame_roots(const Vec<CallFrame> &frames) -> Vec<RuntimeRef<NGObject>>
  {
    Vec<RuntimeRef<NGObject>> roots;
    for (const auto &frame : frames)
    {
      auto appendCell = [&roots](const RuntimeRef<StorageCell> &cell) {
        if (cell && cell->boxedValue)
        {
          roots.push_back(cell->boxedValue);
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

  static void materialize_root_frame_bindings(const RuntimeRef<NGContext> &context, const CallFrame &frame,
                                              uint64_t rootScopeId)
  {
    if (!context)
    {
      return;
    }
    auto globals = context->symbol_table();
    if (!globals)
    {
      return;
    }
    for (const auto &slot : frame.locals)
    {
      if (slot && slot->ownerScopeId == rootScopeId)
      {
        publish_global_binding(context, slot->name, slot->boxedValue);
      }
    }
  }

  static void define_scope_binding(const RuntimeRef<NGContext> &context, const RuntimeRef<Vec<CallFrame>> &frames,
                                   const RuntimeRef<Vec<uint64_t>> &scopeIds, const Str &name,
                                   const RuntimeRef<NGObject> &value)
  {
    if (!frames || frames->empty())
    {
      context->define(name, value);
      return;
    }
    if (find_current_scope_binding_slot(frames, scopeIds, name))
    {
      throw RuntimeException("Redefine " + name);
    }
    auto slot = make_named_boxed_storage_cell(name, value);
    slot->ownerScopeId = current_scope_id(scopeIds);
    frames->back().locals.push_back(slot);
    if (is_module_frame(frames) && context->parent_context() == nullptr &&
        current_scope_id(scopeIds) == root_scope_id(scopeIds))
    {
      publish_global_binding(context, name, value);
    }
  }

  struct FunctionPathVisitor : public DummyVisitor
  {

    Str path;

    void visit(IdExpression *idExpr) override { this->path = idExpr->id; }
  };

  struct ExpressionVisitor : public DummyVisitor
  {

    RuntimeRef<NGObject> object = nullptr;

    RuntimeRef<Vec<RuntimeRef<NGObject>>> collection = nullptr;

    RuntimeRef<NGContext> context = nullptr;
    RuntimeRef<Vec<CallFrame>> activeFrames = nullptr;
    RuntimeRef<Vec<uint64_t>> activeScopes = nullptr;

    bool moved = false;

    explicit ExpressionVisitor(RuntimeRef<NGContext> context, RuntimeRef<Vec<CallFrame>> activeFrames = nullptr,
                               RuntimeRef<Vec<uint64_t>> activeScopes = nullptr)
        : context(std::move(context)), activeFrames(std::move(activeFrames)), activeScopes(std::move(activeScopes))
    {
    }

    auto lookup_binding(const Str &name) const -> RuntimeRef<NGObject>
    {
      if (!activeFrames || activeFrames->empty())
      {
        return context->get(name);
      }
      if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
      {
        return slot->boxedValue;
      }
      if (auto receiver = std::dynamic_pointer_cast<NGStructuralObject>(find_frame_receiver(activeFrames, activeScopes)))
      {
        if (auto value = structural_read_member(receiver, name))
        {
          return value;
        }
      }
      return lookup_global_binding(context, name);
    }

    void write_binding(const Str &name, const RuntimeRef<NGObject> &value) const
    {
      if (!activeFrames || activeFrames->empty())
      {
        context->set(name, value);
        return;
      }
      if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
      {
        sync_binding_slot(context, activeFrames, activeScopes, slot, value);
        return;
      }
      if (auto receiver = std::dynamic_pointer_cast<NGStructuralObject>(find_frame_receiver(activeFrames, activeScopes)))
      {
        if (structural_field_index(receiver, name).has_value() || receiver->properties.contains(name))
        {
          structural_write_member(receiver, name, value);
          return;
        }
      }
      assign_global_binding(context, name, value);
    }

    [[nodiscard]] auto makeReference(Expression *expr) -> RuntimeRef<NGReference>
    {
      if (auto *idExpr = dynamic_cast<IdExpression *>(expr))
      {
        auto name = idExpr->id;
        if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
        {
          return makert<NGReference>(slot, name);
        }
        if (auto receiver = std::dynamic_pointer_cast<NGStructuralObject>(find_frame_receiver(activeFrames, activeScopes)))
        {
          if (structural_field_index(receiver, name).has_value() || receiver->properties.contains(name))
          {
            return makert<NGReference>(
                [receiver, name]() {
                  auto value = structural_read_member(receiver, name);
                  ensure_usable_value(value);
                  return value;
                },
                [receiver, name](const RuntimeRef<NGObject> &value) { structural_write_member(receiver, name, value); }, name);
          }
        }
        if (auto slot = context ? context->get_slot(name) : nullptr)
        {
          return makert<NGReference>(slot, name);
        }
        return makert<NGReference>([ctx = context, frames = activeFrames, scopes = activeScopes, name]() {
                                       if (!frames || frames->empty())
                                       {
                                         auto value = ctx->get(name);
                                         ensure_usable_value(value);
                                         return value;
                                       }
                                       if (auto slot = find_frame_binding_slot(frames, scopes, name))
                                        {
                                          auto value = slot->boxedValue;
                                          ensure_usable_value(value);
                                         return value;
                                       }
                                       if (auto receiver =
                                              std::dynamic_pointer_cast<NGStructuralObject>(find_frame_receiver(frames, scopes)))
                                       {
                                         if (auto value = structural_read_member(receiver, name))
                                         {
                                          ensure_usable_value(value);
                                          return value;
                                        }
                                      }
                                      auto value = lookup_global_binding(ctx, name);
                                      ensure_usable_value(value);
                                      return value;
                                    },
                                     [ctx = context, frames = activeFrames, scopes = activeScopes, name](const RuntimeRef<NGObject> &value) {
                                       if (!frames || frames->empty())
                                       {
                                         ctx->set(name, value);
                                         return;
                                       }
                                       if (auto slot = find_frame_binding_slot(frames, scopes, name))
                                        {
                                          sync_binding_slot(ctx, frames, scopes, slot, value);
                                          return;
                                        }
                                       if (auto receiver =
                                               std::dynamic_pointer_cast<NGStructuralObject>(find_frame_receiver(frames, scopes)))
                                       {
                                         if (structural_field_index(receiver, name).has_value() ||
                                             receiver->properties.contains(name))
                                        {
                                          structural_write_member(receiver, name, value);
                                          return;
                                        }
                                      }
                                      assign_global_binding(ctx, name, value);
                                    },
                                    name);
      }
      if (auto *unaryExpr = dynamic_cast<UnaryExpression *>(expr);
          unaryExpr && unaryExpr->optr && unaryExpr->optr->type == TokenType::TIMES)
      {
        ExpressionVisitor refVisitor{context, activeFrames, activeScopes};
        unaryExpr->operand->accept(&refVisitor);
        auto reference = std::dynamic_pointer_cast<NGReference>(refVisitor.object);
        if (!reference)
        {
          throw RuntimeException("Cannot take reference of non-reference dereference");
        }
        return reference;
      }
      if (auto *idAcc = dynamic_cast<IdAccessorExpression *>(expr))
      {
        ExpressionVisitor mainVisitor{context, activeFrames, activeScopes};
        idAcc->primaryExpression->accept(&mainVisitor);
        auto main = auto_deref_value(mainVisitor.object);
        ensure_usable_value(main);
        auto memberName = idAcc->accessor->repr();
        if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(main))
        {
          if (auto index = structural_field_index(structural, memberName))
          {
            if (auto slot = structural->field_slot(*index))
            {
              return makert<NGReference>(slot, memberName);
            }
          }
          return makert<NGReference>(
              [structural, memberName]() {
                auto value = structural_read_member(structural, memberName);
                ensure_usable_value(value);
                return value;
              },
               [structural, memberName](const RuntimeRef<NGObject> &value) { structural_write_member(structural, memberName, value); },
               memberName);
        }
        if (auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(main))
        {
          if (auto index = tagged_payload_index(*tagged, memberName))
          {
            if (auto slot = tagged->payload_slot(*index))
            {
              return makert<NGReference>(slot, memberName);
            }
          }
          return makert<NGReference>(
              [tagged, memberName]() {
                auto value = tagged_read_member(*tagged, memberName);
                ensure_usable_value(value);
                return value;
              },
              [tagged, memberName](const RuntimeRef<NGObject> &value) { tagged_write_member(*tagged, memberName, value); },
              memberName);
        }
        if (auto tuple = std::dynamic_pointer_cast<NGTuple>(main))
        {
          auto index = static_cast<size_t>(std::stoi(memberName));
          if (auto slot = tuple->element_slot(index))
          {
            return makert<NGReference>(slot, memberName);
          }
          return makert<NGReference>(
              [tuple, index]() {
                auto value = tuple_read_element(*tuple, index);
                ensure_usable_value(value);
                return value;
              },
              [tuple, index](const RuntimeRef<NGObject> &value) { tuple_write_element(*tuple, index, value); }, memberName);
        }
        throw RuntimeException("Unsupported reference target: " + idAcc->repr());
      }
      if (auto *indexExpr = dynamic_cast<IndexAccessorExpression *>(expr))
      {
        ExpressionVisitor mainVisitor{context, activeFrames, activeScopes};
        indexExpr->primary->accept(&mainVisitor);
        auto primary = auto_deref_value(mainVisitor.object);
        ensure_usable_value(primary);

        ExpressionVisitor indexVisitor{context, activeFrames, activeScopes};
        indexExpr->accessor->accept(&indexVisitor);
        auto indexObject = materialize_value(indexVisitor.object, indexVisitor.moved);

        auto numeral = std::dynamic_pointer_cast<NumeralBase>(indexObject);
        if (!numeral)
        {
          throw RuntimeException("Index reference requires numeral index");
        }
        auto index = NGIntegral<size_t>(numeral.get()).value;

        if (auto array = std::dynamic_pointer_cast<NGArray>(primary))
        {
          if (auto slot = array->element_slot(index))
          {
            return makert<NGReference>(slot, indexExpr->repr());
          }
          return makert<NGReference>(
              [array, index]() {
                auto value = array_read_element(*array, index);
                ensure_usable_value(value);
                return value;
              },
              [array, index](const RuntimeRef<NGObject> &value) { array_write_element(*array, index, value); },
              indexExpr->repr());
        }
        if (auto tuple = std::dynamic_pointer_cast<NGTuple>(primary))
        {
          if (auto slot = tuple->element_slot(index))
          {
            return makert<NGReference>(slot, indexExpr->repr());
          }
          return makert<NGReference>(
              [tuple, index]() {
                auto value = tuple_read_element(*tuple, index);
                ensure_usable_value(value);
                return value;
              },
              [tuple, index](const RuntimeRef<NGObject> &value) { tuple_write_element(*tuple, index, value); },
              indexExpr->repr());
        }
        throw RuntimeException("Unsupported indexed reference target: " + indexExpr->repr());
      }
      throw RuntimeException("Unsupported reference target: " + expr->repr());
    }

#pragma region Visit numeral literals

    void visit(IntegralValue<int8_t> *intVal) override
    {
      moved = false;
      object = makert<NGIntegral<int8_t>>(intVal->value);
    }
    void visit(IntegralValue<uint8_t> *intVal) override
    {
      moved = false;
      object = std::make_shared<NGIntegral<uint8_t>>(intVal->value);
    }
    void visit(IntegralValue<int16_t> *intVal) override
    {
      moved = false;
      object = std::make_shared<NGIntegral<int16_t>>(intVal->value);
    }
    void visit(IntegralValue<uint16_t> *intVal) override
    {
      moved = false;
      object = std::make_shared<NGIntegral<uint16_t>>(intVal->value);
    }
    void visit(IntegralValue<int32_t> *intVal) override
    {
      moved = false;
      object = std::make_shared<NGIntegral<int32_t>>(intVal->value);
    }
    void visit(IntegralValue<uint32_t> *intVal) override
    {
      moved = false;
      object = std::make_shared<NGIntegral<uint32_t>>(intVal->value);
    }
    void visit(IntegralValue<int64_t> *intVal) override
    {
      moved = false;
      object = std::make_shared<NGIntegral<int64_t>>(intVal->value);
    }
    void visit(IntegralValue<uint64_t> *intVal) override
    {
      moved = false;
      object = std::make_shared<NGIntegral<uint64_t>>(intVal->value);
    }

    // void visit(FloatingPointValue<float16_t> *floatVal) override
    // {
    //     object = std::make_shared<FloatingPointValue<float16_t>>(floatVal->value);
    // }

    void visit(FloatingPointValue<float /*float32_t*/> *floatVal) override
    {
      moved = false;
      object = std::make_shared<NGFloatingPoint<float /* float32_t */>>(floatVal->value);
    }

    void visit(FloatingPointValue<double /*float64_t*/> *floatVal) override
    {
      moved = false;
      object = std::make_shared<NGFloatingPoint<double /* float64_t */>>(floatVal->value);
    }

    // void visit(FloatingPointValue<float128_t> *floatVal) override
    // {
    //     object = std::make_shared<FloatingPointValue<float128_t>>(floatVal->value);
    // }

#pragma endregion

    void visit(StringValue *strVal) override
    {
      moved = false;
      object = makert<NGString>(strVal->value);
    }

    void visit(BooleanValue *boolVal) override
    {
      moved = false;
      object = makert<NGBoolean>(boolVal->value);
    }

    void visit(TupleLiteral *tuple) override
    {
      Vec<RuntimeRef<NGObject>> objects;

      ExpressionVisitor vis{context, activeFrames, activeScopes};

      for (const auto &element : tuple->elements)
      {
        element->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(element); spread)
        {
          auto collection = vis.collection;
          for (auto &&item : *collection)
          {
            objects.push_back(item);
          }
        }
        else
        {
          objects.push_back(materialize_value(vis.object, vis.moved));
        }
      }

      moved = false;
      object = makert<NGTuple>(objects);
    }

    void visit(FunCallExpression *funCallExpr) override
    {
      FunctionPathVisitor fpVis{};
      funCallExpr->primaryExpression->accept(&fpVis);
      NGArgs callArgs;

      for (auto &param : funCallExpr->arguments)
      {
        ExpressionVisitor vis{context, activeFrames, activeScopes};
        param->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(param))
        {
          auto collection = vis.collection;
          for (auto &&item : *collection)
          {
            callArgs.push_back(item);
          }
        }
        else
        {
          callArgs.push_back(materialize_value(vis.object, vis.moved));
        }
      }

      RuntimeRef<NGObject> dummy = makert<NGObject>();

      if (!context->has_function(fpVis.path, true))
      {
        throw RuntimeException("No such function: " + fpVis.path, funCallExpr->pos);
      }

      moved = false;
      this->object = context->get_function(fpVis.path)(dummy, context, callArgs);
    }

    void visit(UnaryExpression *unoExpr) override
    {
      switch (unoExpr->optr->type)
      {
      case TokenType::MINUS:
      {
        ExpressionVisitor operandVisitor{context, activeFrames, activeScopes};
        unoExpr->operand->accept(&operandVisitor);
        auto result = operandVisitor.object;
        auto numeric = dynamic_cast<NumeralBase *>(result.get());
        if (numeric)
        {
          moved = false;
          this->object = numeric->opNegate();
          return;
        }
        throw RuntimeException("Cannot negate a non-number");
      }
      case TokenType::NOT:
      {
        ExpressionVisitor operandVisitor{context, activeFrames, activeScopes};
        unoExpr->operand->accept(&operandVisitor);
        moved = false;
        this->object = NGObject::boolean(!runtime_value_bool(operandVisitor.object));
        return;
      }
      case TokenType::KEYWORD_REF:
      case TokenType::AMPERSAND:
        moved = false;
        object = makeReference(unoExpr->operand.get());
        return;
      case TokenType::TIMES:
      {
        ExpressionVisitor operandVisitor{context, activeFrames, activeScopes};
        unoExpr->operand->accept(&operandVisitor);
        auto reference = std::dynamic_pointer_cast<NGReference>(operandVisitor.object);
        if (!reference)
        {
          throw RuntimeException("Cannot dereference non-reference value");
        }
        moved = false;
        object = reference->read();
        return;
      }
      case TokenType::KEYWORD_MOVE:
      {
        if (auto idExpr = dynamic_ast_cast<IdExpression>(unoExpr->operand))
        {
          auto value = lookup_binding(idExpr->id);
          ensure_usable_value(value);
          write_binding(idExpr->id, moved_object());
          moved = true;
          object = value;
          return;
        }
        if (auto deref = dynamic_ast_cast<UnaryExpression>(unoExpr->operand);
            deref && deref->optr && deref->optr->type == TokenType::TIMES)
        {
          ExpressionVisitor operandVisitor{context, activeFrames, activeScopes};
          deref->operand->accept(&operandVisitor);
          auto reference = std::dynamic_pointer_cast<NGReference>(operandVisitor.object);
          if (!reference)
          {
            throw RuntimeException("Cannot move from non-reference dereference");
          }
          auto value = reference->read();
          reference->write(moved_object());
          moved = true;
          object = value;
          return;
        }
        ExpressionVisitor operandVisitor{context, activeFrames, activeScopes};
        unoExpr->operand->accept(&operandVisitor);
        moved = operandVisitor.moved;
        object = operandVisitor.object;
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
      ExpressionVisitor leftVisitor{context, activeFrames, activeScopes};
      ExpressionVisitor rightVisitor{context, activeFrames, activeScopes};
      binExpr->left->accept(&leftVisitor);
      binExpr->right->accept(&rightVisitor);

      moved = false;
      object = evaluateExpr(binExpr->optr->type, leftVisitor.object, rightVisitor.object);
    }

    void visit(IdExpression *idExpr) override
    {
      moved = false;
      object = lookup_binding(idExpr->id);
      ensure_usable_value(object);
    }

    void visit(ArrayLiteral *array) override
    {
      Vec<RuntimeRef<NGObject>> objects;

      ExpressionVisitor vis{context, activeFrames, activeScopes};

      for (const auto &element : array->elements)
      {
        element->accept(&vis);
        if (auto spread = dynamic_ast_cast<SpreadExpression>(element); spread)
        {
          auto collection = vis.collection;
          for (auto &&item : *collection)
          {
            objects.push_back(item);
          }
        }
        else
        {
          objects.push_back(materialize_value(vis.object, vis.moved));
        }
      }

      moved = false;
      object = makert<NGArray>(objects);
    }

    void visit(IndexAccessorExpression *index) override
    {
      ExpressionVisitor vis{context, activeFrames, activeScopes};

      index->primary->accept(&vis);

      RuntimeRef<NGObject> primaryObject = vis.object;
      primaryObject = auto_deref_value(primaryObject);
      ensure_usable_value(primaryObject);

      index->accessor->accept(&vis);

      RuntimeRef<NGObject> accessorObject = vis.object;

      moved = false;
      object = primaryObject->opIndex(accessorObject);
    }

    void visit(IndexAssignmentExpression *index) override
    {
      ExpressionVisitor vis{context, activeFrames, activeScopes};

      index->primary->accept(&vis);

      RuntimeRef<NGObject> primaryObject = vis.object;
      primaryObject = auto_deref_value(primaryObject);
      ensure_usable_value(primaryObject);

      index->accessor->accept(&vis);

      RuntimeRef<NGObject> accessorObject = vis.object;

      index->value->accept(&vis);

      RuntimeRef<NGObject> valueObject = materialize_value(vis.object, vis.moved);

      moved = false;
      object = primaryObject->opIndex(accessorObject, valueObject);
    }

    void visit(IdAccessorExpression *idAccExpr) override
    {
      const Str &repr = idAccExpr->accessor->repr();

      ExpressionVisitor vis{context, activeFrames, activeScopes};

      idAccExpr->primaryExpression->accept(&vis);

      RuntimeRef<NGObject> main = auto_deref_value(vis.object);
      ensure_usable_value(main);

      NGArgs callArgs;
      for (const auto &argument : idAccExpr->arguments)
      {
        argument->accept(&vis);
        callArgs.push_back(materialize_value(vis.object, vis.moved));
      }

      moved = false;
      object = runtime_value_respond(main, repr, context, callArgs);
    }

    void visit(NewObjectExpression *newObj) override
    {
      Str typeName = newObj->targetType ? newObj->targetType->repr() : newObj->typeName;
      if (auto ngType = resolveRuntimeType(context, typeName))
      {
        auto structural = makert<NGStructuralObject>();

        structural->customizedType = ngType;
        structural->replace_payload_fields(Vec<RuntimeRef<NGObject>>(ngType->properties.size(), makert<NGUnit>()));

        RuntimeRef<NGContext> newContext = context->fork();
        auto objectScopes = make_scope_chain();
        ExpressionVisitor visitor{newContext, activeFrames, objectScopes};

        for (auto &&[name, expr] : newObj->properties)
        {
          expr->accept(&visitor);
          RuntimeRef<NGObject> result = materialize_value(visitor.object, visitor.moved);

          define_scope_binding(visitor.context, activeFrames, objectScopes, name, result);

          structural_write_member(structural, name, result);
        }

        moved = false;
        object = allocate_heap_object(structural, "heap:" + typeName);
        return;
      }

      auto variantType = resolveRuntimeVariantType(context, typeName);
      if (!variantType)
      {
        throw RuntimeException("Unknown type for object: " + typeName);
      }

      Vec<RuntimeRef<NGObject>> payload;
      payload.reserve(variantType->properties.size());

      for (const auto &property : variantType->properties)
      {
        auto it = newObj->properties.find(property);
        if (it == newObj->properties.end())
        {
          throw RuntimeException("Missing payload property '" + property + "' for variant " + typeName);
        }
        ExpressionVisitor visitor{context, activeFrames, activeScopes};
        it->second->accept(&visitor);
        payload.push_back(materialize_value(visitor.object, visitor.moved));
      }

      moved = false;
      object = allocate_heap_object(
          makert<NGTaggedValue>(variantType->name, variantType->variantName, variantType->variantIndex, std::move(payload),
                                variantType->properties),
          "heap:" + typeName);
    }

    void visit(AssignmentExpression *assignmentExpr) override
    {
      ExpressionVisitor vis{context, activeFrames, activeScopes};

      assignmentExpr->value->accept(&vis);
      auto result = materialize_value(vis.object, vis.moved);
      if (auto idexpr = dynamic_ast_cast<IdExpression>(assignmentExpr->target); idexpr)
      {
        write_binding(idexpr->id, result);
        object = result;
      }
      else if (auto deref = dynamic_ast_cast<UnaryExpression>(assignmentExpr->target);
               deref && deref->optr && deref->optr->type == TokenType::TIMES)
      {
        ExpressionVisitor targetVisitor{context, activeFrames, activeScopes};
        deref->operand->accept(&targetVisitor);
        auto reference = std::dynamic_pointer_cast<NGReference>(targetVisitor.object);
        if (!reference)
        {
          throw RuntimeException("Left-hand side of dereference assignment is not a reference");
        }
        reference->write(result);
        object = result;
      }
      else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(assignmentExpr->target); idAcc)
      {
        idAcc->primaryExpression->accept(&vis);
        auto target = auto_deref_value(vis.object);
        if (auto obj = std::dynamic_pointer_cast<NGStructuralObject>(target); obj)
        {
          structural_write_member(obj, idAcc->accessor->repr(), result);
          object = result;
          return;
        }
        if (auto tup = std::dynamic_pointer_cast<NGTuple>(target); tup)
        {
          tup->opIndex(makert<NGIntegral<int>>(std::stoi(idAcc->accessor->repr())), result);
          object = result;
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
      moved = false;
      ExpressionVisitor vis{context, activeFrames, activeScopes};
      typeCheckExpr->value->accept(&vis);
      RuntimeRef<NGObject> value = auto_deref_value(vis.object);

      if (auto anno = dynamic_ast_cast<TypeAnnotation>(typeCheckExpr->type); anno)
      {
        auto name = anno->repr();
        auto targetType = resolveRuntimeType(context, name);
        if (targetType)
        {
          auto valueType = runtime_value_type(value);
          bool matches = *valueType == *targetType;
          if (!matches)
          {
            matches = stripGenericTypeSuffix(valueType->name) == stripGenericTypeSuffix(name);
          }
          this->object = makert<NGBoolean>(matches);
        }
        else
        {
          // todo: simply fix this, and will migrate to typechecker later.
          this->object = makert<NGBoolean>((runtime_value_type(value)->name == anno->name));
        }
      }
      else if (auto idAcccessor = dynamic_ast_cast<IdAccessorExpression>(typeCheckExpr->type); idAcccessor)
      {
        idAcccessor->primaryExpression->accept(&vis);
        auto name = idAcccessor->accessor->repr();
        auto result = vis.object;
        auto mod = std::dynamic_pointer_cast<NGModule>(result);
        if (!mod)
        {
          throw RuntimeException("Invalid module to locate type: " + name);
        }
        auto typeIt = mod->types.find(name);
        if (typeIt == mod->types.end() || !typeIt->second)
        {
          throw RuntimeException("Invalid type name, cannot find: " + name);
        }
        auto targetType = typeIt->second;
        this->object = makert<NGBoolean>(*(runtime_value_type(value)) == *(targetType));
      }
      else
      {
        throw RuntimeException("Invalid target expression for type checking: " + typeCheckExpr->type->repr());
      }
    }

    void visit(TypeOfExpression * /*typeOfExpr*/) override
    {
      throw RuntimeException("typeof(expr) is only supported in compile-time type queries");
    }
    void visit(SpreadExpression *spreadExpression) override
    {
      moved = false;
      ExpressionVisitor vis{context, activeFrames, activeScopes};

      spreadExpression->expression->accept(&vis);
      auto result = vis.object;

      if (auto tup = std::dynamic_pointer_cast<NGTuple>(result); tup)
      {
        collection = makert<Vec<RuntimeRef<NGObject>>>(tup->payload_items());
        object = result;
        return;
      }
      if (auto arr = std::dynamic_pointer_cast<NGArray>(result); arr)
      {
        collection = makert<Vec<RuntimeRef<NGObject>>>(arr->payload_items());
        object = result;
        return;
      }
      throw RuntimeException("Invalid spread expression, expect array or tuple, but got: " +
                             spreadExpression->expression->repr());
    }

    void visit(UnitLiteral *unit) override
    {
      moved = false;
      object = makert<NGUnit>();
    }

    void visit(CastExpression *castExpr) override
    {
      moved = false;
      ExpressionVisitor exprVis{context, activeFrames, activeScopes};
      castExpr->expression->accept(&exprVis);
      RuntimeRef<NGObject> value = exprVis.object;

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
      if (auto targetType = resolveRuntimeType(context, targetTypeName))
      {
        // If the value is already an NGNewType with the same type, unwrap it
        if (auto newTypeVal = std::dynamic_pointer_cast<NGNewType>(value); newTypeVal)
        {
          if (newTypeVal->newType->name == targetTypeName)
          {
            // Same newtype — no-op
            object = value;
            return;
          }
          // Unwrap then potentially rewrap
          value = newTypeVal->wrapped;
        }

        // Wrap into newtype
        auto newType = makert<NGType>();
        newType->name = targetTypeName;
        object = makert<NGNewType>(newType, value);
      }
      else
      {
        // For primitive casts or unknown types, just pass through
        object = value;
      }
    }
  };

  struct StatementVisitor : public DummyVisitor
  {
    RuntimeRef<NGContext> context;
    RuntimeRef<StorageCell> returnSlot;
    RuntimeRef<Vec<CallFrame>> activeFrames;
    RuntimeRef<Vec<uint64_t>> activeScopes;

    explicit StatementVisitor(RuntimeRef<NGContext> context, RuntimeRef<StorageCell> returnSlot = nullptr,
                              RuntimeRef<Vec<CallFrame>> activeFrames = nullptr,
                              RuntimeRef<Vec<uint64_t>> activeScopes = nullptr)
        : context(std::move(context)), returnSlot(std::move(returnSlot)), activeFrames(std::move(activeFrames)),
          activeScopes(std::move(activeScopes))
    {
    }

    void capture_binding(const Str &name, const RuntimeRef<NGObject> &value, bool defineNew)
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
        auto slot = make_named_boxed_storage_cell(name, value);
        slot->ownerContext = context.get();
        slot->ownerScopeId = current_scope_id(activeScopes);
        frame.locals.push_back(slot);
        return;
      }

      if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
      {
        sync_binding_slot(context, activeFrames, activeScopes, slot, value);
      }
    }

    void define_binding(const Str &name, const RuntimeRef<NGObject> &value, bool moved = false)
    {
      auto materialized = materialize_value(value, moved);
      if (!activeFrames || activeFrames->empty())
      {
        context->define(name, materialized);
      }
      capture_binding(name, materialized, true);
      if (is_module_frame(activeFrames) && context->parent_context() == nullptr &&
          current_scope_id(activeScopes) == root_scope_id(activeScopes))
      {
        publish_global_binding(context, name, materialized);
      }
    }

    void assign_binding(const Str &name, const RuntimeRef<NGObject> &value, bool moved = false)
    {
      auto materialized = materialize_value(value, moved);
      if (activeFrames && !activeFrames->empty())
      {
        if (auto slot = find_frame_binding_slot(activeFrames, activeScopes, name))
        {
          sync_binding_slot(context, activeFrames, activeScopes, slot, materialized);
          return;
        }
        if (auto receiver = std::dynamic_pointer_cast<NGStructuralObject>(find_frame_receiver(activeFrames, activeScopes)))
        {
          if (structural_field_index(receiver, name).has_value() || receiver->properties.contains(name))
          {
            structural_write_member(receiver, name, materialized);
            return;
          }
        }
      }
      assign_global_binding(context, name, materialized);
    }

    void visit(ReturnStatement *returnStatement) override
    {
      RuntimeRef<NGObject> result;
      if (returnStatement->expression)
      {
        ExpressionVisitor vis{context, activeFrames, activeScopes};
        returnStatement->expression->accept(&vis);
        result = materialize_value(vis.object, vis.moved);
      }
      else
      {
        result = makert<NGUnit>();
      }
      sync_storage_cell(returnSlot, result);
    }

    void visit(IfStatement *ifStmt) override
    {
      StatementVisitor stmtVis{context, returnSlot, activeFrames, activeScopes};
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

      ExpressionVisitor vis{context, activeFrames, activeScopes};
      ifStmt->testing->accept(&vis);
      if (runtime_value_bool(vis.object))
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
      StatementVisitor vis{context, returnSlot, activeFrames, blockScopes};
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
      ExpressionVisitor vis{context, activeFrames, activeScopes};
      valDef->value->accept(&vis);

      define_binding(valDef->name, vis.object, vis.moved);
    }

    void visit(ValueBindingStatement *valBind) override
    {
      ExpressionVisitor vis{context, activeFrames, activeScopes};
      valBind->value->accept(&vis);
      auto result = vis.object;

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
        auto tuple = std::dynamic_pointer_cast<NGTuple>(result);
        if (!tuple)
        {
          throw RuntimeException("Tuple unpacking requires a tuple value");
        }
        auto items = tuple->payload_items();
        for (auto &&binding : valBind->bindings)
        {
          if (!binding->spreadReceiver)
          {
            define_binding(binding->name, items.at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
            Vec<RuntimeRef<NGObject>> values{items.begin() + binding->index, items.end()};
            define_binding(binding->name, makert<NGTuple>(values), true);
          }
        }
      }
      break;

      case BindingType::ARRAY_UNPACK:
      {
        auto array = std::dynamic_pointer_cast<NGArray>(result);
        if (!array)
        {
          throw RuntimeException("Array unpacking requires an array value");
        }
        auto items = array->payload_items();
        for (auto &&binding : valBind->bindings)
        {
          if (!binding->spreadReceiver)
          {
            define_binding(binding->name, items.at(binding->index));
          }
          else if (!binding->name.empty()) // empty spread receiver just ignores everything
          {
            Vec<RuntimeRef<NGObject>> values{items.begin() + binding->index, items.end()};
            define_binding(binding->name, makert<NGArray>(values), true);
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
      ExpressionVisitor vis{context, activeFrames, activeScopes};
      simpleStmt->expression->accept(&vis);
    }

    void visit(SwitchStatement *switchStmt) override
    {
      ExpressionVisitor vis{context, activeFrames, activeScopes};
      switchStmt->scrutinee->accept(&vis);
      auto scrutinee = vis.object;

      auto *tagged = dynamic_cast<NGTaggedValue *>(scrutinee.get());
      if (!tagged)
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
        if (c.variantName == tagged->variantName)
        {
          auto caseScopes = fork_scope_chain(activeScopes);
          StatementVisitor caseVis{context, returnSlot, activeFrames, caseScopes};
          auto payloadValues = tagged->payload_items();
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
        StatementVisitor caseVis{context, returnSlot, activeFrames, caseScopes};
        otherwise->body->accept(&caseVis);
        return;
      }

      throw RuntimeException("No matching case for variant: " + tagged->variantName);
    }

    void visit(LoopStatement *loopStatement) override
    {
      auto loopScopes = fork_scope_chain(activeScopes);
      ExpressionVisitor vis{context, activeFrames, loopScopes};
      for (auto &&binding : loopStatement->bindings)
      {
        binding.target->accept(&vis);
        switch (binding.type)
        {
        case LoopBindingType::LOOP_ASSIGN:
          define_binding(binding.name, vis.object, vis.moved);
          break;
        default:
          throw RuntimeException("Unsupported loop binding");
        }
      }

      StatementVisitor stmtVis{context, returnSlot, activeFrames, loopScopes};
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
          for (auto &&object : iter.slotValues)
          {
            assign_binding(loopStatement->bindings[i].name, object, true);
            i++;
          }
        }
      }
    }

    void visit(NextStatement *nextStatement) override
    {
      ExpressionVisitor vis{context, activeFrames, activeScopes};
      try
      {
        Vec<RuntimeRef<NGObject>> slotValues{};
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
              slotValues.push_back(materialize_value(vis.object, vis.moved));
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
    RuntimeRef<NGContext> context;
    size_t gcRootProviderId = 0;

    Vec<Str> modulePaths;
    RuntimeRef<Vec<CallFrame>> activeFrames = makert<Vec<CallFrame>>();

    explicit Stupid(Vec<Str> modulePaths, bool loadingPrelude = false)
        : context(makert<NGContext>()), modulePaths(modulePaths)
    {
      gcRootProviderId = register_gc_root_provider([frames = activeFrames, ctx = context]() {
        auto roots = enumerate_context_roots(ctx);
        auto frameRoots = enumerate_call_frame_roots(*frames);
        roots.insert(roots.end(), frameRoots.begin(), frameRoots.end());
        return roots;
      });
      if (!loadingPrelude)
      {
        loadPrelude();
      }
    }

    RuntimeRef<NGModule> asModule() { return makert<NGModule>(context); }

    void loadPrelude()
    {
      if (context->has_module("prelude"))
      {
        return;
      }
      if (auto target = get_module_registry().queryModuleById("std.prelude"); target)
      {
        auto targetModule = target->runtimeModule;
        context->define_module(target->moduleName, target->runtimeModule);
        importInto(context, {"*"}, targetModule);
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
      context->symbol_table()->exports = mod->exports;
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
      StatementVisitor vis{context, nullptr, activeFrames, moduleScopes};

      for (const auto &stmt : mod->statements)
      {
        stmt->accept(&vis);
      }
      materialize_root_frame_bindings(context, activeFrames->back(), root_scope_id(moduleScopes));
    }

    void visit(ImportDecl *importDecl) override
    {
      if (!context->has_module(importDecl->module))
      {
        // load module
        NG::module::FileBasedExternalModuleLoader loader{this->modulePaths};
        auto &&moduleInfo = loader.load(importDecl->modulePath);
        if (auto target = get_module_registry().queryModuleById(moduleInfo->moduleId); target)
        {
          context->define_module(importDecl->module, target->runtimeModule);
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
          context->define_module(importDecl->module, moduleInfo->runtimeModule);
        }
      }
      RuntimeRef<NGModule> targetModule = context->get_module(importDecl->module);

      importInto(context, importDecl->imports, targetModule);

      if (!importDecl->alias.empty())
      {
        context->define(importDecl->alias, targetModule);
      }
    }

    static void importInto(RuntimeRef<NGContext> context, Vec<Str> declaredImports,
                           const RuntimeRef<NGModule> &fromModule)
    {
      Set<Str> imports = resolveImports(declaredImports, fromModule);
      std::copy(imports.begin(), imports.end(), std::back_inserter(context->symbol_table()->imported));

      for (auto &&imp : imports)
      {
        if (fromModule->functions.contains(imp))
        {
          context->define_function(imp, fromModule->functions[imp]);
        }
        if (fromModule->types.contains(imp))
        {
          context->define_type(imp, fromModule->types[imp]);
        }
        if (fromModule->objects.contains(imp))
        {
          context->define(imp, fromModule->objects[imp]);
        }
        if (fromModule->native_functions.contains(imp))
        {
          context->define_function(imp, fromModule->native_functions[imp]);
        }
      }
    }

    static auto resolveImports(const Vec<Str> &imports, const RuntimeRef<NGModule> &targetModule) -> Set<Str>
    {
      bool importAll = (std::ranges::find(imports, "*") != end(imports));

      bool exportsAll =
          (std::find(begin(targetModule->exports), end(targetModule->exports), "*") != end(targetModule->exports));
      Set<Str> exported{};
      if (exportsAll)
      {
        for (auto &&[fnName, _ignored] : targetModule->functions)
        {
          if (!targetModule->imports.contains(fnName) || targetModule->exports.contains(fnName))
          {
            exported.insert(fnName);
          }
        }
        for (auto &&[typeName, _ignored] : targetModule->types)
        {
          if (!targetModule->imports.contains(typeName) || targetModule->exports.contains(typeName))
          {
            exported.insert(typeName);
          }
        }
        for (auto &&[objName, _ignored] : targetModule->objects)
        {
          if (!targetModule->imports.contains(objName) || targetModule->exports.contains(objName))
          {
            exported.insert(objName);
          }
        }
        for (auto &&[fnName, _ignored] : targetModule->native_functions)
        {
          if (!targetModule->imports.contains(fnName) || targetModule->exports.contains(fnName))
          {
            exported.insert(fnName);
          }
        }
      }
      else
      {
        exported.insert(begin(targetModule->exports), end(targetModule->exports));
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
          [funDef, frames = activeFrames](const NGSelf &dummy, const NGCtx &ngContext,
                                          const NGArgs &args) -> RuntimeRef<NGObject>
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
        RuntimeRef<NGContext> newContext = ngContext->fork();
        auto scopeIds = make_scope_chain();
        CallFrame callFrame{};
        callFrame.functionName = funDef->funName;
        callFrame.receiver = make_named_boxed_storage_cell("self", dummy);
        callFrame.receiver->ownerContext = newContext.get();
        callFrame.receiver->ownerScopeId = current_scope_id(scopeIds);
        auto returnTypeName = funDef->returnType ? funDef->returnType->repr() : "unit";
        auto returnRuntimeType = resolveRuntimeType(newContext, returnTypeName);
        callFrame.returnSlot =
            make_storage_cell(TypeLayout{.name = returnTypeName}, StorageClass::FRAME, nullptr, "ret", returnRuntimeType);
        callFrame.returnSlot->ownerContext = newContext.get();
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
            Vec<RuntimeRef<NGObject>> packItems;
            for (size_t j = i; j < args.size(); ++j)
            {
              packItems.push_back(args[j]);
            }
            auto packed = makert<NGTuple>(packItems);
            auto slot = make_named_boxed_storage_cell(funDef->params[i]->paramName, packed);
            slot->ownerContext = newContext.get();
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
            break; // pack parameter is always the last one
          }
          else if (args.size() > i)
          {
            auto slot = make_named_boxed_storage_cell(funDef->params[i]->paramName, args[i]);
            slot->ownerContext = newContext.get();
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
          }
          else if (funDef->params[i]->value != nullptr)
          {
            ExpressionVisitor vis{newContext, frames, scopeIds};
            funDef->params[i]->value->accept(&vis);
            auto materialized = materialize_value(vis.object, vis.moved);
            auto slot = make_named_boxed_storage_cell(funDef->params[i]->paramName, materialized);
            slot->ownerContext = newContext.get();
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
          runtime_sync_storage_cell(frames->back().returnSlot, nullptr);
          try
          {
          StatementVisitor vis{newContext, frames->back().returnSlot, frames, scopeIds};
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
                  Vec<RuntimeRef<NGObject>> packItems;
                  for (size_t j = i; j < nextIter.slotValues.size(); ++j)
                  {
                    packItems.push_back(nextIter.slotValues[j]);
                   }
                   auto packed = makert<NGTuple>(packItems);
                   sync_storage_cell(frames->back().params[i], packed);
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
        auto result = frames->back().returnSlot->boxedValue ? frames->back().returnSlot->boxedValue : makert<NGUnit>();
        sync_storage_cell(frames->back().returnSlot, result);
        return result;
      };

      context->define_function(funDef->funName, functionInvoker);
    }

    void visit(Statement *stmt) override
    {
      StatementVisitor vis{context, nullptr, activeFrames, make_scope_chain()};
      stmt->accept(&vis);
    }

    void visit(ValDef *valDef) override
    {
      StatementVisitor vis{context, nullptr, activeFrames, make_scope_chain()};

      valDef->body->accept(&vis);
    }

    void visit(TypeDef *typeDef) override
    {
      auto type = makert<NGType>();

      type->name = typeDef->typeName;
      type->layout = build_inline_type_layout(context, typeDef->typeName, typeDef->properties)
                         .value_or(TypeLayout{.name = typeDef->typeName, .kind = LayoutKind::DYNAMIC});

      for (const auto &property : typeDef->properties)
      {
        type->properties.push_back(property->propertyName);
        type->layout.fields.push_back(FieldLayout{.name = property->propertyName});
      }

      for (const auto &memFn : typeDef->memberFunctions)
      {
        type->memberFunctions[memFn->funName] =
            [memFn, frames = activeFrames](const NGSelf &dummy, const NGCtx &ngContext,
                                           const NGArgs &args) -> RuntimeRef<NGObject>
        {
          RuntimeRef<NGContext> newContext = ngContext->fork();
          auto scopeIds = make_scope_chain();
          CallFrame callFrame{};
          callFrame.functionName = memFn->funName;
          callFrame.receiver = make_named_boxed_storage_cell("self", dummy);
          callFrame.receiver->ownerContext = newContext.get();
          callFrame.receiver->ownerScopeId = current_scope_id(scopeIds);
          auto returnTypeName = memFn->returnType ? memFn->returnType->repr() : "unit";
          auto returnRuntimeType = resolveRuntimeType(newContext, returnTypeName);
          callFrame.returnSlot =
              make_storage_cell(TypeLayout{.name = returnTypeName}, StorageClass::FRAME, nullptr, "ret", returnRuntimeType);
          callFrame.returnSlot->ownerContext = newContext.get();
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
            RuntimeRef<NGObject> value;
            if (args.size() > i)
            {
              value = args[i];
            }
            else if (memFn->params[i]->value != nullptr)
            {
              ExpressionVisitor vis{newContext, frames, scopeIds};
              memFn->params[i]->value->accept(&vis);
              value = materialize_value(vis.object, vis.moved);
            }
            else
            {
              throw RuntimeException("Missing argument for parameter '" + memFn->params[i]->paramName +
                                     "' in member function '" + memFn->funName + "'");
            }
            auto slot = make_named_boxed_storage_cell(memFn->params[i]->paramName, value);
            slot->ownerContext = newContext.get();
            slot->ownerScopeId = current_scope_id(scopeIds);
            frames->back().params.push_back(slot);
          }

          runtime_sync_storage_cell(frames->back().returnSlot, nullptr);
          StatementVisitor vis{newContext, frames->back().returnSlot, frames, scopeIds};
          memFn->body->accept(&vis);
          auto result = frames->back().returnSlot->boxedValue ? frames->back().returnSlot->boxedValue : makert<NGUnit>();
          sync_storage_cell(frames->back().returnSlot, result);
          return result;
        };
      }

      context->define_type(type->name, type);
    }

    void visit(TypeAliasDef *typeAliasDef) override
    {
      // Type alias is transparent — just register the underlying type under the alias name
      // The type checker resolves aliases; at runtime we store the underlying type directly
      auto underlyingType = makert<NGType>();
      underlyingType->name = typeAliasDef->aliasName;
      underlyingType->layout = concrete_layout_for_annotation(context, typeAliasDef->underlyingType.get())
                                   .value_or(TypeLayout{.name = typeAliasDef->aliasName, .kind = LayoutKind::DYNAMIC});
      underlyingType->layout.name = typeAliasDef->aliasName;
      context->define_type(typeAliasDef->aliasName, underlyingType);
    }

    void visit(NewTypeDef *newTypeDef) override
    {
      // Create a new nominal type for the newtype
      auto newType = makert<NGType>();
      newType->name = newTypeDef->typeName;
      newType->layout = concrete_layout_for_annotation(context, newTypeDef->wrappedType.get())
                            .value_or(TypeLayout{.name = newTypeDef->typeName, .kind = LayoutKind::INLINE_VALUE});
      newType->layout.name = newTypeDef->typeName;
      context->define_type(newTypeDef->typeName, newType);
    }

    void visit(TaggedUnionDef *taggedUnion) override
    {
      auto type = makert<NGType>();
      type->name = taggedUnion->typeName;
      type->layout = build_tagged_union_type_layout(context, taggedUnion)
                         .value_or(TypeLayout{.name = taggedUnion->typeName, .kind = LayoutKind::TAGGED_UNION});
      context->define_type(taggedUnion->typeName, type);

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
        context->define_variant_type(variantName, variantType);

        context->define_function(variantName,
          [unionName, variantName, variantIndex, payloadNames](const NGSelf &self, const NGCtx &ctx,
                                                                const NGArgs &args) -> RuntimeRef<NGObject>
          {
            Vec<RuntimeRef<NGObject>> payload = args;
            return makert<NGTaggedValue>(unionName, variantName, variantIndex, std::move(payload), payloadNames);
          });
      }
    }

    void summary() override { context->summary(); }

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
