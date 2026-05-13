#include <intp/runtime.hpp>
#include <runtime/struct_layout_access.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  namespace
  {
    void ensure_structural_cell_handlers(const RuntimeRef<NGType> &type)
    {
      if (!type)
      {
        return;
      }
      if (!type->showCellHandler)
      {
        type->showCellHandler = [](const RuntimeRef<StorageCell> &cell) {
          Str repr{};
          auto valueType = runtime_value_type(cell);
          auto slots = runtime_cell_slot_refs(cell);
          if (valueType && !valueType->properties.empty())
          {
            for (size_t i = 0; i < valueType->properties.size(); ++i)
            {
              auto value = i < slots.size() ? slots[i] : nullptr;
              if (!value)
              {
                continue;
              }
              if (!repr.empty())
              {
                repr += ", ";
              }
              repr += valueType->properties[i] + ": " + runtime_value_show(value);
            }
          }
          for (const auto &[name, slot] : runtime_cell_named_slot_refs(cell))
          {
            if (!repr.empty())
            {
              repr += ", ";
            }
            repr += name + ": " + runtime_value_show(slot);
          }
          return "{ " + repr + " }";
        };
      }
      if (!type->boolCellHandler)
      {
        type->boolCellHandler = [](const RuntimeRef<StorageCell> &) { return true; };
      }
      if (!type->respondCellHandler)
      {
        type->respondCellHandler =
            [](const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
          return structural_member_slot(cell, member);
        };
      }
    }
  } // namespace

  auto make_runtime_structural_cell(const RuntimeRef<NGType> &type,
                                    const Vec<RuntimeRef<StorageCell>> &fields,
                                    const Map<Str, RuntimeRef<StorageCell>> &properties,
                                    StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    ensure_structural_cell_handlers(type);
    auto cell = make_storage_cell(type ? type->layout : TypeLayout{}, storageClass, {}, type);
    cell->runtimeType = type;
    cell->layout = type ? type->layout : TypeLayout{};
    cell->bytes.resize(cell->layout.size);
    cell->opaqueRefs.assign(fields.begin(), fields.end());
    cell->namedRefs.clear();
    for (const auto &[name, slot] : properties)
    {
      cell->namedRefs.insert_or_assign(name, slot);
    }
    cell->nativeHandles.clear();
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_structural_value(const RuntimeRef<StorageCell> &value) -> bool
  {
    if (!value)
    {
      return false;
    }
    auto type = runtime_value_type(value);
    if (type && (type->name == "Array" || type->name == "Tuple" || type->name == "String" || type->name == "Module" ||
                 type->name == "ref"))
    {
      return false;
    }
    return (type && (!type->properties.empty() || type->layout.kind == LayoutKind::DYNAMIC)) ||
           !runtime_cell_named_slot_refs(value).empty();
  }

  auto runtime_structural_type(const RuntimeRef<StorageCell> &value) -> RuntimeRef<NGType>
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    auto type = runtime_value_type(value);
    ensure_structural_cell_handlers(type);
    return type;
  }

  auto runtime_structural_field_slots(const RuntimeRef<StorageCell> &value) -> Vec<RuntimeRef<StorageCell>>
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    return runtime_cell_slot_refs(value);
  }

  auto runtime_structural_field_slot(const RuntimeRef<StorageCell> &value, size_t index) -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    return runtime_cell_slot_ref(value, index);
  }

  auto runtime_structural_field_index(const RuntimeRef<StorageCell> &value, const Str &name) -> std::optional<size_t>
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    return structural_field_index(value, name);
  }

  auto runtime_structural_property_slots(const RuntimeRef<StorageCell> &value) -> Map<Str, RuntimeRef<StorageCell>>
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    return runtime_cell_named_slot_refs(value);
  }

  auto runtime_structural_property_slot(const RuntimeRef<StorageCell> &value, const Str &name)
      -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    return structural_member_slot(value, name);
  }

  auto runtime_structural_property_slot_or_create(const RuntimeRef<StorageCell> &value, const Str &name)
      -> RuntimeRef<StorageCell>
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    return structural_member_slot_or_create(value, name);
  }

  auto runtime_structural_read_member(const RuntimeRef<StorageCell> &value, const Str &member)
      -> RuntimeRef<StorageCell>
  {
    return structural_member_slot(value, member);
  }

  void runtime_structural_write_member(const RuntimeRef<StorageCell> &value, const Str &member,
                                       const RuntimeRef<StorageCell> &nextValue)
  {
    structural_write_member(value, member, nextValue);
  }

  void runtime_structural_replace_field_slots(const RuntimeRef<StorageCell> &value,
                                              const Vec<RuntimeRef<StorageCell>> &slots)
  {
    if (!runtime_is_structural_value(value))
    {
      throw RuntimeException("Expected structural runtime value");
    }
    value->opaqueRefs.assign(slots.begin(), slots.end());
  }
} // namespace NG::runtime
