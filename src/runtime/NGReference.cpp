#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

namespace NG::runtime
{
  auto NGMovedObject::movedType() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{.name = "moved"});
    return type;
  }

  auto NGMovedObject::type() const -> RuntimeRef<NGType> { return movedType(); }

  auto NGMovedObject::show() const -> Str { return "<moved>"; }

  auto NGMovedObject::boolValue() const -> bool { return false; }

  NGReference::NGReference(Getter getter, Setter setter, Str debugName, MarkHook markHook)
      : getter(std::move(getter)), setter(std::move(setter)), markHook(std::move(markHook)), debugName(std::move(debugName))
  {
  }

  auto NGReference::referenceType() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{.name = "ref"});
    return type;
  }

  auto NGReference::type() const -> RuntimeRef<NGType> { return referenceType(); }

  auto NGReference::show() const -> Str { return "ref(" + debugName + ")"; }

  auto NGReference::boolValue() const -> bool { return true; }

  auto NGReference::read() const -> RuntimeRef<NGObject> { return getter(); }

  void NGReference::write(const RuntimeRef<NGObject> &value) const { setter(value); }

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
      return makert<NGString>(str->value);
    }
    if (auto array = std::dynamic_pointer_cast<NGArray>(value))
    {
      Vec<RuntimeRef<NGObject>> items;
      items.reserve(array->items->size());
      for (auto &item : *array->items)
      {
        items.push_back(clone_value(item));
      }
      return makert<NGArray>(items);
    }
    if (auto tuple = std::dynamic_pointer_cast<NGTuple>(value))
    {
      Vec<RuntimeRef<NGObject>> items;
      items.reserve(tuple->items->size());
      for (auto &item : *tuple->items)
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
      for (auto &[name, prop] : structural->properties)
      {
        cloned->properties[name] = clone_value(prop);
      }
      cloned->fields.reserve(structural->fields.size());
      for (auto &field : structural->fields)
      {
        cloned->fields.push_back(clone_value(field));
      }
      return cloned;
    }
    if (auto newType = std::dynamic_pointer_cast<NGNewType>(value))
    {
      return makert<NGNewType>(newType->newType, clone_value(newType->wrapped));
    }
    if (auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(value))
    {
      Vec<RuntimeRef<NGObject>> payload;
      payload.reserve(tagged->payload.size());
      for (auto &item : tagged->payload)
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
