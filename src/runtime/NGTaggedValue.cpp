
#include "intp/runtime.hpp"
#include "intp/runtime_numerals.hpp"
#include <runtime/tagged_layout_access.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto tagged_runtime_type(const NGTaggedValue &tagged) -> RuntimeRef<NGType>
  {
    auto layout = TypeLayout{.name = tagged.unionName, .kind = LayoutKind::TAGGED_UNION};
    VariantLayout variant{.name = tagged.variantName, .tag = static_cast<uint32_t>(tagged.variantIndex)};
    variant.fields.reserve(tagged.payloadNames.size());
    for (size_t i = 0; i < tagged.payloadNames.size(); ++i)
    {
      variant.fields.push_back(FieldLayout{.name = tagged.payloadNames[i]});
    }
    layout.variants.push_back(std::move(variant));
    return makert<NGType>(NGType{
        .name = tagged.unionName,
        .layout = std::move(layout),
        .respondHandler =
            [](const NGSelf &self, const Str &member, const NGCtx &, const NGArgs &) -> RuntimeRef<NGObject> {
              auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(self);
              if (!tagged)
              {
                return nullptr;
              }
              if (auto value = tagged_read_member(*tagged, member))
              {
                return value;
              }
              if (member == "tag")
              {
                return makert<NGString>(tagged->variantName);
              }
              if (member == "index")
              {
                return makert<NGIntegral<int32_t>>(tagged->variantIndex);
              }
              return nullptr;
            },
        .showHandler =
            [](const NGSelf &self) {
              auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(self);
              return tagged ? tagged_runtime_show(*tagged) : Str{"<tagged>"};
            },
        .boolHandler = [](const NGSelf &) { return true; },
    });
  }

  auto tagged_runtime_show(const NGTaggedValue &tagged) -> Str
  {
    Str result = tagged.variantName + "(";
    auto values = tagged.payload_items();
    for (size_t i = 0; i < values.size(); ++i)
    {
      if (i > 0)
      {
        result += ", ";
      }
      result += runtime_value_show(values[i]);
    }
    result += ")";
    return result;
  }

  namespace
  {
    void sync_tagged_slot(const RuntimeRef<StorageCell> &slot, const RuntimeRef<NGObject> &value)
    {
      if (!slot)
      {
        return;
      }
      runtime_sync_storage_cell(slot, value);
    }
  } // namespace


  void NGTaggedValue::sync_payload_backing() const
  {
    if (!payloadRef.valid())
    {
      payloadRef = payloadStore.allocate(buffer_runtime::make_byte_buffer_layout(0, "Tagged.payload"));
    }
  }

  void NGTaggedValue::sync_payload_slots() const
  {
    sync_payload_backing();
    auto &slots = const_cast<NGTaggedValue *>(this)->payloadSlots;
    auto &payloadRefs = payloadStore.get(payloadRef).opaqueRefs;
    if (slots.size() != payloadRefs.size())
    {
      slots.resize(payloadRefs.size());
    }
    for (size_t i = 0; i < slots.size(); ++i)
    {
      if (!slots[i] && i < payloadRefs.size() && payloadRefs[i])
      {
        slots[i] = std::static_pointer_cast<StorageCell>(payloadRefs[i]);
      }
      if (!slots[i])
      {
        slots[i] = make_boxed_storage_cell(makert<NGUnit>(), StorageClass::TEMPORARY);
        slots[i]->name = i < payloadNames.size() && !payloadNames[i].empty() ? payloadNames[i] : std::to_string(i);
      }
      payloadRefs[i] = slots[i];
    }
  }

  auto NGTaggedValue::payload_cell() const -> CellRef
  {
    sync_payload_backing();
    return payloadRef;
  }

  auto NGTaggedValue::payload_items() const -> Vec<RuntimeRef<NGObject>>
  {
    sync_payload_backing();
    sync_payload_slots();
    Vec<RuntimeRef<NGObject>> values;
    values.reserve(payloadSlots.size());
    for (const auto &slot : payloadSlots)
    {
      values.push_back(slot ? slot->boxedValue : makert<NGUnit>());
    }
    payloadStore.get(payloadRef).opaqueRefs.assign(payloadSlots.begin(), payloadSlots.end());
    return values;
  }

  void NGTaggedValue::replace_payload_items(const Vec<RuntimeRef<NGObject>> &values)
  {
    if (!payloadRef.valid() || payloadStore.get(payloadRef).opaqueRefs.size() != values.size())
    {
      payloadRef = payloadStore.allocate(
          buffer_runtime::make_byte_buffer_layout(values.size() * sizeof(uintptr_t), "Tagged.payload"));
    }
    auto &payloadCell = payloadStore.get(payloadRef);
    payloadSlots.resize(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
      if (!payloadSlots[i])
      {
        payloadSlots[i] = make_boxed_storage_cell(values[i], StorageClass::TEMPORARY);
        payloadSlots[i]->name = i < payloadNames.size() && !payloadNames[i].empty() ? payloadNames[i] : std::to_string(i);
      }
      sync_tagged_slot(payloadSlots[i], values[i]);
    }
    payloadCell.opaqueRefs.assign(payloadSlots.begin(), payloadSlots.end());
  }

  auto NGTaggedValue::payload_slot(size_t index) const -> RuntimeRef<StorageCell>
  {
    sync_payload_slots();
    if (index >= payloadSlots.size())
    {
      return nullptr;
    }
    return payloadSlots[index];
  }

} // namespace NG::runtime
