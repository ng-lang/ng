#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto make_storage_cell(const TypeLayout &layout, StorageClass storageClass, const RuntimeRef<NGObject> &boxedValue,
                         Str name, const RuntimeRef<NGType> &runtimeType)
      -> RuntimeRef<StorageCell>
  {
    auto cell = makert<StorageCell>();
    static_cast<buffer_runtime::FrameSlot &>(*cell) = buffer_runtime::make_slot(std::move(name), layout, storageClass);
    runtime_sync_storage_cell(cell, boxedValue, runtimeType);
    return cell;
  }

  auto make_boxed_storage_cell(const RuntimeRef<NGObject> &value, StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    TypeLayout layout;
    RuntimeRef<NGType> valueType = runtime_value_type(value);
    if (valueType)
    {
      layout = valueType->layout;
      if (layout.name.empty())
      {
        layout.name = valueType->name;
      }
    }
    return make_storage_cell(layout, storageClass, value, {}, valueType);
  }
} // namespace NG::runtime
