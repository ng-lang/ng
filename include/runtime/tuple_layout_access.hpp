#pragma once

#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

#include <optional>

namespace NG::runtime
{
  inline auto tuple_length(const NGTuple &tuple) -> size_t
  {
    return tuple.payload_items().size();
  }

  inline auto tuple_read_element(const NGTuple &tuple, size_t index) -> RuntimeRef<NGObject>
  {
    if (index >= tuple_length(tuple))
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    if (auto slot = tuple.element_slot(index))
    {
      return slot->boxedValue;
    }
    return tuple.payload_items().at(index);
  }

  inline void tuple_write_element(NGTuple &tuple, size_t index, const RuntimeRef<NGObject> &value)
  {
    if (index >= tuple_length(tuple))
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    if (auto slot = tuple.element_slot(index))
    {
      runtime_sync_storage_cell(slot, value);
      (void) tuple.payload_items();
      return;
    }
    auto values = tuple.payload_items();
    values.at(index) = value;
    tuple.replace_payload_items(values);
  }

  inline auto tuple_read_member(const NGTuple &tuple, const Str &member) -> RuntimeRef<NGObject>
  {
    if (member == "size")
    {
      return makert<NGIntegral<uint32_t>>(tuple_length(tuple));
    }
    try
    {
      auto index = std::stoul(member);
      return tuple_read_element(tuple, index);
    }
    catch (const std::exception &)
    {
      return nullptr;
    }
  }
} // namespace NG::runtime
