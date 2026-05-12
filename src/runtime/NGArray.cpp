#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto make_runtime_array_cell(const Vec<RuntimeRef<StorageCell>> &slots, size_t capacityHint,
                               StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto type = array_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, {}, type);
    cell->bytes.resize(type->layout.size);
    cell->opaqueRefs.assign(slots.begin(), slots.end());
    cell->namedRefs.clear();
    cell->nativeHandles.clear();
    cell->initialized = true;
    (void) capacityHint;
    return cell;
  }

  auto runtime_is_array_value(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return type && type->name == "Array";
  }

  auto runtime_array_length(const RuntimeRef<StorageCell> &cell) -> size_t
  {
    if (runtime_is_array_value(cell))
    {
      return runtime_cell_slot_refs(cell).size();
    }
    throw RuntimeException("Expected Array runtime value");
  }

  auto runtime_array_slots(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>
  {
    if (runtime_is_array_value(cell))
    {
      return runtime_cell_slot_refs(cell);
    }
    throw RuntimeException("Expected Array runtime value");
  }

  auto array_runtime_type() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> arrayType = makert<NGType>(NGType{
        .name = "Array",
        .layout = buffer_runtime::make_array_header_layout(),
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              Str result{};
              for (const auto &slot : runtime_cell_slot_refs(cell))
              {
                if (!result.empty())
                {
                  result += ", ";
                }
                result += runtime_value_show(slot);
              }
              return "[" + result + "]";
            },
        .boolCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return !runtime_cell_slot_refs(cell).empty();
            },
        .cellBinaryOperators =
            {
                {RuntimeBinaryOperator::LShift,
                 [](const RuntimeRef<StorageCell> &self,
                    const RuntimeRef<StorageCell> &other) -> RuntimeRef<StorageCell> {
                   auto slots = runtime_cell_slot_refs(self);
                   auto appended = make_storage_cell(other ? other->layout : TypeLayout{}, StorageClass::TEMPORARY,
                                                     std::to_string(slots.size()),
                                                     other ? other->runtimeType : nullptr);
                   runtime_copy_storage_cell(appended, other);
                   appended->name = std::to_string(slots.size());
                   slots.push_back(appended);
                   return make_runtime_array_cell(slots, slots.size());
                 }},
            },
    });
    return arrayType;
  }
} // namespace NG::runtime
