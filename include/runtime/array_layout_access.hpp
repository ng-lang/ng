#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  inline auto array_length(const NGArray &array) -> size_t
  {
    array.sync_header_backing();
    return static_cast<size_t>(array.header_length());
  }

  inline auto array_element_slot(const NGArray &array, size_t index) -> RuntimeRef<StorageCell>
  {
    if (index >= array_length(array))
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    if (auto slot = array.element_slot(index))
    {
      return slot;
    }
    auto values = array.payload_items();
    auto mutableArray = const_cast<NGArray *>(&array);
    mutableArray->replace_payload_items(values, array.header_capacity());
    return mutableArray->element_slot(index);
  }

  inline auto array_read_element(const NGArray &array, size_t index) -> RuntimeRef<NGObject>
  {
    auto slot = array_element_slot(array, index);
    return slot ? slot->boxedValue : nullptr;
  }

  inline void array_write_element(NGArray &array, size_t index, const RuntimeRef<NGObject> &value)
  {
    auto slot = array_element_slot(array, index);
    runtime_sync_storage_cell(slot, value);
    (void) array.payload_items();
  }
} // namespace NG::runtime
