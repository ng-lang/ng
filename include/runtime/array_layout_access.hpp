#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  inline auto array_length(const RuntimeRef<StorageCell> &array) -> size_t
  {
    return runtime_array_length(array);
  }

  inline auto array_element_slot(const RuntimeRef<StorageCell> &array, size_t index) -> RuntimeRef<StorageCell>
  {
    if (index >= array_length(array))
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    if (auto slot = runtime_cell_slot_ref(array, index))
    {
      return slot;
    }
    throw RuntimeException("Array element slot is missing: " + std::to_string(index));
  }

  inline auto array_read_element(const RuntimeRef<StorageCell> &array, size_t index) -> RuntimeRef<StorageCell>
  {
    return array_element_slot(array, index);
  }

  inline void array_write_element(const RuntimeRef<StorageCell> &array, size_t index, const RuntimeRef<StorageCell> &value)
  {
    auto slot = array_element_slot(array, index);
    runtime_copy_storage_cell(slot, value);
  }
} // namespace NG::runtime
