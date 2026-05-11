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

  inline auto structural_member_slot(const RuntimeRef<NGStructuralObject> &structural, const Str &member)
      -> RuntimeRef<StorageCell>
  {
    if (!structural)
    {
      return nullptr;
    }
    if (auto index = structural_field_index(structural, member))
    {
      return structural->field_slot(*index);
    }
    return structural->property_slot(member);
  }

  inline auto structural_member_slot_or_create(const RuntimeRef<NGStructuralObject> &structural, const Str &member)
      -> RuntimeRef<StorageCell>
  {
    if (!structural)
    {
      return nullptr;
    }
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
        structural->replace_payload_fields(values);
        slot = structural->field_slot(*index);
      }
      return slot;
    }
    return structural->property_slot_or_create(member);
  }

  inline auto structural_read_member(const RuntimeRef<NGStructuralObject> &structural, const Str &member)
      -> RuntimeRef<NGObject>
  {
    auto slot = structural_member_slot(structural, member);
    return slot ? slot->boxedValue : nullptr;
  }

  inline void structural_write_member(const RuntimeRef<NGStructuralObject> &structural, const Str &member,
                                      const RuntimeRef<NGObject> &value)
  {
    auto slot = structural_member_slot_or_create(structural, member);
    runtime_sync_storage_cell(slot, value);
    (void) structural->payload_fields();
  }
} // namespace NG::runtime
