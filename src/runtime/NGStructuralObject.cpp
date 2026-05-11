
#include <intp/runtime.hpp>
#include <runtime/struct_layout_access.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto structural_runtime_show(const NGStructuralObject &structural) -> Str
  {
    Str repr{};
    auto values = structural.payload_fields();
    if (structural.customizedType && !structural.customizedType->properties.empty())
    {
      for (size_t i = 0; i < structural.customizedType->properties.size(); ++i)
      {
        const auto &name = structural.customizedType->properties[i];
        RuntimeRef<NGObject> value = i < values.size() ? values[i] : nullptr;
        if (!value)
        {
          continue;
        }
        if (!repr.empty())
        {
          repr += ", ";
        }
        if (std::dynamic_pointer_cast<NGStructuralObject>(value))
        {
          repr += (name + ": #[obj]");
        }
        else
        {
          repr += (name + ": " + runtime_value_show(value));
        }
      }
      return "{ " + repr + " }";
    }

    for (const auto &[name, value] : structural.properties)
    {
      if (!repr.empty())
      {
        repr += ", ";
      }
      if (std::dynamic_pointer_cast<NGStructuralObject>(value))
      {
        repr += (name + ": #[obj]");
      }
      else
      {
        repr += (name + ": " + runtime_value_show(value));
      }
    }

    return "{ " + repr + " }";
  }

  auto structural_runtime_type(const NGStructuralObject &structural) -> RuntimeRef<NGType>
  {
    if (!structural.customizedType)
    {
      return NGObject::objectType();
    }
    if (!structural.customizedType->showHandler)
    {
      structural.customizedType->showHandler = [](const NGSelf &self) {
        auto structural = std::dynamic_pointer_cast<NGStructuralObject>(self);
        return structural ? structural_runtime_show(*structural) : Str{"{ }"};
      };
    }
    if (!structural.customizedType->boolHandler)
    {
      structural.customizedType->boolHandler = [](const NGSelf &) { return true; };
    }
    if (!structural.customizedType->respondHandler)
    {
      structural.customizedType->respondHandler =
          [](const NGSelf &self, const Str &member, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject> {
            auto structural = std::dynamic_pointer_cast<NGStructuralObject>(self);
            if (!structural)
            {
              return nullptr;
            }
            if (structural->selfMemberFunctions.contains(member))
            {
              auto newContext = context ? context->fork() : makert<NGContext>();
              newContext->define("self", self);
              auto result = structural->selfMemberFunctions[member](self, newContext, args);
              return result ? result : makert<NGUnit>();
            }
            return structural_read_member(structural, member);
          };
    }
    return structural.customizedType;
  }

  namespace
  {
    auto structural_payload_arity(const NGStructuralObject &structural) -> size_t
    {
      if (structural.customizedType)
      {
        return structural.customizedType->properties.size();
      }
      if (structural.payloadRef.valid())
      {
        return structural.payloadStore.get(structural.payloadRef).opaqueRefs.size();
      }
      return structural.fieldSlots.size();
    }

    auto structural_field_name(const NGStructuralObject &structural, size_t index) -> Str
    {
      if (structural.customizedType && index < structural.customizedType->properties.size())
      {
        return structural.customizedType->properties[index];
      }
      return std::to_string(index);
    }

    void sync_structural_slot(const RuntimeRef<StorageCell> &slot, const RuntimeRef<NGObject> &value)
    {
      if (!slot)
      {
        return;
      }
      runtime_sync_storage_cell(slot, value);
    }
  } // namespace


  void NGStructuralObject::sync_payload_backing() const
  {
    auto payloadSize = payloadRef.valid() ? payloadStore.get(payloadRef).opaqueRefs.size() : structural_payload_arity(*this);
    if (!payloadRef.valid())
    {
      payloadRef = payloadStore.allocate(
          buffer_runtime::make_byte_buffer_layout(payloadSize * sizeof(uintptr_t), "Structural.payload"));
    }
    auto &payloadCell = payloadStore.get(payloadRef);
    if (payloadCell.opaqueRefs.size() < payloadSize)
    {
      payloadCell.opaqueRefs.resize(payloadSize);
    }
  }

  void NGStructuralObject::sync_field_slots() const
  {
    sync_payload_backing();
    auto &slots = const_cast<NGStructuralObject *>(this)->fieldSlots;
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
        slots[i]->name = structural_field_name(*this, i);
      }
      payloadRefs[i] = slots[i];
    }
  }

  auto NGStructuralObject::payload_cell() const -> CellRef
  {
    sync_payload_backing();
    return payloadRef;
  }

  auto NGStructuralObject::payload_fields() const -> Vec<RuntimeRef<NGObject>>
  {
    sync_payload_backing();
    sync_field_slots();
    Vec<RuntimeRef<NGObject>> values;
    values.reserve(fieldSlots.size());
    for (const auto &slot : fieldSlots)
    {
      values.push_back(slot ? slot->boxedValue : makert<NGUnit>());
    }
    payloadStore.get(payloadRef).opaqueRefs.assign(fieldSlots.begin(), fieldSlots.end());
    return values;
  }

  void NGStructuralObject::replace_payload_fields(const Vec<RuntimeRef<NGObject>> &values)
  {
    if (!payloadRef.valid() || payloadStore.get(payloadRef).opaqueRefs.size() != values.size())
    {
      payloadRef = payloadStore.allocate(
          buffer_runtime::make_byte_buffer_layout(values.size() * sizeof(uintptr_t), "Structural.payload"));
    }
    auto &payloadCell = payloadStore.get(payloadRef);
    fieldSlots.resize(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
      if (!fieldSlots[i])
      {
        fieldSlots[i] = make_boxed_storage_cell(values[i], StorageClass::TEMPORARY);
        fieldSlots[i]->name = structural_field_name(*this, i);
      }
      sync_structural_slot(fieldSlots[i], values[i]);
    }
    payloadCell.opaqueRefs.assign(fieldSlots.begin(), fieldSlots.end());
  }

  auto NGStructuralObject::field_slot(size_t index) const -> RuntimeRef<StorageCell>
  {
    sync_field_slots();
    if (index >= fieldSlots.size())
    {
      return nullptr;
    }
    return fieldSlots[index];
  }

} // namespace NG::runtime
