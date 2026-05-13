#pragma once

#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

#include <optional>

namespace NG::runtime
{
  inline auto tagged_payload_index(const RuntimeRef<StorageCell> &cell, const Str &member) -> std::optional<size_t>
  {
    if (auto type = runtime_value_type(cell); type)
    {
      if (!type->layout.variants.empty())
      {
        const auto &fields = type->layout.variants.front().fields;
        for (size_t i = 0; i < fields.size(); ++i)
        {
          if (fields[i].name == member)
          {
            return i;
          }
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
    }
    try
    {
      return std::stoul(member);
    }
    catch (const std::exception &)
    {
      return std::nullopt;
    }
  }

  inline auto tagged_member_slot(const RuntimeRef<StorageCell> &cell, const Str &member) -> RuntimeRef<StorageCell>
  {
    if (auto index = tagged_payload_index(cell, member))
    {
      return runtime_cell_slot_ref(cell, *index);
    }
    return nullptr;
  }

  inline auto tagged_read_member_slot(const RuntimeRef<StorageCell> &cell, const Str &member) -> RuntimeRef<StorageCell>
  {
    if (member == "tag")
    {
      auto type = runtime_value_type(cell);
      return make_runtime_string(type ? type->variantName : Str{});
    }
    if (member == "index")
    {
      auto type = runtime_value_type(cell);
      return numeral_cell_from_value<int32_t>(type ? type->variantIndex : -1);
    }
    return tagged_member_slot(cell, member);
  }
} // namespace NG::runtime
