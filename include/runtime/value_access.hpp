#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

#include <optional>
#include <utility>

namespace NG::runtime
{
  inline auto make_runtime_boolean(bool value, StorageClass storageClass = StorageClass::TEMPORARY)
      -> RuntimeRef<StorageCell>
  {
    auto cell = make_storage_cell(boolean_runtime_type()->layout, storageClass, {}, boolean_runtime_type());
    cell->bytes.resize(1);
    cell->bytes[0] = value ? 1 : 0;
    cell->initialized = true;
    return cell;
  }

  inline auto runtime_boolean_value(const RuntimeRef<StorageCell> &cell) -> std::optional<bool>
  {
    if (!cell)
    {
      return std::nullopt;
    }
    if (cell->runtimeType && cell->runtimeType->name == "Bool" && !cell->bytes.empty())
    {
      return cell->bytes[0] != 0;
    }
    return std::nullopt;
  }

  inline auto unit_cell(StorageClass storageClass = StorageClass::TEMPORARY) -> RuntimeRef<StorageCell>
  {
    auto cell = make_storage_cell(unit_runtime_type()->layout, storageClass, {}, unit_runtime_type());
    cell->initialized = true;
    return cell;
  }

  inline auto make_runtime_native_handle_cell(Str typeName, uintptr_t address, bool owning, StorageClass storageClass)
      -> RuntimeRef<StorageCell>
  {
    auto type = makert<NGType>();
    type->name = typeName;
    type->layout = buffer_runtime::make_native_handle_layout(typeName);
    type->showCellHandler = [](const RuntimeRef<StorageCell> &cell) {
      auto it = cell ? cell->nativeHandles.find(0) : Map<size_t, NativeHandle>::const_iterator{};
      if (!cell || it == cell->nativeHandles.end())
      {
        return Str{"native(<null>)"};
      }
      return it->second.typeName + "(0x" + std::to_string(it->second.address) + ")";
    };
    type->boolCellHandler = [](const RuntimeRef<StorageCell> &cell) {
      auto it = cell ? cell->nativeHandles.find(0) : Map<size_t, NativeHandle>::const_iterator{};
      return cell && it != cell->nativeHandles.end() && it->second.address != 0;
    };

    auto cell = make_storage_cell(type->layout, storageClass, {}, type);
    cell->nativeHandles.insert_or_assign(0, NativeHandle{
                                                .typeName = std::move(typeName),
                                                .address = address,
                                                .owning = owning,
                                            });
    cell->dropArmed = owning;
    cell->initialized = true;
    return cell;
  }

  inline auto runtime_native_handle_value(const RuntimeRef<StorageCell> &cell) -> NativeHandle
  {
    if (!cell || cell->layout.kind != LayoutKind::NATIVE_HANDLE)
    {
      return {};
    }
    auto it = cell->nativeHandles.find(0);
    return it == cell->nativeHandles.end() ? NativeHandle{} : it->second;
  }

  inline auto native_handles_contain_owner(const Map<size_t, NativeHandle> &handles) -> bool
  {
    return std::ranges::any_of(handles, [](const auto &entry) { return entry.second.owning; });
  }

  inline auto clone_native_handles_as_borrowed(const Map<size_t, NativeHandle> &handles) -> Map<size_t, NativeHandle>
  {
    auto borrowed = handles;
    for (auto &[_, handle] : borrowed)
    {
      handle.owning = false;
    }
    return borrowed;
  }

  struct RuntimeModuleCellState
  {
    Map<Str, NGCallable> functions;
    Map<Str, RuntimeRef<NGType>> types;
    Map<Str, NGCallable> nativeFunctions;
    Set<Str> traitNames;
    Set<Str> imports;
    Set<Str> exports;
    Map<Str, std::shared_ptr<void>> nativeState;
  };

  inline auto runtime_module_state(const RuntimeRef<StorageCell> &value) -> RuntimeModuleCellState *
  {
    return value && value->moduleState ? value->moduleState.get() : nullptr;
  }

  inline auto make_runtime_module(const NGSymbols &symbols) -> RuntimeRef<StorageCell>
  {
    auto cell = make_storage_cell(module_runtime_type()->layout, StorageClass::GLOBAL, {}, module_runtime_type());
    cell->runtimeType = module_runtime_type();
    cell->layout = module_runtime_type()->layout;
    cell->initialized = true;

    auto state = std::make_shared<RuntimeModuleCellState>();
    if (symbols)
    {
      for (const auto &[name, slot] : symbols->objectSlots)
      {
        cell->namedRefs.insert_or_assign(name, slot);
      }
      state->functions = symbols->functions;
      state->types = symbols->types;
      state->traitNames = symbols->traitNames;
      state->exports.insert(symbols->exports.begin(), symbols->exports.end());
      state->imports.insert(symbols->imported.begin(), symbols->imported.end());
    }
    cell->moduleState = std::move(state);
    return cell;
  }

  inline auto runtime_is_module_value(const RuntimeRef<StorageCell> &value) -> bool
  {
    return value && value->runtimeType && value->runtimeType->name == "Module";
  }

  inline auto runtime_module_slots(const RuntimeRef<StorageCell> &value) -> Vec<RuntimeRef<StorageCell>>
  {
    Vec<RuntimeRef<StorageCell>> slots;
    if (!runtime_is_module_value(value))
    {
      throw RuntimeException("Expected module runtime value");
    }
    for (const auto &[name, ref] : value->namedRefs)
    {
      slots.push_back(ref);
    }
    return slots;
  }

  inline auto runtime_module_object_slots(const RuntimeRef<StorageCell> &value) -> Map<Str, RuntimeRef<StorageCell>>
  {
    if (!runtime_is_module_value(value))
    {
      throw RuntimeException("Expected module runtime value");
    }
    Map<Str, RuntimeRef<StorageCell>> refs;
    for (const auto &[name, ref] : value->namedRefs)
    {
      refs.insert_or_assign(name, ref);
    }
    return refs;
  }

  inline auto runtime_module_slot_named(const RuntimeRef<StorageCell> &value, const Str &name) -> RuntimeRef<StorageCell>
  {
    auto slots = runtime_module_object_slots(value);
    if (auto it = slots.find(name); it != slots.end())
    {
      return it->second;
    }
    return nullptr;
  }

  inline auto runtime_module_functions(const RuntimeRef<StorageCell> &value) -> Map<Str, NGCallable>
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    return state->functions;
  }

  inline auto runtime_module_types(const RuntimeRef<StorageCell> &value) -> Map<Str, RuntimeRef<NGType>>
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    return state->types;
  }

  inline auto runtime_module_trait_names(const RuntimeRef<StorageCell> &value) -> Set<Str>
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    return state->traitNames;
  }

  inline auto runtime_module_imports(const RuntimeRef<StorageCell> &value) -> Set<Str>
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    return state->imports;
  }

  inline auto runtime_module_exports(const RuntimeRef<StorageCell> &value) -> Set<Str>
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    return state->exports;
  }

  inline void runtime_module_set_native_function(const RuntimeRef<StorageCell> &value, const Str &name, NGCallable handler)
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    state->nativeFunctions.insert_or_assign(name, std::move(handler));
  }

  inline auto runtime_module_native_functions(const RuntimeRef<StorageCell> &value) -> Map<Str, NGCallable>
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    return state->nativeFunctions;
  }

  inline auto runtime_module_type_named(const RuntimeRef<StorageCell> &value, const Str &name) -> RuntimeRef<NGType>
  {
    auto types = runtime_module_types(value);
    if (auto it = types.find(name); it != types.end())
    {
      return it->second;
    }
    return nullptr;
  }

  inline void runtime_module_set_native_state(const RuntimeRef<StorageCell> &value, Str name, std::shared_ptr<void> stateValue)
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    state->nativeState.insert_or_assign(std::move(name), std::move(stateValue));
  }

  inline auto runtime_module_get_native_state(const RuntimeRef<StorageCell> &value, const Str &name) -> std::shared_ptr<void>
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    auto it = state->nativeState.find(name);
    return it == state->nativeState.end() ? nullptr : it->second;
  }

  inline void runtime_module_clear_native_state(const RuntimeRef<StorageCell> &value, const Str &name)
  {
    auto state = runtime_module_state(value);
    if (!state) throw RuntimeException("Expected module runtime value");
    state->nativeState.erase(name);
  }

  inline auto runtime_cell_slot_ref(const RuntimeRef<StorageCell> &cell, size_t index) -> RuntimeRef<StorageCell>
  {
    if (!cell || index >= cell->opaqueRefs.size() || !cell->opaqueRefs[index])
    {
      return nullptr;
    }
    return cell->opaqueRefs[index];
  }

  inline auto runtime_cell_slot_refs(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>
  {
    Vec<RuntimeRef<StorageCell>> slots;
    if (!cell)
    {
      return slots;
    }
    slots.reserve(cell->opaqueRefs.size());
    for (const auto &ref : cell->opaqueRefs)
    {
      slots.push_back(ref);
    }
    return slots;
  }

  inline auto runtime_cell_named_slot_refs(const RuntimeRef<StorageCell> &cell) -> Map<Str, RuntimeRef<StorageCell>>
  {
    Map<Str, RuntimeRef<StorageCell>> refs;
    if (!cell)
    {
      return refs;
    }
    for (const auto &[name, ref] : cell->namedRefs)
    {
      refs.insert_or_assign(name, ref);
    }
    return refs;
  }

  inline void ensure_nominal_cell_handlers(const RuntimeRef<NGType> &type)
  {
    if (!type)
    {
      return;
    }
    if (!type->showCellHandler)
    {
      type->showCellHandler = [](const RuntimeRef<StorageCell> &cell) {
        return runtime_value_show(runtime_cell_slot_ref(cell, 0));
      };
    }
    if (!type->boolCellHandler)
    {
      type->boolCellHandler = [](const RuntimeRef<StorageCell> &cell) {
        return runtime_value_bool(runtime_cell_slot_ref(cell, 0));
      };
    }
    if (type->memberFunctions.empty() && !type->respondCellHandler)
    {
      type->respondCellHandler =
          [](const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &env,
             const NGArgs &args) -> RuntimeRef<StorageCell> {
             auto wrapped = runtime_cell_slot_ref(cell, 0);
            if (!wrapped)
            {
              return nullptr;
            }
            return runtime_value_respond_slot(wrapped, member, env, args);
          };
    }
  }

  inline void ensure_nominal_cell_materializer(const RuntimeRef<NGType> &type)
  {
    ensure_nominal_cell_handlers(type);
  }

  inline auto make_runtime_newtype_cell(const RuntimeRef<NGType> &type, const RuntimeRef<StorageCell> &wrapped,
                                        StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto nominalType = type ? type : runtime_object_type();
    ensure_nominal_cell_handlers(nominalType);
    auto cell = make_storage_cell(wrapped ? wrapped->layout : nominalType->layout, storageClass, {}, nominalType);
    cell->runtimeType = nominalType;
    cell->layout = wrapped ? wrapped->layout : nominalType->layout;
    cell->bytes.clear();
    cell->opaqueRefs.clear();
    if (wrapped)
    {
      cell->opaqueRefs.push_back(wrapped);
    }
    cell->namedRefs.clear();
    cell->nativeHandles.clear();
    cell->traitObjectName.clear();
    cell->initialized = true;
    return cell;
  }

  inline auto runtime_cell_has_value(const RuntimeRef<StorageCell> &cell) -> bool
  {
    return cell && cell->initialized;
  }

  inline auto runtime_cell_is_moved(const RuntimeRef<StorageCell> &cell) -> bool
  {
    if (!cell)
    {
      return false;
    }
    if (cell->runtimeType && cell->runtimeType->name == "moved")
    {
      return true;
    }
    return false;
  }

  inline void ensure_usable_cell(const RuntimeRef<StorageCell> &cell)
  {
    if (runtime_cell_is_moved(cell))
    {
      throw RuntimeException("Use after move");
    }
  }

  inline void clear_storage_cell(const RuntimeRef<StorageCell> &cell)
  {
    if (!cell)
    {
      return;
    }
    cell->bytes.clear();
    cell->opaqueRefs.clear();
    cell->namedRefs.clear();
    cell->moduleState = nullptr;
    cell->traitObjectName.clear();
    cell->nativeHandles.clear();
    cell->initialized = false;
    cell->marked = false;
    cell->dropArmed = false;
    cell->lifecycleDropped = false;
    cell->dropInProgress = false;
  }

  inline auto runtime_value_type(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<NGType>
  {
    if (cell && cell->runtimeType)
    {
      return cell->runtimeType;
    }
    if (cell)
    {
      static Map<Str, RuntimeRef<NGType>> synthesizedTypes;
      auto layout = cell->layout;
      if (layout.name.empty())
      {
        layout.name = "Object";
      }
      auto key = std::to_string(layout.id) + ":" + layout.name + ":" +
                 std::to_string(static_cast<uint8_t>(layout.kind)) + ":" + std::to_string(layout.size) + ":" +
                 std::to_string(layout.alignment);
      for (const auto &field : layout.fields)
      {
        key += "|f:" + field.name + ":" + std::to_string(field.layoutId) + ":" + std::to_string(field.offset) + ":" +
               std::to_string(field.size) + ":" + std::to_string(field.alignment);
      }
      for (const auto &variant : layout.variants)
      {
        key += "|v:" + variant.name + ":" + std::to_string(variant.tag) + ":" +
               std::to_string(variant.payloadOffset) + ":" + std::to_string(variant.payloadSize) + ":" +
               std::to_string(variant.alignment);
        for (const auto &field : variant.fields)
        {
          key += "|vf:" + field.name + ":" + std::to_string(field.layoutId) + ":" + std::to_string(field.offset) +
                 ":" + std::to_string(field.size) + ":" + std::to_string(field.alignment);
        }
      }
      auto it = synthesizedTypes.find(key);
      if (it != synthesizedTypes.end())
      {
        return it->second;
      }
      auto type = makert<NGType>(NGType{
          .name = layout.name,
          .layout = layout,
      });
      synthesizedTypes.insert_or_assign(key, type);
      return type;
    }
    return runtime_object_type();
  }

  inline auto runtime_value_layout(const RuntimeRef<StorageCell> &cell) -> TypeLayout
  {
    if (cell &&
        (cell->layout.id != 0 || !cell->layout.name.empty() || cell->layout.size != 0 || !cell->layout.fields.empty() ||
         !cell->layout.variants.empty()))
    {
      return cell->layout;
    }
    if (cell && cell->runtimeType)
    {
      auto layout = cell->runtimeType->layout;
      if (layout.name.empty())
      {
        layout.name = cell->runtimeType->name;
      }
      return layout;
    }
    return cell ? cell->layout : TypeLayout{};
  }

  inline void runtime_copy_storage_cell(const RuntimeRef<StorageCell> &dst, const RuntimeRef<StorageCell> &src)
  {
    if (!dst)
    {
      return;
    }
    if (!src)
    {
      dst->runtimeType = nullptr;
      dst->layout = {};
      dst->bytes.clear();
      dst->nativeHandles.clear();
      dst->opaqueRefs.clear();
      dst->namedRefs.clear();
      dst->moduleState = nullptr;
      dst->traitObjectName.clear();
      dst->lifecycleDropped = false;
      dst->dropInProgress = false;
      dst->dropArmed = false;
      dst->initialized = false;
      return;
    }
    dst->namedRefs = src->namedRefs;
    dst->moduleState = src->moduleState;
    dst->traitObjectName = src->traitObjectName;
    dst->runtimeType = src->runtimeType;
    dst->layout = src->layout;
    dst->bytes = src->bytes;
    dst->nativeHandles = src->nativeHandles;
    dst->opaqueRefs = src->opaqueRefs;
    dst->initialized = src->initialized;
    dst->dropArmed = src->dropArmed;
    dst->lifecycleDropped = src->lifecycleDropped;
    dst->dropInProgress = false;
  }

  inline auto clone_runtime_storage_cell(const RuntimeRef<StorageCell> &source,
                                         StorageClass storageClass = StorageClass::TEMPORARY,
                                         Str name = {}) -> RuntimeRef<StorageCell>
  {
    if (!source)
    {
      auto slot = unit_cell(storageClass);
      slot->name = std::move(name);
      return slot;
    }
    if (runtime_is_reference_value(source))
    {
      auto slot = make_storage_cell(source->layout, storageClass, std::move(name), source->runtimeType);
      slot->bytes = source->bytes;
      auto sourceHasOwningNativeHandle = native_handles_contain_owner(source->nativeHandles);
      auto borrowOwningNativeHandle = sourceHasOwningNativeHandle && source->storageClass != StorageClass::TEMPORARY;
      slot->nativeHandles = borrowOwningNativeHandle ? clone_native_handles_as_borrowed(source->nativeHandles)
                                                     : source->nativeHandles;
      slot->initialized = source->initialized;
      slot->marked = false;
      slot->ownerScopeId = source->ownerScopeId;
      slot->dropArmed = borrowOwningNativeHandle ? false : source->dropArmed;
      slot->opaqueRefs = source->opaqueRefs;
      slot->namedRefs = source->namedRefs;
      slot->moduleState = source->moduleState;
      slot->traitObjectName = source->traitObjectName;
      slot->lifecycleDropped = borrowOwningNativeHandle ? true : source->lifecycleDropped;
      slot->dropInProgress = false;
      return slot;
    }
    if (source->storageClass == StorageClass::HEAP && storageClass == StorageClass::HEAP)
    {
      return source;
    }
    auto slot = make_storage_cell(source->layout, storageClass, std::move(name), source->runtimeType);
    slot->bytes = source->bytes;
    auto sourceHasOwningNativeHandle = native_handles_contain_owner(source->nativeHandles);
    auto borrowOwningNativeHandle = sourceHasOwningNativeHandle && source->storageClass != StorageClass::TEMPORARY;
    slot->nativeHandles = borrowOwningNativeHandle ? clone_native_handles_as_borrowed(source->nativeHandles)
                                                   : source->nativeHandles;
    slot->initialized = source->initialized;
    slot->marked = false;
    slot->ownerScopeId = source->ownerScopeId;
    slot->dropArmed = borrowOwningNativeHandle ? false : source->dropArmed;
    slot->moduleState = source->moduleState;
    slot->traitObjectName = source->traitObjectName;
    slot->lifecycleDropped = borrowOwningNativeHandle ? true : source->lifecycleDropped;
    slot->dropInProgress = false;
    slot->opaqueRefs.clear();
    slot->opaqueRefs.reserve(source->opaqueRefs.size());
    for (const auto &ref : source->opaqueRefs)
    {
      slot->opaqueRefs.push_back(ref ? clone_runtime_storage_cell(ref, storageClass, ref->name) : nullptr);
    }
    slot->namedRefs.clear();
    for (const auto &[refName, ref] : source->namedRefs)
    {
      slot->namedRefs.insert_or_assign(refName, ref ? clone_runtime_storage_cell(ref, storageClass, refName) : nullptr);
    }
    return slot;
  }

  inline auto make_temporary_runtime_slot(const RuntimeRef<StorageCell> &value, Str name = {}) -> RuntimeRef<StorageCell>
  {
    if (!value)
    {
      auto slot = unit_cell();
      slot->name = std::move(name);
      return slot;
    }
    return clone_runtime_storage_cell(value, StorageClass::TEMPORARY, std::move(name));
  }

  inline auto runtime_value_show(const RuntimeRef<StorageCell> &cell) -> Str
  {
    if (runtime_is_trait_object_ref(cell))
    {
      return runtime_value_show(runtime_trait_object_target(cell));
    }
    if (cell)
    {
      if (auto valueType = runtime_value_type(cell); valueType && valueType->showCellHandler)
      {
        return valueType->showCellHandler(cell);
      }
    }
    return "unit";
  }

  inline auto runtime_value_bool(const RuntimeRef<StorageCell> &cell) -> bool
  {
    if (runtime_is_trait_object_ref(cell))
    {
      return runtime_value_bool(runtime_trait_object_target(cell));
    }
    if (cell)
    {
      if (auto valueType = runtime_value_type(cell); valueType && valueType->boolCellHandler)
      {
        return valueType->boolCellHandler(cell);
      }
    }
    return false;
  }

  inline auto runtime_dispatch_member(const RuntimeRef<NGType> &type, const NGSelf &self, const Str &member,
                                      const NGEnv &env, const NGArgs &args) -> RuntimeRef<StorageCell>
  {
    if (!type)
    {
      return nullptr;
    }
    if (!type->memberFunctions.contains(member))
    {
      auto separator = member.find("::");
      if (separator == Str::npos)
      {
        return nullptr;
      }
      auto shortMember = member.substr(separator + 2);
      if (!type->memberFunctions.contains(shortMember))
      {
        return nullptr;
      }
      auto result = type->memberFunctions.at(shortMember)(self, env, args);
      return result ? result : unit_cell();
    }
    auto result = type->memberFunctions.at(member)(self, env, args);
    return result ? result : unit_cell();
  }

  inline auto runtime_value_respond_slot(const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &env,
                                         const NGArgs &args) -> RuntimeRef<StorageCell>
  {
    if (!cell)
    {
      throw RuntimeException("Cannot respond to member '" + member + "' on null storage cell");
    }
    if (runtime_is_trait_object_ref(cell))
    {
      auto target = runtime_trait_object_target(cell);
      auto traitName = runtime_trait_object_name(cell);
      auto qualifiedMember = member.find("::") == Str::npos ? traitName + "::" + member : member;
      if (auto result = runtime_dispatch_member(runtime_value_type(target), target, qualifiedMember, env, args))
      {
        return result;
      }
      throw NotImplementedException("Not implemented " + runtime_value_type(target)->name + "#" + qualifiedMember);
    }
    if (auto type = runtime_value_type(cell); type && type->respondCellHandler)
    {
      if (auto result = type->respondCellHandler(cell, member, env, args))
      {
        return result;
      }
    }
    if (auto result = runtime_dispatch_member(runtime_value_type(cell), cell, member, env, args))
    {
      return result;
    }
    throw NotImplementedException("Not implemented " + runtime_value_type(cell)->name + "#" + member);
  }

  inline auto runtime_value_respond(const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &env,
                                    const NGArgs &args) -> RuntimeRef<StorageCell>
  {
    return runtime_value_respond_slot(cell, member, env, args);
  }
} // namespace NG::runtime
