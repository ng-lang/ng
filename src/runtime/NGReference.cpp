#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto moved_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "moved",
        .layout = TypeLayout{.name = "moved", .kind = LayoutKind::INLINE_VALUE},
        .showCellHandler = [](const RuntimeRef<StorageCell> &) { return Str{"<moved>"}; },
        .boolCellHandler = [](const RuntimeRef<StorageCell> &) { return false; },
    });
    return type;
  }

  auto reference_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "ref",
        .layout = buffer_runtime::make_reference_layout("ref"),
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return "ref(" + cell->name + ")";
            },
        .boolCellHandler = [](const RuntimeRef<StorageCell> &) { return true; },
    });
    return type;
  }

  auto trait_object_ref_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "ref<trait>",
        .layout = buffer_runtime::make_reference_layout("ref<trait>"),
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return "ref<" + runtime_trait_object_name(cell) + ">(" + cell->name + ")";
            },
        .boolCellHandler = [](const RuntimeRef<StorageCell> &) { return true; },
    });
    return type;
  }

  auto make_runtime_reference_cell(const RuntimeRef<StorageCell> &targetCell, Str debugName,
                                   StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto type = reference_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, std::move(debugName), type);
    cell->opaqueRefs = {targetCell};
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_reference_value(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return type && type->name == "ref";
  }

  auto runtime_reference_target(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_reference_value(cell))
    {
      return nullptr;
    }
    return runtime_cell_slot_ref(cell, 0);
  }

  auto make_runtime_trait_object_ref(const RuntimeRef<StorageCell> &targetRef, Str traitName, Str debugName,
                                     StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_reference_value(targetRef))
    {
      throw RuntimeException("Trait object requires a concrete reference value");
    }
    auto type = trait_object_ref_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, std::move(debugName), type);
    cell->opaqueRefs = {targetRef};
    cell->traitObjectName = std::move(traitName);
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_trait_object_ref(const RuntimeRef<StorageCell> &cell) -> bool
  {
    return cell && cell->runtimeType && cell->runtimeType->name == "ref<trait>";
  }

  auto runtime_trait_object_target_ref(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_trait_object_ref(cell))
    {
      return nullptr;
    }
    return runtime_cell_slot_ref(cell, 0);
  }

  auto runtime_trait_object_target(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
  {
    auto targetRef = runtime_trait_object_target_ref(cell);
    if (!targetRef)
    {
      throw RuntimeException("Dangling trait object reference");
    }
    return runtime_read_reference(targetRef);
  }

  auto runtime_trait_object_name(const RuntimeRef<StorageCell> &cell) -> Str
  {
    if (!runtime_is_trait_object_ref(cell))
    {
      return {};
    }
    return cell->traitObjectName;
  }

  auto runtime_read_reference(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
  {
    if (auto target = runtime_reference_target(cell))
    {
      if (!runtime_cell_has_value(target))
      {
        throw RuntimeException("Dangling heap reference");
      }
      ensure_usable_cell(target);
      return target;
    }
    throw RuntimeException("Cannot dereference non-reference value");
  }

  void runtime_write_reference(const RuntimeRef<StorageCell> &cell, const RuntimeRef<StorageCell> &nextValue)
  {
    if (auto target = runtime_reference_target(cell))
    {
      if (!runtime_cell_has_value(target))
      {
        throw RuntimeException("Dangling heap reference");
      }
      runtime_copy_storage_cell(target, nextValue);
      return;
    }
    throw RuntimeException("Cannot assign through non-reference value");
  }

  void mark_moved_storage_cell(const RuntimeRef<StorageCell> &cell)
  {
    if (!cell)
    {
      return;
    }
    auto type = moved_runtime_type();
    cell->runtimeType = type;
    cell->layout = type->layout;
    cell->bytes.clear();
    cell->opaqueRefs.clear();
    cell->namedRefs.clear();
    cell->nativeHandles.clear();
    cell->traitObjectName.clear();
    cell->dropArmed = false;
    cell->initialized = true;
  }

} // namespace NG::runtime
