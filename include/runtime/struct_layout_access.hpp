#pragma once

#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

#include <optional>

namespace NG::runtime
{
  inline auto structural_field_index(const RuntimeRef<StorageCell> &cell, const Str &member) -> std::optional<size_t>
  {
    auto type = runtime_value_type(cell);
    if (!type)
    {
      return std::nullopt;
    }
    const auto &layoutFields = type->layout.fields;
    for (size_t i = 0; i < layoutFields.size(); ++i)
    {
      if (layoutFields[i].name == member)
      {
        return i;
      }
    }
    const auto &props = type->properties;
    for (size_t i = 0; i < props.size(); ++i)
    {
      if (props[i] == member)
      {
        return i;
      }
    }
    return std::nullopt;
  }

  inline auto structural_member_slot(const RuntimeRef<StorageCell> &cell, const Str &member) -> RuntimeRef<StorageCell>
  {
    if (auto index = structural_field_index(cell, member))
    {
      return runtime_cell_slot_ref(cell, *index);
    }
    auto named = runtime_cell_named_slot_refs(cell);
    if (auto it = named.find(member); it != named.end())
    {
      return it->second;
    }
    return nullptr;
  }

  inline auto structural_member_slot_or_create(const RuntimeRef<StorageCell> &cell, const Str &member)
      -> RuntimeRef<StorageCell>
  {
    if (!cell)
    {
      return nullptr;
    }
    if (auto index = structural_field_index(cell, member))
    {
      if (*index >= cell->opaqueRefs.size())
      {
        cell->opaqueRefs.resize(*index + 1);
      }
      if (!cell->opaqueRefs[*index])
      {
        auto slot = unit_cell();
        slot->name = member;
        cell->opaqueRefs[*index] = slot;
      }
      return cell->opaqueRefs[*index];
    }
    if (auto slot = structural_member_slot(cell, member))
    {
      return slot;
    }
    auto slot = unit_cell();
    slot->name = member;
    cell->namedRefs.insert_or_assign(member, slot);
    return slot;
  }

  inline auto structural_read_member_slot(const RuntimeRef<StorageCell> &cell, const Str &member)
      -> RuntimeRef<StorageCell>
  {
    return structural_member_slot(cell, member);
  }

  inline void structural_write_member(const RuntimeRef<StorageCell> &cell, const Str &member,
                                      const RuntimeRef<StorageCell> &value)
  {
    auto slot = structural_member_slot_or_create(cell, member);
    runtime_copy_storage_cell(slot, value ? value : unit_cell());
  }
} // namespace NG::runtime
