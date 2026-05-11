
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/array_layout_access.hpp>
#include <runtime/value_access.hpp>
namespace NG::runtime
{
  namespace
  {
    auto array_runtime_show_impl(const NGArray &array) -> Str
    {
      Str result{};

      for (const auto &item : array.payload_items())
      {
        if (!result.empty())
        {
          result += ", ";
        }

        result += runtime_value_show(item);
      }

      return "[" + result + "]";
    }

    void sync_array_slot(const RuntimeRef<StorageCell> &slot, const RuntimeRef<NGObject> &value)
    {
      if (!slot)
      {
        return;
      }
      runtime_sync_storage_cell(slot, value);
    }
  } // namespace

  NGArray::NGArray(const Vec<RuntimeRef<NGObject>> &vec)
  {
    payloadCapacity = vec.capacity();
    replace_payload_items(vec, payloadCapacity);
  }

  auto NGArray::arrayType() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> arrayType = makert<NGType>(NGType{
      .name = "Array",
      .layout = buffer_runtime::make_array_header_layout(),
      .showHandler =
          [](const NGSelf &self) {
            auto array = std::dynamic_pointer_cast<NGArray>(self);
            return array ? array_runtime_show_impl(*array) : Str{"[]"};
          },
      .boolHandler =
          [](const NGSelf &self) {
            auto array = std::dynamic_pointer_cast<NGArray>(self);
            return array && array_length(*array) != 0;
          },
    });
    return arrayType;
  }

  void NGArray::sync_header_backing() const
  {
    auto payloadSize = payloadRef.valid() ? headerStore.get(payloadRef).opaqueRefs.size() : size_t{0};
    if (!headerRef.valid())
    {
      headerRef = buffer_runtime::allocate_array_header(headerStore, payloadSize, std::max(payloadCapacity, payloadSize));
    }
    if (!payloadRef.valid())
    {
      payloadRef = headerStore.allocate(buffer_runtime::make_byte_buffer_layout(0, "Array.payload"));
    }
    auto &payloadCell = headerStore.get(payloadRef);
    payloadSize = payloadCell.opaqueRefs.size();
    payloadCapacity = std::max(payloadCapacity, payloadSize);

    const auto &layout = headerStore.get(headerRef).layout;
    buffer_runtime::write_u64_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "length"), payloadSize);
    buffer_runtime::write_u64_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "capacity"), payloadCapacity);
    buffer_runtime::write_native_handle_field(
        headerStore, headerRef, *buffer_runtime::find_field(layout, "data"),
        NativeHandle{
            .typeName = "Array.payload",
            .address = reinterpret_cast<uintptr_t>(payloadCell.bytes.data()),
            .owning = false,
        });
  }

  void NGArray::sync_element_slots() const
  {
    sync_header_backing();
    auto &slots = const_cast<NGArray *>(this)->elementSlots;
    auto &payloadRefs = headerStore.get(payloadRef).opaqueRefs;
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

  auto NGArray::header_cell() const -> CellRef
  {
    sync_header_backing();
    return headerRef;
  }

  auto NGArray::header_length() const -> uint64_t
  {
    sync_header_backing();
    const auto &layout = headerStore.get(headerRef).layout;
    return buffer_runtime::read_u64_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "length"));
  }

  auto NGArray::header_capacity() const -> uint64_t
  {
    sync_header_backing();
    const auto &layout = headerStore.get(headerRef).layout;
    return buffer_runtime::read_u64_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "capacity"));
  }

  auto NGArray::header_data_handle() const -> NativeHandle
  {
    sync_header_backing();
    const auto &layout = headerStore.get(headerRef).layout;
    return buffer_runtime::read_native_handle_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "data"));
  }

  auto NGArray::payload_cell() const -> CellRef
  {
    sync_header_backing();
    return payloadRef;
  }

  auto NGArray::payload_items() const -> Vec<RuntimeRef<NGObject>>
  {
    sync_header_backing();
    sync_element_slots();
    Vec<RuntimeRef<NGObject>> values;
    values.reserve(elementSlots.size());
    for (const auto &slot : elementSlots)
    {
      values.push_back(slot ? slot->boxedValue : makert<NGUnit>());
    }
    headerStore.get(payloadRef).opaqueRefs.assign(elementSlots.begin(), elementSlots.end());
    return values;
  }

  void NGArray::replace_payload_items(const Vec<RuntimeRef<NGObject>> &values, size_t capacityHint)
  {
    if (!payloadRef.valid() || headerStore.get(payloadRef).opaqueRefs.size() != values.size())
    {
      payloadRef = headerStore.allocate(
          buffer_runtime::make_byte_buffer_layout(values.size() * sizeof(uintptr_t), "Array.payload"));
    }
    auto &payloadCell = headerStore.get(payloadRef);
    payloadCapacity = std::max(capacityHint, values.size());
    elementSlots.resize(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
      if (!elementSlots[i])
      {
        elementSlots[i] = make_boxed_storage_cell(values[i], StorageClass::TEMPORARY);
        elementSlots[i]->name = std::to_string(i);
      }
      sync_array_slot(elementSlots[i], values[i]);
    }
    payloadCell.opaqueRefs.assign(elementSlots.begin(), elementSlots.end());
    sync_header_backing();
  }

  auto NGArray::element_slot(size_t index) const -> RuntimeRef<StorageCell>
  {
    sync_element_slots();
    if (index >= elementSlots.size())
    {
      return nullptr;
    }
    return elementSlots[index];
  }

  auto NGArray::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
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

    return array_read_element(*this, static_cast<size_t>(indexVal));
  }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  auto NGArray::opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
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

    array_write_element(*this, static_cast<size_t>(indexVal), newValue);
    return newValue;
  }

  auto NGArray::opEquals(RuntimeRef<NGObject> other) const -> bool
  {
    sync_header_backing();

    if (auto array = std::dynamic_pointer_cast<NGArray>(other); array != nullptr)
    {
      array->sync_header_backing();
      auto size = array_length(*this);
      if (size != array_length(*array))
      {
        return false;
      }
      for (size_t i = 0; i < size; ++i)
      {
        if (!array_read_element(*this, i)->opEquals(array_read_element(*array, i)))
        {
          return false;
        }
      }
      return true;
    }

    return false;
  }

  auto NGArray::opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
  {
    auto values = payload_items();
    values.push_back(other);
    replace_payload_items(values, std::max(payloadCapacity, values.size()));
    return makert<NGArray>(values);
  }
} // namespace NG::runtime
