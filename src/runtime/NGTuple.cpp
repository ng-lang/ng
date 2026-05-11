#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/tuple_layout_access.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  namespace
  {
    auto tuple_runtime_show_impl(const NGTuple &tuple) -> Str
    {
      Str result{};

      for (const auto &item : tuple.payload_items())
      {
        if (!result.empty())
        {
          result += ", ";
        }

        result += runtime_value_show(item);
      }

      return "(" + result + ")";
    }

    void sync_tuple_slot(const RuntimeRef<StorageCell> &slot, const RuntimeRef<NGObject> &value)
    {
      if (!slot)
      {
        return;
      }
      runtime_sync_storage_cell(slot, value);
    }
  } // namespace

  NGTuple::NGTuple(const Vec<RuntimeRef<NGObject>> &vec)
  {
    replace_payload_items(vec);
  }

  void NGTuple::sync_payload_backing() const
  {
    if (!payloadRef.valid())
    {
      payloadRef = payloadStore.allocate(buffer_runtime::make_byte_buffer_layout(0, "Tuple.payload"));
    }
  }

  void NGTuple::sync_element_slots() const
  {
    sync_payload_backing();
    auto &slots = const_cast<NGTuple *>(this)->elementSlots;
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
        slots[i]->name = std::to_string(i);
      }
      payloadRefs[i] = slots[i];
    }
  }

  auto NGTuple::payload_cell() const -> CellRef
  {
    sync_payload_backing();
    return payloadRef;
  }

  auto NGTuple::payload_items() const -> Vec<RuntimeRef<NGObject>>
  {
    sync_payload_backing();
    sync_element_slots();
    Vec<RuntimeRef<NGObject>> values;
    values.reserve(elementSlots.size());
    for (const auto &slot : elementSlots)
    {
      values.push_back(slot ? slot->boxedValue : makert<NGUnit>());
    }
    payloadStore.get(payloadRef).opaqueRefs.assign(elementSlots.begin(), elementSlots.end());
    return values;
  }

  void NGTuple::replace_payload_items(const Vec<RuntimeRef<NGObject>> &values)
  {
    if (!payloadRef.valid() || payloadStore.get(payloadRef).opaqueRefs.size() != values.size())
    {
      payloadRef = payloadStore.allocate(
          buffer_runtime::make_byte_buffer_layout(values.size() * sizeof(uintptr_t), "Tuple.payload"));
    }
    auto &payloadCell = payloadStore.get(payloadRef);
    elementSlots.resize(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
      if (!elementSlots[i])
      {
        elementSlots[i] = make_boxed_storage_cell(values[i], StorageClass::TEMPORARY);
        elementSlots[i]->name = std::to_string(i);
      }
      sync_tuple_slot(elementSlots[i], values[i]);
    }
    payloadCell.opaqueRefs.assign(elementSlots.begin(), elementSlots.end());
  }

  auto NGTuple::element_slot(size_t index) const -> RuntimeRef<StorageCell>
  {
    sync_element_slots();
    if (index >= elementSlots.size())
    {
      return nullptr;
    }
    return elementSlots[index];
  }


  auto NGTuple::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
  {
    auto ngInt = std::dynamic_pointer_cast<NumeralBase>(index);
    if (ngInt == nullptr)
    {
      throw IllegalTypeException("Not a valid index");
    }
    auto indexVal = NGIntegral<int32_t>::valueOf(ngInt.get());
    if (indexVal < 0)
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(indexVal));
    }
    return tuple_read_element(*this, static_cast<size_t>(indexVal));
  };

  auto NGTuple::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
  {
    auto ngInt = std::dynamic_pointer_cast<NumeralBase>(index);
    if (ngInt == nullptr)
    {
      throw IllegalTypeException("Not a valid index");
    }
    auto indexVal = NGIntegral<int32_t>::valueOf(ngInt.get());
    if (indexVal < 0)
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(indexVal));
    }
    tuple_write_element(*this, static_cast<size_t>(indexVal), newValue);
    return newValue;
  };

  auto NGTuple::opEquals(RuntimeRef<NGObject> other) const -> bool
  {
    if (this == other.get())
    {
      return true;
    }
    auto otherTuple = std::dynamic_pointer_cast<NGTuple>(other);
    if (!otherTuple)
    {
      return false;
    }
    auto values = payload_items();
    auto otherValues = otherTuple->payload_items();
    if (values.size() != otherValues.size())
    {
      return false;
    }
    for (size_t i = 0; i < values.size(); ++i)
    {
      if (!values[i]->opEquals(otherValues[i]))
      {
        return false;
      }
    }
    return true;
  };

  auto NGTuple::tupleType() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> tupleType = makert<NGType>(NGType{
      .name = "Tuple",
      .layout = TypeLayout{.name = "Tuple", .kind = LayoutKind::DYNAMIC},
      .memberFunctions = {},
      .showHandler =
          [](const NGSelf &self) {
            auto tuple = std::dynamic_pointer_cast<NGTuple>(self);
            return tuple ? tuple_runtime_show_impl(*tuple) : Str{"()"};
          },
      .boolHandler =
          [](const NGSelf &self) {
            auto tuple = std::dynamic_pointer_cast<NGTuple>(self);
            return tuple && !tuple->payload_items().empty();
          },
      .respondHandler =
          [](const NGSelf &self, const Str &member, const NGEnv &, const NGArgs &) -> RuntimeRef<NGObject> {
            auto tuple = std::dynamic_pointer_cast<NGTuple>(self);
            if (!tuple)
            {
              return nullptr;
            }
            return tuple_read_member(*tuple, member);
          },
    });
    return tupleType;
  };

} // namespace NG::runtime
