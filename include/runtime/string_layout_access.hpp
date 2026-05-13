#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

namespace NG::runtime
{
  inline auto string_length(const RuntimeRef<StorageCell> &string) -> size_t
  {
    return runtime_string_value(string).size();
  }

  inline auto string_data_handle(const RuntimeRef<StorageCell> &string) -> NativeHandle
  {
    auto payload = runtime_string_value(string);
    return NativeHandle{
        .typeName = "String.payload",
        .address = reinterpret_cast<uintptr_t>(payload.data()),
        .owning = false,
    };
  }

  inline auto string_read_code_unit(const RuntimeRef<StorageCell> &string, size_t index) -> RuntimeRef<StorageCell>
  {
    if (index >= string_length(string))
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    auto payload = runtime_string_value(string);
    return numeral_cell_from_value<int32_t>(static_cast<unsigned char>(payload[index]));
  }
} // namespace NG::runtime
