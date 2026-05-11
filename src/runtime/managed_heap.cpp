#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  namespace
  {
    struct ManagedHeapState
    {
      size_t nextProviderId = 1;
      Vec<RuntimeRef<StorageCell>> cells;
      Map<size_t, GCRootProvider> rootProviders;
      Set<NGContext *> trackedContexts;
    };

    auto heap_state() -> ManagedHeapState &
    {
      static ManagedHeapState state;
      return state;
    }

    void trace_object(const RuntimeRef<NGObject> &value, Set<const NGObject *> &seenObjects,
                      Set<const StorageCell *> &seenCells);

    void trace_storage_cell(const RuntimeRef<StorageCell> &cell, Set<const NGObject *> &seenObjects,
                            Set<const StorageCell *> &seenCells)
    {
      if (!cell || seenCells.contains(cell.get()))
      {
        return;
      }
      seenCells.insert(cell.get());
      trace_object(cell->boxedValue, seenObjects, seenCells);
    }

    void trace_reference_target(const RuntimeRef<NGReference> &reference, Set<const NGObject *> &seenObjects,
                                Set<const StorageCell *> &seenCells)
    {
      if (!reference)
      {
        return;
      }
      if (auto cell = reference->storage_cell())
      {
        trace_storage_cell(cell, seenObjects, seenCells);
        return;
      }
      reference->mark_referenced_heap();
      trace_object(reference->read(), seenObjects, seenCells);
    }

    void trace_object(const RuntimeRef<NGObject> &value, Set<const NGObject *> &seenObjects,
                      Set<const StorageCell *> &seenCells)
    {
      if (!value || seenObjects.contains(value.get()) || is_moved_object(value))
      {
        return;
      }
      seenObjects.insert(value.get());

      if (auto reference = std::dynamic_pointer_cast<NGReference>(value))
      {
        trace_reference_target(reference, seenObjects, seenCells);
        return;
      }
      if (auto array = std::dynamic_pointer_cast<NGArray>(value))
      {
        array->sync_element_slots();
        const auto &payload = array->header_store().get(array->payload_cell()).opaqueRefs;
        for (const auto &cell : payload)
        {
          trace_storage_cell(cell ? std::static_pointer_cast<StorageCell>(cell) : nullptr, seenObjects, seenCells);
        }
        return;
      }
      if (auto tuple = std::dynamic_pointer_cast<NGTuple>(value))
      {
        tuple->sync_element_slots();
        const auto &payload = tuple->payload_store().get(tuple->payload_cell()).opaqueRefs;
        for (const auto &cell : payload)
        {
          trace_storage_cell(cell ? std::static_pointer_cast<StorageCell>(cell) : nullptr, seenObjects, seenCells);
        }
        return;
      }
      if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(value))
      {
        for (const auto &[name, slot] : structural->propertySlots)
        {
          trace_storage_cell(slot, seenObjects, seenCells);
        }
        structural->sync_field_slots();
        const auto &payload = structural->payload_store().get(structural->payload_cell()).opaqueRefs;
        for (const auto &cell : payload)
        {
          trace_storage_cell(cell ? std::static_pointer_cast<StorageCell>(cell) : nullptr, seenObjects, seenCells);
        }
        return;
      }
      if (auto newType = std::dynamic_pointer_cast<NGNewType>(value))
      {
        trace_object(newType->wrapped, seenObjects, seenCells);
        return;
      }
      if (auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(value))
      {
        tagged->sync_payload_slots();
        const auto &payload = tagged->payload_store().get(tagged->payload_cell()).opaqueRefs;
        for (const auto &cell : payload)
        {
          trace_storage_cell(cell ? std::static_pointer_cast<StorageCell>(cell) : nullptr, seenObjects, seenCells);
        }
        return;
      }
      if (auto module = std::dynamic_pointer_cast<NGModule>(value))
      {
        for (const auto &[name, object] : module->objects) trace_object(object, seenObjects, seenCells);
      }
    }

  } // namespace

  void register_context_for_gc(NGContext *context)
  {
    heap_state().trackedContexts.insert(context);
  }

  void unregister_context_for_gc(NGContext *context)
  {
    heap_state().trackedContexts.erase(context);
  }

  auto auto_deref_value(const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>
  {
    ensure_usable_value(value);
    if (auto reference = std::dynamic_pointer_cast<NGReference>(value))
    {
      auto dereferenced = reference->read();
      ensure_usable_value(dereferenced);
      return dereferenced;
    }
    return value;
  }

  auto allocate_heap_object(const RuntimeRef<NGObject> &value, const Str &debugName) -> RuntimeRef<NGReference>
  {
    auto cell = make_boxed_storage_cell(value, StorageClass::HEAP);
    cell->name = debugName;
    heap_state().cells.push_back(cell);
    return makert<NGReference>(cell, debugName, [cell]() { cell->marked = true; });
  }

  auto enumerate_context_roots(const RuntimeRef<NGContext> &context) -> Vec<RuntimeRef<NGObject>>
  {
    Vec<RuntimeRef<NGObject>> roots;
    if (!context)
    {
      return roots;
    }
    Set<const RuntimeSymbolTable *> seenSymbols;
    auto rootSymbols = context->symbol_table();
    for (auto *tracked : heap_state().trackedContexts)
    {
      if (!tracked)
      {
        continue;
      }
      auto trackedSymbols = tracked->symbol_table();
      if (tracked != context.get() && trackedSymbols.get() != rootSymbols.get())
      {
        continue;
      }

      for (const auto &[name, slot] : tracked->objectSlots)
      {
        if (slot && slot->boxedValue)
        {
          roots.push_back(slot->boxedValue);
        }
      }
      if (seenSymbols.insert(trackedSymbols.get()).second)
      {
        for (const auto &[name, slot] : trackedSymbols->objectSlots)
        {
          if (slot && slot->boxedValue)
          {
            roots.push_back(slot->boxedValue);
          }
        }
        for (const auto &[name, module] : trackedSymbols->modules) roots.push_back(module);
      }
    }
    return roots;
  }

  auto register_gc_root_provider(GCRootProvider provider) -> size_t
  {
    auto &state = heap_state();
    size_t id = state.nextProviderId++;
    state.rootProviders[id] = std::move(provider);
    return id;
  }

  void unregister_gc_root_provider(size_t providerId)
  {
    heap_state().rootProviders.erase(providerId);
  }

  void collect_managed_heap()
  {
    auto &state = heap_state();
    for (const auto &cell : state.cells) cell->marked = false;

    Set<const NGObject *> seenObjects;
    Set<const StorageCell *> seenCells;
    for (const auto &[id, provider] : state.rootProviders)
    {
      for (const auto &root : provider())
      {
        trace_object(root, seenObjects, seenCells);
      }
    }

    for (const auto &cell : state.cells)
    {
      if (seenCells.contains(cell.get()))
      {
        cell->marked = true;
      }
    }

    std::erase_if(state.cells, [](const RuntimeRef<StorageCell> &cell) {
      if (cell->marked)
      {
        return false;
      }
      runtime_sync_storage_cell(cell, nullptr);
      return true;
    });
  }

  auto managed_heap_size() -> size_t { return heap_state().cells.size(); }
} // namespace NG::runtime
