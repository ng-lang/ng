#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/tagged_layout_access.hpp>
#include <runtime/value_access.hpp>

#include <cstring>

namespace NG::runtime
{
  namespace
  {
    void ensure_tagged_cell_handlers(const RuntimeRef<NGType> &type)
    {
      if (!type)
      {
        return;
      }
      if (!type->respondCellHandler)
      {
        type->respondCellHandler =
            [](const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
          return tagged_read_member_slot(cell, member);
        };
      }
      if (!type->showCellHandler)
      {
        type->showCellHandler = [](const RuntimeRef<StorageCell> &cell) {
          auto type = runtime_value_type(cell);
          Str result = (type && !type->variantName.empty() ? type->variantName : Str{"<tagged>"}) + "(";
          auto slots = runtime_cell_slot_refs(cell);
          for (size_t i = 0; i < slots.size(); ++i)
          {
            if (i > 0)
            {
              result += ", ";
            }
            result += runtime_value_show(slots[i]);
          }
          result += ")";
          return result;
        };
      }
      if (!type->boolCellHandler)
      {
        type->boolCellHandler = [](const RuntimeRef<StorageCell> &) { return true; };
      }
    }
  } // namespace

  auto make_runtime_tagged_cell(const RuntimeRef<NGType> &type,
                                const Vec<RuntimeRef<StorageCell>> &payloadSlots,
                                StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    ensure_tagged_cell_handlers(type);
    auto cell = make_storage_cell(type ? type->layout : TypeLayout{}, storageClass, {}, type);
    cell->runtimeType = type;
    cell->layout = type ? type->layout : TypeLayout{};
    cell->bytes.resize(std::max<size_t>(cell->layout.size, sizeof(int32_t)));
    auto variantIndex = type ? type->variantIndex : -1;
    if (cell->bytes.size() >= sizeof(int32_t))
    {
      std::memcpy(cell->bytes.data(), &variantIndex, sizeof(int32_t));
    }
    cell->opaqueRefs.assign(payloadSlots.begin(), payloadSlots.end());
    cell->namedRefs.clear();
    cell->nativeHandles.clear();
    cell->initialized = true;
    return cell;
  }

  auto make_runtime_tagged_cell(Str unionName, Str variantName, int32_t variantIndex,
                                Vec<RuntimeRef<StorageCell>> payloadSlots, Vec<Str> payloadNames,
                                StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto layout = TypeLayout{.name = unionName, .kind = LayoutKind::TAGGED_UNION};
    VariantLayout variant{.name = variantName, .tag = static_cast<uint32_t>(variantIndex)};
    variant.fields.reserve(payloadNames.size());
    for (const auto &name : payloadNames)
    {
      variant.fields.push_back(FieldLayout{.name = name});
    }
    layout.variants.push_back(std::move(variant));
    auto type = makert<NGType>(NGType{
        .name = std::move(unionName),
        .layout = std::move(layout),
        .properties = payloadNames,
        .variantName = std::move(variantName),
        .variantIndex = variantIndex,
    });
    ensure_tagged_cell_handlers(type);
    return make_runtime_tagged_cell(type, payloadSlots, storageClass);
  }

  auto runtime_is_tagged_value(const RuntimeRef<StorageCell> &value) -> bool
  {
    auto type = runtime_value_type(value);
    return type && type->layout.kind == LayoutKind::TAGGED_UNION;
  }

  auto runtime_tagged_type(const RuntimeRef<StorageCell> &value) -> RuntimeRef<NGType>
  {
    if (!runtime_is_tagged_value(value))
    {
      throw RuntimeException("Expected tagged runtime value");
    }
    auto type = runtime_value_type(value);
    ensure_tagged_cell_handlers(type);
    return type;
  }

  auto runtime_tagged_union_name(const RuntimeRef<StorageCell> &value) -> Str
  {
    return runtime_tagged_type(value)->name;
  }

  auto runtime_tagged_variant_name(const RuntimeRef<StorageCell> &value) -> Str
  {
    return runtime_tagged_type(value)->variantName;
  }

  auto runtime_tagged_variant_index(const RuntimeRef<StorageCell> &value) -> int32_t
  {
    auto type = runtime_tagged_type(value);
    return type ? type->variantIndex : -1;
  }

  auto runtime_tagged_payload_names(const RuntimeRef<StorageCell> &value) -> Vec<Str>
  {
    auto type = runtime_tagged_type(value);
    if (!type->layout.variants.empty())
    {
      Vec<Str> names;
      for (const auto &field : type->layout.variants.front().fields)
      {
        names.push_back(field.name);
      }
      return names;
    }
    return type->properties;
  }

  auto runtime_tagged_slots(const RuntimeRef<StorageCell> &value) -> Vec<RuntimeRef<StorageCell>>
  {
    if (!runtime_is_tagged_value(value))
    {
      throw RuntimeException("Expected tagged runtime value");
    }
    return runtime_cell_slot_refs(value);
  }

  auto runtime_tagged_slot(const RuntimeRef<StorageCell> &value, size_t index) -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_tagged_value(value))
    {
      throw RuntimeException("Expected tagged runtime value");
    }
    return runtime_cell_slot_ref(value, index);
  }

  auto runtime_tagged_payload_index(const RuntimeRef<StorageCell> &value, const Str &member)
      -> std::optional<size_t>
  {
    if (!runtime_is_tagged_value(value))
    {
      throw RuntimeException("Expected tagged runtime value");
    }
    return tagged_payload_index(value, member);
  }

  auto runtime_tagged_read_member(const RuntimeRef<StorageCell> &value, const Str &member)
      -> RuntimeRef<StorageCell>
  {
    return tagged_read_member_slot(value, member);
  }
} // namespace NG::runtime
