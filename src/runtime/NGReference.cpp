#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/array_layout_access.hpp>

namespace NG::runtime
{
  namespace
  {
    auto reference_runtime_show_impl(const NGReference &ref) -> Str
    {
      return "ref(" + ref.debugName + ")";
    }
  } // namespace

  auto NGMovedObject::movedType() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
      .name = "moved",
      .layout = TypeLayout{.name = "moved", .kind = LayoutKind::INLINE_VALUE},
      .showHandler = [](const NGSelf &) { return Str{"<moved>"}; },
      .boolHandler = [](const NGSelf &) { return false; },
    });
    return type;
  }

  NGReference::NGReference(RuntimeRef<StorageCell> targetCell, Str debugName, MarkHook markHook)
      : targetCell(std::move(targetCell)), markHook(std::move(markHook)), debugName(std::move(debugName))
  {
  }

  auto NGReference::referenceType() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
      .name = "ref",
      .layout = buffer_runtime::make_reference_layout("ref"),
      .showHandler =
          [](const NGSelf &self) {
            auto ref = std::dynamic_pointer_cast<NGReference>(self);
            return ref ? reference_runtime_show_impl(*ref) : Str{"ref(?)"};
          },
      .boolHandler = [](const NGSelf &) { return true; },
    });
    return type;
  }

  auto NGReference::read() const -> RuntimeRef<NGObject>
  {
    if (!targetCell)
    {
      throw RuntimeException("Dangling reference cell");
    }
    if (!targetCell->boxedValue)
    {
      throw RuntimeException("Dangling heap reference");
    }
    ensure_usable_value(targetCell->boxedValue);
    return targetCell->boxedValue;
  }

  void NGReference::write(const RuntimeRef<NGObject> &value) const
  {
    if (!targetCell)
    {
      throw RuntimeException("Dangling reference cell");
    }
    if (!targetCell->boxedValue)
    {
      throw RuntimeException("Dangling heap reference");
    }
    runtime_sync_storage_cell(targetCell, value);
  }

  void NGReference::mark_referenced_heap() const
  {
    if (markHook)
    {
      markHook();
    }
  }

  auto moved_object() -> RuntimeRef<NGObject> { return makert<NGMovedObject>(); }

  auto is_moved_object(const RuntimeRef<NGObject> &value) -> bool
  {
    return std::dynamic_pointer_cast<NGMovedObject>(value) != nullptr;
  }

  void ensure_usable_value(const RuntimeRef<NGObject> &value)
  {
    if (is_moved_object(value))
    {
      throw RuntimeException("Use after move");
    }
  }

  static auto clone_numeral(const std::shared_ptr<NumeralBase> &numeral) -> RuntimeRef<NGObject>
  {
    if (auto value = std::dynamic_pointer_cast<NGIntegral<int8_t>>(numeral)) return makert<NGIntegral<int8_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGIntegral<uint8_t>>(numeral)) return makert<NGIntegral<uint8_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGIntegral<int16_t>>(numeral)) return makert<NGIntegral<int16_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGIntegral<uint16_t>>(numeral)) return makert<NGIntegral<uint16_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGIntegral<int32_t>>(numeral)) return makert<NGIntegral<int32_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGIntegral<uint32_t>>(numeral)) return makert<NGIntegral<uint32_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGIntegral<int64_t>>(numeral)) return makert<NGIntegral<int64_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGIntegral<uint64_t>>(numeral)) return makert<NGIntegral<uint64_t>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGFloatingPoint<float>>(numeral)) return makert<NGFloatingPoint<float>>(value->value);
    if (auto value = std::dynamic_pointer_cast<NGFloatingPoint<double>>(numeral)) return makert<NGFloatingPoint<double>>(value->value);
    return nullptr;
  }

  auto clone_value(const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>
  {
    if (!value)
    {
      return nullptr;
    }
    if (is_moved_object(value))
    {
      return value;
    }
    if (std::dynamic_pointer_cast<NGReference>(value) || std::dynamic_pointer_cast<NGModule>(value))
    {
      return value;
    }
    if (auto numeral = std::dynamic_pointer_cast<NumeralBase>(value))
    {
      if (auto cloned = clone_numeral(numeral))
      {
        return cloned;
      }
    }
    if (auto boolean = std::dynamic_pointer_cast<NGBoolean>(value))
    {
      return makert<NGBoolean>(boolean->value);
    }
    if (auto str = std::dynamic_pointer_cast<NGString>(value))
    {
      return makert<NGString>(str->payload_value());
    }
    if (auto array = std::dynamic_pointer_cast<NGArray>(value))
    {
      auto source = array->payload_items();
      Vec<RuntimeRef<NGObject>> items;
      items.reserve(source.size());
      for (auto &item : source)
      {
        items.push_back(clone_value(item));
      }
      return makert<NGArray>(items);
    }
    if (auto tuple = std::dynamic_pointer_cast<NGTuple>(value))
    {
      auto source = tuple->payload_items();
      Vec<RuntimeRef<NGObject>> items;
      items.reserve(source.size());
      for (auto &item : source)
      {
        items.push_back(clone_value(item));
      }
      return makert<NGTuple>(items);
    }
    if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(value))
    {
      auto cloned = makert<NGStructuralObject>();
      cloned->customizedType = structural->customizedType;
      cloned->selfMemberFunctions = structural->selfMemberFunctions;
      for (const auto &[name, slot] : structural->propertySlots)
      {
        auto clonedSlot = cloned->property_slot_or_create(name);
        runtime_sync_storage_cell(clonedSlot, clone_value(slot ? slot->boxedValue : nullptr));
      }
      auto sourceFields = structural->payload_fields();
      Vec<RuntimeRef<NGObject>> clonedFields;
      clonedFields.reserve(sourceFields.size());
      for (auto &field : sourceFields)
      {
        clonedFields.push_back(clone_value(field));
      }
      cloned->replace_payload_fields(clonedFields);
      return cloned;
    }
    if (auto newType = std::dynamic_pointer_cast<NGNewType>(value))
    {
      return makert<NGNewType>(newType->newType, clone_value(newType->wrapped));
    }
    if (auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(value))
    {
      auto sourcePayload = tagged->payload_items();
      Vec<RuntimeRef<NGObject>> payload;
      payload.reserve(sourcePayload.size());
      for (auto &item : sourcePayload)
      {
        payload.push_back(clone_value(item));
      }
      return makert<NGTaggedValue>(tagged->unionName, tagged->variantName, tagged->variantIndex, std::move(payload),
                                   tagged->payloadNames);
    }
    if (std::dynamic_pointer_cast<NGUnit>(value))
    {
      return makert<NGUnit>();
    }
    return value;
  }

  auto materialize_value(const RuntimeRef<NGObject> &value, bool moved) -> RuntimeRef<NGObject>
  {
    if (!value)
    {
      return nullptr;
    }
    if (moved)
    {
      return value;
    }
    return clone_value(value);
  }
} // namespace NG::runtime
