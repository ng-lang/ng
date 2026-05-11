#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

#include <optional>

namespace NG::runtime
{
  template <class T>
  inline auto runtime_self_alias(T *self) -> RuntimeRef<NGObject>
  {
    return RuntimeRef<NGObject>(self, [](NGObject *) {});
  }

  inline auto runtime_value_type(const RuntimeRef<NGObject> &value) -> RuntimeRef<NGType>
  {
    if (!value)
    {
      return NGObject::objectType();
    }
    if (auto newType = std::dynamic_pointer_cast<NGNewType>(value))
    {
      return newType->newType ? newType->newType : NGObject::objectType();
    }
    if (std::dynamic_pointer_cast<NGReference>(value))
    {
      return NGReference::referenceType();
    }
    if (std::dynamic_pointer_cast<NGMovedObject>(value))
    {
      return NGMovedObject::movedType();
    }
    if (std::dynamic_pointer_cast<NGBoolean>(value))
    {
      return NGBoolean::booleanType();
    }
    if (std::dynamic_pointer_cast<NGString>(value))
    {
      return NGString::stringType();
    }
    if (std::dynamic_pointer_cast<NGArray>(value))
    {
      return NGArray::arrayType();
    }
    if (std::dynamic_pointer_cast<NGTuple>(value))
    {
      return NGTuple::tupleType();
    }
    if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(value))
    {
      return structural_runtime_type(*structural);
    }
    if (std::dynamic_pointer_cast<NGUnit>(value))
    {
      return NGUnit::unitType();
    }
    if (auto tagged = std::dynamic_pointer_cast<NGTaggedValue>(value))
    {
      return tagged_runtime_type(*tagged);
    }
    if (std::dynamic_pointer_cast<NGModule>(value))
    {
      return NGModule::moduleType();
    }
    if (auto numeral = std::dynamic_pointer_cast<NumeralBase>(value))
    {
      return makert<NGType>(NGType{
          .name = numeral->floating_point() ? "Float" : "Int",
          .showHandler =
              [](const NGSelf &self) {
                auto numeral = std::dynamic_pointer_cast<NumeralBase>(self);
                return numeral ? numeral_runtime_show(*numeral) : Str{"0"};
              },
          .boolHandler =
              [](const NGSelf &self) {
                auto numeral = std::dynamic_pointer_cast<NumeralBase>(self);
                return numeral && numeral_runtime_bool(*numeral);
              },
      });
    }
    return NGObject::objectType();
  }

  inline auto runtime_value_layout(const RuntimeRef<NGObject> &value) -> TypeLayout
  {
    if (auto valueType = runtime_value_type(value); valueType)
    {
      auto layout = valueType->layout;
      if (layout.name.empty())
      {
        layout.name = valueType->name;
      }
      return layout;
    }
    return TypeLayout{};
  }

  inline auto runtime_value_type(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<NGType>
  {
    if (cell && cell->runtimeType)
    {
      return cell->runtimeType;
    }
    if (cell && cell->boxedValue)
    {
      return runtime_value_type(cell->boxedValue);
    }
    if (cell)
    {
      return makert<NGType>(NGType{
          .name = cell->layout.name.empty() ? "Object" : cell->layout.name,
          .layout = cell->layout,
      });
    }
    return NGObject::objectType();
  }

  inline auto runtime_value_layout(const RuntimeRef<StorageCell> &cell) -> TypeLayout
  {
    if (cell &&
        (cell->layout.id != 0 || !cell->layout.name.empty() || cell->layout.size != 0 || !cell->layout.fields.empty() ||
         !cell->layout.variants.empty()))
    {
      return cell->layout;
    }
    if (cell && cell->runtimeType)
    {
      auto layout = cell->runtimeType->layout;
      if (layout.name.empty())
      {
        layout.name = cell->runtimeType->name;
      }
      return layout;
    }
    if (cell && cell->boxedValue)
    {
      return runtime_value_layout(cell->boxedValue);
    }
    return cell ? cell->layout : TypeLayout{};
  }

  inline void runtime_sync_storage_cell(const RuntimeRef<StorageCell> &cell, const RuntimeRef<NGObject> &value,
                                        const RuntimeRef<NGType> &runtimeType = nullptr)
  {
    if (!cell)
    {
      return;
    }

    auto effectiveType = runtimeType ? runtimeType : (value ? runtime_value_type(value) : cell->runtimeType);
    auto effectiveLayout = value ? runtime_value_layout(value) : cell->layout;
    if ((effectiveLayout.id == 0 && effectiveLayout.name.empty()) && effectiveType)
    {
      effectiveLayout = effectiveType->layout;
    }
    if (effectiveLayout.name.empty() && effectiveType)
    {
      effectiveLayout.name = effectiveType->name;
    }

    cell->boxedValue = value;
    cell->runtimeType = effectiveType;
    cell->layout = effectiveLayout;
    cell->bytes.resize(cell->layout.size);
  }

  inline auto runtime_value_show(const RuntimeRef<NGObject> &value) -> Str
  {
    if (auto newType = std::dynamic_pointer_cast<NGNewType>(value); newType && !newType->newType->showHandler)
    {
      return runtime_value_show(newType->wrapped);
    }
    if (auto valueType = runtime_value_type(value); valueType && valueType->showHandler)
    {
      return valueType->showHandler(value ? value : makert<NGUnit>());
    }
    return "unit";
  }

  inline auto runtime_value_show(const RuntimeRef<StorageCell> &cell) -> Str
  {
    if (cell)
    {
      auto self = cell->boxedValue ? cell->boxedValue : makert<NGUnit>();
      if (auto valueType = runtime_value_type(cell); valueType && valueType->showHandler)
      {
        return valueType->showHandler(self);
      }
    }
    return cell ? runtime_value_show(cell->boxedValue) : "unit";
  }

  inline auto runtime_value_bool(const RuntimeRef<NGObject> &value) -> bool
  {
    if (auto newType = std::dynamic_pointer_cast<NGNewType>(value); newType && !newType->newType->boolHandler)
    {
      return runtime_value_bool(newType->wrapped);
    }
    if (auto valueType = runtime_value_type(value); valueType && valueType->boolHandler)
    {
      return valueType->boolHandler(value ? value : makert<NGUnit>());
    }
    return false;
  }

  inline auto runtime_value_bool(const RuntimeRef<StorageCell> &cell) -> bool
  {
    if (cell)
    {
      auto self = cell->boxedValue ? cell->boxedValue : makert<NGUnit>();
      if (auto valueType = runtime_value_type(cell); valueType && valueType->boolHandler)
      {
        return valueType->boolHandler(self);
      }
    }
    return cell && runtime_value_bool(cell->boxedValue);
  }

  inline auto runtime_dispatch_member(const RuntimeRef<NGType> &type, const NGSelf &self, const Str &member,
                                      const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject>
  {
    if (!type)
    {
      return nullptr;
    }
    if (type->respondHandler)
    {
      if (auto result = type->respondHandler(self, member, context, args))
      {
        return result;
      }
    }
    if (!type->memberFunctions.contains(member))
    {
      return nullptr;
    }
    auto dispatchContext = context ? context->fork() : makert<NGContext>();
    dispatchContext->define("self", self ? self : makert<NGUnit>());
    auto result = type->memberFunctions.at(member)(self, dispatchContext, args);
    return result ? result : makert<NGUnit>();
  }

  inline auto runtime_value_respond(const RuntimeRef<NGObject> &value, const Str &member, const NGCtx &context,
                                    const NGArgs &args) -> RuntimeRef<NGObject>
  {
    if (!value)
    {
      throw RuntimeException("Cannot respond to member '" + member + "' on unit");
    }
    if (auto result = runtime_dispatch_member(runtime_value_type(value), value, member, context, args))
    {
      return result;
    }
    if (auto newType = std::dynamic_pointer_cast<NGNewType>(value))
    {
      return runtime_value_respond(newType->wrapped, member, context, args);
    }
    throw NotImplementedException("Not implemented " + runtime_value_type(value)->name + "#" + member);
  }

  inline auto runtime_value_respond(const RuntimeRef<StorageCell> &cell, const Str &member, const NGCtx &context,
                                    const NGArgs &args) -> RuntimeRef<NGObject>
  {
    if (!cell)
    {
      throw RuntimeException("Cannot respond to member '" + member + "' on null storage cell");
    }
    if (auto result = runtime_dispatch_member(runtime_value_type(cell), cell->boxedValue, member, context, args))
    {
      return result;
    }
    return runtime_value_respond(cell->boxedValue, member, context, args);
  }
} // namespace NG::runtime
