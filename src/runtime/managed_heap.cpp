#include <intp/runtime.hpp>

namespace NG::runtime
{
  namespace
  {
    struct ManagedCell
    {
      RuntimeRef<NGObject> value;
      bool marked = false;
      Str debugName;
    };

    struct ManagedHeapState
    {
      size_t nextProviderId = 1;
      Vec<std::shared_ptr<ManagedCell>> cells;
      Map<size_t, GCRootProvider> rootProviders;
    };

    auto heap_state() -> ManagedHeapState &
    {
      static ManagedHeapState state;
      return state;
    }

    void trace_object(const RuntimeRef<NGObject> &value, Set<const NGObject *> &seen);

    void trace_reference_target(const RuntimeRef<NGReference> &reference, Set<const NGObject *> &seen)
    {
      if (!reference)
      {
        return;
      }
      reference->mark_referenced_heap();
      trace_object(reference->read(), seen);
    }

    void trace_object(const RuntimeRef<NGObject> &value, Set<const NGObject *> &seen)
    {
      if (!value || seen.contains(value.get()) || is_moved_object(value))
      {
        return;
      }
      seen.insert(value.get());

      if (auto reference = std::dynamic_pointer_cast<NGReference>(value))
      {
        trace_reference_target(reference, seen);
        return;
      }
      if (auto array = std::dynamic_pointer_cast<NGArray>(value))
      {
        for (const auto &item : *array->items) trace_object(item, seen);
        return;
      }
      if (auto tuple = std::dynamic_pointer_cast<NGTuple>(value))
      {
        for (const auto &item : *tuple->items) trace_object(item, seen);
        return;
      }
      if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(value))
      {
        for (const auto &[name, property] : structural->properties) trace_object(property, seen);
        for (const auto &field : structural->fields) trace_object(field, seen);
        return;
      }
      if (auto newType = std::dynamic_pointer_cast<NGNewType>(value))
      {
        trace_object(newType->wrapped, seen);
        return;
      }
      if (auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(value))
      {
        for (const auto &item : tagged->payload) trace_object(item, seen);
        return;
      }
      if (auto module = std::dynamic_pointer_cast<NGModule>(value))
      {
        for (const auto &[name, object] : module->objects) trace_object(object, seen);
      }
    }

    void trace_context_roots(const RuntimeRef<NGContext> &context, Vec<RuntimeRef<NGObject>> &roots, Set<const NGContext *> &seen)
    {
      if (!context || seen.contains(context.get()))
      {
        return;
      }
      seen.insert(context.get());

      if (context->retVal)
      {
        roots.push_back(context->retVal);
      }
      for (const auto &[name, object] : context->objects) roots.push_back(object);
      for (const auto &[name, module] : context->modules) roots.push_back(module);

      for (auto it = context->children.begin(); it != context->children.end();)
      {
        if (auto child = it->lock())
        {
          trace_context_roots(child, roots, seen);
          ++it;
        }
        else
        {
          it = context->children.erase(it);
        }
      }
    }
  } // namespace

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
    auto cell = std::make_shared<ManagedCell>();
    cell->value = value;
    cell->debugName = debugName;
    heap_state().cells.push_back(cell);

    return makert<NGReference>(
        [cell]() -> RuntimeRef<NGObject> {
          if (!cell->value)
          {
            throw RuntimeException("Dangling heap reference");
          }
          ensure_usable_value(cell->value);
          return cell->value;
        },
        [cell](const RuntimeRef<NGObject> &newValue) {
          if (!cell->value)
          {
            throw RuntimeException("Dangling heap reference");
          }
          cell->value = newValue;
        },
        debugName,
        [cell]() { cell->marked = true; });
  }

  auto enumerate_context_roots(const RuntimeRef<NGContext> &context) -> Vec<RuntimeRef<NGObject>>
  {
    Vec<RuntimeRef<NGObject>> roots;
    Set<const NGContext *> seen;
    trace_context_roots(context, roots, seen);
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

    Set<const NGObject *> seen;
    for (const auto &[id, provider] : state.rootProviders)
    {
      for (const auto &root : provider())
      {
        trace_object(root, seen);
      }
    }

    for (const auto &cell : state.cells)
    {
      if (cell->value && seen.contains(cell->value.get()))
      {
        cell->marked = true;
      }
    }

    std::erase_if(state.cells, [](const std::shared_ptr<ManagedCell> &cell) {
      if (cell->marked)
      {
        return false;
      }
      cell->value = nullptr;
      return true;
    });
  }

  auto managed_heap_size() -> size_t { return heap_state().cells.size(); }
} // namespace NG::runtime
