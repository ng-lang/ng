#pragma once

#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

#include <optional>

namespace NG::runtime
{
  inline auto tuple_length(const RuntimeRef<StorageCell> &tuple) -> size_t
  {
    return runtime_tuple_length(tuple);
  }

  inline auto tuple_element_slot(const RuntimeRef<StorageCell> &tuple, size_t index) -> RuntimeRef<StorageCell>
  {
    if (index >= tuple_length(tuple))
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    if (auto slot = runtime_cell_slot_ref(tuple, index))
    {
      return slot;
    }
    throw RuntimeException("Tuple element slot is missing: " + std::to_string(index));
  }

  inline auto tuple_read_element(const RuntimeRef<StorageCell> &tuple, size_t index) -> RuntimeRef<StorageCell>
  {
    return tuple_element_slot(tuple, index);
  }

  inline void tuple_write_element(const RuntimeRef<StorageCell> &tuple, size_t index, const RuntimeRef<StorageCell> &value)
  {
    auto slot = tuple_element_slot(tuple, index);
    runtime_copy_storage_cell(slot, value);
  }

  inline auto tuple_read_member_slot(const RuntimeRef<StorageCell> &cell, const Str &member) -> RuntimeRef<StorageCell>;

  inline auto tuple_read_member(const RuntimeRef<StorageCell> &tuple, const Str &member) -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_tuple_value(tuple))
    {
      return nullptr;
    }
    return tuple_read_member_slot(tuple, member);
  }

  inline auto tuple_read_member_slot(const RuntimeRef<StorageCell> &cell, const Str &member) -> RuntimeRef<StorageCell>
  {
    if (member == "size")
    {
      return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(runtime_cell_slot_refs(cell).size()));
    }
    try
    {
      auto index = std::stoul(member);
      return runtime_cell_slot_ref(cell, index);
    }
    catch (const std::exception &)
    {
      return nullptr;
    }
  }
} // namespace NG::runtime
