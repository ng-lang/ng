#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  namespace
  {
    struct ManagedHeapState
    {
      size_t nextProviderId = 1;
      size_t nextFinalizerId = 1;
      Vec<RuntimeRef<StorageCell>> cells;
      Map<size_t, GCRootProvider> rootProviders;
      Map<size_t, GCFinalizer> finalizers;
    };

    auto heap_state() -> ManagedHeapState &
    {
      static ManagedHeapState state;
      return state;
    }

    void trace_storage_cell(const RuntimeRef<StorageCell> &cell, Set<const StorageCell *> &seenCells)
    {
      if (!cell || seenCells.contains(cell.get()))
      {
        return;
      }
      seenCells.insert(cell.get());
      for (const auto &ref : cell->opaqueRefs)
      {
        if (ref)
        {
          trace_storage_cell(ref, seenCells);
        }
      }
      for (const auto &[name, ref] : cell->namedRefs)
      {
        if (ref)
        {
          trace_storage_cell(ref, seenCells);
        }
      }
    }

  } // namespace

  auto allocate_heap_cell(const RuntimeRef<StorageCell> &value, const Str &debugName) -> RuntimeRef<StorageCell>
  {
    auto cell = clone_runtime_storage_cell(value, StorageClass::HEAP, debugName);
    cell->name = debugName;
    heap_state().cells.push_back(cell);
    return make_runtime_reference_cell(cell, debugName, StorageClass::TEMPORARY);
  }

  auto enumerate_symbol_roots(const NGSymbols &symbols) -> GCRootSet
  {
    GCRootSet roots;
    if (!symbols)
    {
      return roots;
    }

    for (const auto &[name, slot] : symbols->objectSlots)
    {
      if (slot)
      {
        roots.cells.push_back(slot);
      }
    }
    for (const auto &[name, module] : symbols->modules)
    {
      if (module)
      {
        roots.cells.push_back(module);
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

  auto register_gc_finalizer(GCFinalizer finalizer) -> size_t
  {
    auto &state = heap_state();
    size_t id = state.nextFinalizerId++;
    state.finalizers[id] = std::move(finalizer);
    return id;
  }

  void unregister_gc_finalizer(size_t finalizerId)
  {
    heap_state().finalizers.erase(finalizerId);
  }

  void collect_managed_heap()
  {
    auto &state = heap_state();
    for (const auto &cell : state.cells) cell->marked = false;

    Set<const StorageCell *> seenCells;
    for (const auto &[id, provider] : state.rootProviders)
    {
      auto roots = provider();
      for (const auto &cell : roots.cells)
      {
        trace_storage_cell(cell, seenCells);
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
      for (const auto &[id, finalizer] : heap_state().finalizers)
      {
        finalizer(cell);
      }
      clear_storage_cell(cell);
      return true;
    });
  }

  auto managed_heap_size() -> size_t { return heap_state().cells.size(); }
} // namespace NG::runtime
