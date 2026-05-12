#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto make_runtime_tuple_cell(const Vec<RuntimeRef<StorageCell>> &slots,
                               StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto type = tuple_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, {}, type);
    cell->bytes.resize(type->layout.size);
    cell->opaqueRefs.assign(slots.begin(), slots.end());
    cell->namedRefs.clear();
    cell->nativeHandles.clear();
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_tuple_value(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return type && type->name == "Tuple";
  }

  auto runtime_tuple_length(const RuntimeRef<StorageCell> &cell) -> size_t
  {
    if (runtime_is_tuple_value(cell))
    {
      return runtime_cell_slot_refs(cell).size();
    }
    throw RuntimeException("Expected Tuple runtime value");
  }

  auto runtime_tuple_slots(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>
  {
    if (runtime_is_tuple_value(cell))
    {
      return runtime_cell_slot_refs(cell);
    }
    throw RuntimeException("Expected Tuple runtime value");
  }

  auto tuple_runtime_type() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> tupleType = makert<NGType>(NGType{
        .name = "Tuple",
        .layout = TypeLayout{.name = "Tuple", .kind = LayoutKind::DYNAMIC},
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
              return "(" + result + ")";
            },
        .boolCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return !runtime_cell_slot_refs(cell).empty();
            },
        .respondCellHandler =
            [](const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &,
               const NGArgs &) -> RuntimeRef<StorageCell> {
          if (member == "size")
          {
            return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(runtime_cell_slot_refs(cell).size()));
          }
          try
          {
            return runtime_cell_slot_ref(cell, std::stoul(member));
          }
          catch (const std::exception &)
          {
            return nullptr;
          }
        },
    });
    return tupleType;
  }
} // namespace NG::runtime
