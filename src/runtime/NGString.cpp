#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  namespace
  {
    auto string_cell_payload(const RuntimeRef<StorageCell> &cell) -> Str
    {
      return cell ? Str(reinterpret_cast<const char *>(cell->bytes.data()), cell->bytes.size()) : Str{};
    }
  } // namespace

  auto make_runtime_string(Str value, StorageClass storageClass) -> RuntimeRef<StorageCell>
  {
    auto type = string_runtime_type();
    auto cell = make_storage_cell(type->layout, storageClass, {}, type);
    cell->bytes.assign(value.begin(), value.end());
    cell->initialized = true;
    return cell;
  }

  auto runtime_is_string_value(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return type && type->name == "String";
  }

  auto runtime_string_value(const RuntimeRef<StorageCell> &cell) -> Str
  {
    if (runtime_is_string_value(cell))
    {
      return string_cell_payload(cell);
    }
    throw RuntimeException("Expected String runtime value");
  }

  auto string_runtime_type() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> stringType = makert<NGType>(NGType{
        .name = "String",
        .layout = buffer_runtime::make_string_header_layout(),
        .memberFunctions = {
            {"size",
             [](const NGSelf &self, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
               return numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(string_cell_payload(self).size()));
             }},
            {"charAt",
             [](const NGSelf &self, const NGEnv &, const NGArgs &args) -> RuntimeRef<StorageCell> {
               if (args.empty())
               {
                 throw RuntimeException("String.charAt() requires an index argument");
               }
               auto index = read_numeric_cell_as<int32_t>(args[0]);
               if (index < 0)
               {
                 throw RuntimeException("Index out of bounds: " + std::to_string(index));
               }
               auto payload = string_cell_payload(self);
               if (static_cast<size_t>(index) >= payload.size())
               {
                 throw RuntimeException("Index out of bounds: " + std::to_string(index));
               }
               return numeral_cell_from_value<int32_t>(static_cast<unsigned char>(payload[static_cast<size_t>(index)]));
            }},
            {"append",
             [](const NGSelf &self, const NGEnv &, const NGArgs &args) -> RuntimeRef<StorageCell> {
               if (args.empty())
               {
                 throw RuntimeException("String.append() requires a value argument");
               }
               return make_runtime_string(string_cell_payload(self) + runtime_value_show(args[0]));
             }},
        },
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return string_cell_payload(cell);
            },
        .boolCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return !string_cell_payload(cell).empty();
            },
        .cellBinaryOperators =
            {
                {RuntimeBinaryOperator::Add,
                 [](const RuntimeRef<StorageCell> &self,
                    const RuntimeRef<StorageCell> &other) -> RuntimeRef<StorageCell> {
                   return make_runtime_string(string_cell_payload(self) + runtime_value_show(other));
                 }},
            },
        .cellOrderHandler =
            [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) {
              if (!runtime_is_string_value(other))
              {
                return Orders::UNORDERED;
              }
              auto left = string_cell_payload(self);
              auto right = string_cell_payload(other);
              if (left < right) return Orders::LT;
              if (left > right) return Orders::GT;
              return Orders::EQ;
            },
    });
    return stringType;
  }
} // namespace NG::runtime
