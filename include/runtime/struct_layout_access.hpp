#pragma once

#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

#include <optional>

namespace NG::runtime
{
  inline auto structural_field_index(const RuntimeRef<NGStructuralObject> &structural, const Str &member)
      -> std::optional<size_t>
  {
    if (!structural || !structural->customizedType)
    {
      return std::nullopt;
    }
    const auto &props = structural->customizedType->properties;
    for (size_t i = 0; i < props.size(); ++i)
    {
      if (props[i] == member)
      {
        return i;
      }
    }
    return std::nullopt;
  }

  inline auto structural_read_member(const RuntimeRef<NGStructuralObject> &structural, const Str &member)
      -> RuntimeRef<NGObject>
  {
    if (auto index = structural_field_index(structural, member))
    {
      if (auto slot = structural->field_slot(*index))
      {
        return slot->boxedValue;
      }
      return makert<NGUnit>();
    }
    if (structural->properties.contains(member))
    {
      return structural->properties.at(member);
    }
    return nullptr;
  }

  inline void structural_write_member(const RuntimeRef<NGStructuralObject> &structural, const Str &member,
                                      const RuntimeRef<NGObject> &value)
  {
    if (auto index = structural_field_index(structural, member))
    {
      auto slot = structural->field_slot(*index);
      if (!slot)
      {
        auto values = structural->payload_fields();
        if (*index >= values.size())
        {
          values.resize(*index + 1, makert<NGUnit>());
        }
        values[*index] = value;
        structural->replace_payload_fields(values);
      }
      else
      {
        runtime_sync_storage_cell(slot, value);
        (void) structural->payload_fields();
      }
      return;
    }
    structural->properties[member] = value;
  }
} // namespace NG::runtime
