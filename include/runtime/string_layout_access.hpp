#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

namespace NG::runtime
{
  inline auto string_length(const NGString &string) -> size_t
  {
    return static_cast<size_t>(string.header_length());
  }

  inline auto string_data_handle(const NGString &string) -> NativeHandle
  {
    return string.header_data_handle();
  }

  inline auto string_read_code_unit(const NGString &string, size_t index) -> RuntimeRef<NGObject>
  {
    if (index >= string_length(string))
    {
      throw RuntimeException("Index out of bounds: " + std::to_string(index));
    }
    auto payload = string.payload_value();
    return makert<NGIntegral<int32_t>>(static_cast<unsigned char>(payload[index]));
  }
} // namespace NG::runtime
