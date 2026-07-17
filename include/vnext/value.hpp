// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace NG::vnext
{
  /// The initial tagged vNext runtime value. Aggregate alternatives are added
  /// here rather than creating parallel VM-specific representations.
  class Value final
  {
  public:
    Value() : storage_(int64_t{}) {}
    Value(int64_t integer) : storage_(integer) {}
    Value(std::string string) : storage_(std::move(string)) {}

    [[nodiscard]] static auto integer(int64_t value) -> Value { return Value{value}; }
    [[nodiscard]] static auto string(std::string value) -> Value { return Value{std::move(value)}; }

    [[nodiscard]] auto isInteger() const -> bool { return std::holds_alternative<int64_t>(storage_); }
    [[nodiscard]] auto isString() const -> bool { return std::holds_alternative<std::string>(storage_); }
    [[nodiscard]] auto asInteger() const -> int64_t
    {
      if (!isInteger()) throw std::runtime_error("runtime value is not an i64");
      return std::get<int64_t>(storage_);
    }
    [[nodiscard]] auto asString() const -> const std::string &
    {
      if (!isString()) throw std::runtime_error("runtime value is not a string");
      return std::get<std::string>(storage_);
    }

    friend auto operator==(const Value &, const Value &) -> bool = default;
    friend auto operator==(const Value &value, int64_t integer) -> bool { return value.isInteger() && value.asInteger() == integer; }
    friend auto operator==(int64_t integer, const Value &value) -> bool { return value == integer; }

  private:
    std::variant<int64_t, std::string> storage_;
  };
} // namespace NG::vnext
