#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto make_storage_cell(const TypeLayout &layout, StorageClass storageClass, Str name,
                         const RuntimeRef<NGType> &runtimeType)
      -> RuntimeRef<StorageCell>
  {
    auto cell = makert<StorageCell>();
    static_cast<buffer_runtime::FrameSlot &>(*cell) = buffer_runtime::make_slot(std::move(name), layout, storageClass);
    cell->runtimeType = runtimeType;
    cell->layout = layout;
    cell->initialized = runtimeType != nullptr || layout.size != 0 || !layout.name.empty();
    return cell;
  }

  auto make_value_storage_cell(const RuntimeRef<StorageCell> &value, StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto cell = make_storage_cell(value ? value->layout : TypeLayout{}, storageClass, {},
                                  value ? value->runtimeType : nullptr);
    runtime_copy_storage_cell(cell, value);
    return cell;
  }
} // namespace NG::runtime
