#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/array_layout_access.hpp>
#include <runtime/tuple_layout_access.hpp>
#include <runtime/value_access.hpp>

#include <cstring>

namespace NG::runtime
{
  namespace
  {
    auto clone_sequence_slot(const RuntimeRef<StorageCell> &slot, Str name) -> RuntimeRef<StorageCell>
    {
      return clone_runtime_storage_cell(slot, StorageClass::TEMPORARY, std::move(name));
    }

    auto numeral_cell_like(const RuntimeRef<StorageCell> &prototype, int64_t value) -> RuntimeRef<StorageCell>
    {
      auto type = prototype ? prototype->runtimeType : nullptr;
      auto name = type ? type->name : Str{};
      if (name == "i8") return numeral_cell_from_value<int8_t>(static_cast<int8_t>(value));
      if (name == "u8") return numeral_cell_from_value<uint8_t>(static_cast<uint8_t>(value));
      if (name == "i16") return numeral_cell_from_value<int16_t>(static_cast<int16_t>(value));
      if (name == "u16") return numeral_cell_from_value<uint16_t>(static_cast<uint16_t>(value));
      if (name == "i32" || name == "int") return numeral_cell_from_value<int32_t>(static_cast<int32_t>(value));
      if (name == "u32" || name == "uint") return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(value));
      if (name == "i64") return numeral_cell_from_value<int64_t>(value);
      if (name == "u64") return numeral_cell_from_value<uint64_t>(static_cast<uint64_t>(value));
      throw RuntimeException("Range bound is not an integral numeric cell");
    }
  }

  auto from_end_index_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "FromEndIndex",
        .layout = TypeLayout{.name = "FromEndIndex", .kind = LayoutKind::INLINE_VALUE, .size = sizeof(int32_t)},
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return "^" + std::to_string(runtime_from_end_index_value(cell));
            },
        .boolCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return runtime_from_end_index_value(cell) != 0;
            },
    });
    return type;
  }

  auto make_runtime_from_end_index(int32_t value, StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto type = from_end_index_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, {}, type);
    cell->bytes.resize(sizeof(int32_t));
    std::memcpy(cell->bytes.data(), &value, sizeof(int32_t));
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_from_end_index(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return type && type->name == "FromEndIndex";
  }

  auto runtime_from_end_index_value(const RuntimeRef<StorageCell> &cell) -> int32_t
  {
    if (!runtime_is_from_end_index(cell) || cell->bytes.size() < sizeof(int32_t))
    {
      throw RuntimeException("Expected from-end index runtime value");
    }
    int32_t value = 0;
    std::memcpy(&value, cell->bytes.data(), sizeof(int32_t));
    return value;
  }

  auto range_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "Range",
        .layout = TypeLayout{.name = "Range", .kind = LayoutKind::DYNAMIC},
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              auto slots = runtime_range_slots(cell);
              Str out;
              for (const auto &slot : slots)
              {
                if (!out.empty()) out += ", ";
                out += runtime_value_show(slot);
              }
              return "Range(" + out + ")";
            },
        .boolCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return !runtime_range_slots(cell).empty();
            },
        .memberFunctions =
            {
                {"size",
                 [](const NGSelf &self, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
                   return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(runtime_range_slots(self).size()));
                 }},
                {"get",
                 [](const NGSelf &self, const NGEnv &, const NGArgs &args) -> RuntimeRef<StorageCell> {
                   if (args.size() != 1)
                   {
                     throw RuntimeException("Range.get expects one index argument");
                   }
                   auto index = read_numeric_cell_as<int32_t>(args[0]);
                   if (index < 0)
                   {
                     throw RuntimeException("Range.get index out of bounds: " + std::to_string(index));
                   }
                   return runtime_sequence_slot(self, static_cast<size_t>(index));
                 }},
            },
    });
    return type;
  }

  auto make_runtime_range_cell(const RuntimeRef<StorageCell> &start, const RuntimeRef<StorageCell> &end,
                               bool inclusive, StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto type = range_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, {}, type);
    cell->opaqueRefs = {clone_sequence_slot(start, "start"), clone_sequence_slot(end, "end")};
    cell->bytes = {static_cast<uint8_t>(inclusive ? 1 : 0)};
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_range_value(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return type && type->name == "Range";
  }

  auto runtime_range_slots(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>
  {
    if (!runtime_is_range_value(cell) || cell->opaqueRefs.size() != 2)
    {
      throw RuntimeException("Expected Range runtime value");
    }
    auto start = read_numeric_cell_as<int64_t>(cell->opaqueRefs[0]);
    auto end = read_numeric_cell_as<int64_t>(cell->opaqueRefs[1]);
    if (!cell->bytes.empty() && cell->bytes[0] != 0)
    {
      end += start <= end ? 1 : -1;
    }
    const int64_t step = start <= end ? 1 : -1;
    Vec<RuntimeRef<StorageCell>> result;
    for (int64_t value = start; step > 0 ? value < end : value > end; value += step)
    {
      result.push_back(numeral_cell_like(cell->opaqueRefs[0], value));
    }
    return result;
  }

  auto span_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "Span",
        .layout = TypeLayout{.name = "Span", .kind = LayoutKind::DYNAMIC},
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              Str out;
              for (const auto &slot : runtime_span_slots(cell))
              {
                if (!out.empty()) out += ", ";
                out += runtime_value_show(slot);
              }
              return "span[" + out + "]";
            },
        .boolCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return !runtime_span_slots(cell).empty();
            },
        .memberFunctions =
            {
                {"size",
                 [](const NGSelf &self, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
                   return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(runtime_span_slots(self).size()));
                 }},
                {"get",
                 [](const NGSelf &self, const NGEnv &, const NGArgs &args) -> RuntimeRef<StorageCell> {
                   if (args.size() != 1)
                   {
                     throw RuntimeException("Span.get expects one index argument");
                   }
                   auto index = read_numeric_cell_as<int32_t>(args[0]);
                   if (index < 0)
                   {
                     throw RuntimeException("Span.get index out of bounds: " + std::to_string(index));
                   }
                   return runtime_sequence_slot(self, static_cast<size_t>(index));
                 }},
            },
    });
    return type;
  }

  auto make_runtime_span_cell(const Vec<RuntimeRef<StorageCell>> &slots, StorageClass storageClass)
      -> RuntimeRef<StorageCell>
  {
    auto type = span_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, {}, type);
    cell->opaqueRefs.assign(slots.begin(), slots.end());
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_span_value(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return type && type->name == "Span";
  }

  auto runtime_span_slots(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>
  {
    if (!runtime_is_span_value(cell))
    {
      throw RuntimeException("Expected Span runtime value");
    }
    return runtime_cell_slot_refs(cell);
  }

  auto runtime_builtin_sequence_slots(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>
  {
    if (runtime_is_tuple_value(cell)) return runtime_tuple_slots(cell);
    if (runtime_is_array_value(cell)) return runtime_array_slots(cell);
    if (runtime_is_range_value(cell)) return runtime_range_slots(cell);
    if (runtime_is_span_value(cell)) return runtime_span_slots(cell);
    throw RuntimeException("Expected Sequence-compatible runtime value");
  }

  auto runtime_sequence_length(const RuntimeRef<StorageCell> &cell) -> size_t
  {
    if (runtime_is_tuple_value(cell)) return runtime_tuple_length(cell);
    if (runtime_is_array_value(cell)) return runtime_array_length(cell);
    return runtime_builtin_sequence_slots(cell).size();
  }

  auto runtime_sequence_slot(const RuntimeRef<StorageCell> &cell, size_t index) -> RuntimeRef<StorageCell>
  {
    if (runtime_is_tuple_value(cell)) return tuple_element_slot(cell, index);
    if (runtime_is_array_value(cell)) return array_element_slot(cell, index);

    auto slots = runtime_builtin_sequence_slots(cell);
    if (index >= slots.size())
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    return slots[index];
  }
} // namespace NG::runtime
