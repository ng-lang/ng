#pragma once

#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

#include <optional>

namespace NG::runtime
{
  inline auto tagged_payload_index(const NGTaggedValue &tagged, const Str &member) -> std::optional<size_t>
  {
    for (size_t i = 0; i < tagged.payloadNames.size(); ++i)
    {
      if (tagged.payloadNames[i] == member)
      {
        return i;
      }
    }
    try
    {
      auto index = std::stoul(member);
      return index;
    }
    catch (const std::exception &)
    {
      return std::nullopt;
    }
  }

  inline auto tagged_read_member(const NGTaggedValue &tagged, const Str &member) -> RuntimeRef<NGObject>
  {
    if (auto index = tagged_payload_index(tagged, member))
    {
      if (auto slot = tagged.payload_slot(*index))
      {
        return slot->boxedValue;
      }
    }
    return nullptr;
  }

  inline void tagged_write_member(NGTaggedValue &tagged, const Str &member,
                                  const RuntimeRef<NGObject> &value)
  {
    if (auto index = tagged_payload_index(tagged, member))
    {
      auto slot = tagged.payload_slot(*index);
      if (!slot)
      {
        auto values = tagged.payload_items();
        values.resize(*index + 1, makert<NGUnit>());
        values[*index] = value;
        tagged.replace_payload_items(values);
      }
      else
      {
        runtime_sync_storage_cell(slot, value);
        (void) tagged.payload_items();
      }
    }
  }
} // namespace NG::runtime
