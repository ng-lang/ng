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
    int32_t indexValue = 0;
    if (runtime_is_from_end_index(index))
    {
      auto fromEnd = runtime_from_end_index_value(index);
      if (fromEnd <= 0)
      {
        throw RuntimeException("From-end index must be positive: " + std::to_string(fromEnd));
      }
      size_t length = 0;
      try
      {
        length = runtime_sequence_length(container);
      }
      catch (const RuntimeException &)
      {
        throw IllegalTypeException("Not index-accessible");
      }
      if (length == 0 || static_cast<size_t>(fromEnd) > length)
      {
        throw RuntimeException("Index out of bounds: ^" + std::to_string(fromEnd));
      }
      indexValue = static_cast<int32_t>(length - static_cast<size_t>(fromEnd));
    }
    else
    {
      indexValue = read_numeric_cell_as<int32_t>(index);
      if (indexValue < 0)
      {
        throw RuntimeException("Index out of bounds: " + std::to_string(indexValue));
      }
    }

    auto offset = static_cast<size_t>(indexValue);
    try
    {
      return runtime_sequence_slot(container, offset);
    }
    catch (const SequenceCompatibilityException &)
    {
      throw IllegalTypeException("Not index-accessible");
    }
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
