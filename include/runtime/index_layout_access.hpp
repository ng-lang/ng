#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/array_layout_access.hpp>
#include <runtime/tuple_layout_access.hpp>

namespace NG::runtime
{
  inline auto runtime_index_slot(const RuntimeRef<StorageCell> &container, const RuntimeRef<StorageCell> &index)
      -> RuntimeRef<StorageCell>
  {
    auto indexValue = read_numeric_cell_as<int32_t>(index);
    if (indexValue < 0)
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(indexValue));
    }

    auto offset = static_cast<size_t>(indexValue);
    if (runtime_is_array_value(container))
    {
      return array_element_slot(container, offset);
    }
    if (runtime_is_tuple_value(container))
    {
      return tuple_element_slot(container, offset);
    }

    throw IllegalTypeException("Not index-accessible");
  }

  inline auto runtime_index_read(const RuntimeRef<StorageCell> &container, const RuntimeRef<StorageCell> &index)
      -> RuntimeRef<StorageCell>
  {
    return runtime_index_slot(container, index);
  }

  inline auto runtime_index_write(const RuntimeRef<StorageCell> &container, const RuntimeRef<StorageCell> &index,
                                  const RuntimeRef<StorageCell> &value) -> RuntimeRef<StorageCell>
  {
    auto slot = runtime_index_slot(container, index);
    runtime_copy_storage_cell(slot, value);
    return slot;
  }
} // namespace NG::runtime
