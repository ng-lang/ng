// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace NG::vnext
{
  class Value final
  {
    struct ArrayStorage { std::shared_ptr<std::vector<Value>> elements; auto operator==(const ArrayStorage &) const -> bool = default; };
    struct TupleStorage { std::shared_ptr<std::vector<Value>> elements; auto operator==(const TupleStorage &) const -> bool = default; };
    struct StructStorage
    {
      uint32_t type{};
      std::shared_ptr<std::vector<Value>> fields;
      auto operator==(const StructStorage &) const -> bool = default;
    };

  public:
    Value() : storage_(int64_t{}) {}
    Value(int64_t integer) : storage_(integer) {}
    Value(std::string string) : storage_(std::move(string)) {}
    Value(std::vector<Value> elements) : storage_(ArrayStorage{std::make_shared<std::vector<Value>>(std::move(elements))}) {}

    [[nodiscard]] static auto integer(int64_t value) -> Value { return Value{value}; }
    [[nodiscard]] static auto string(std::string value) -> Value { return Value{std::move(value)}; }
    [[nodiscard]] static auto array(std::vector<Value> elements) -> Value { return Value{std::move(elements)}; }
    [[nodiscard]] static auto tuple(std::vector<Value> elements) -> Value
    {
      Value value;
      value.storage_ = TupleStorage{std::make_shared<std::vector<Value>>(std::move(elements))};
      return value;
    }
    [[nodiscard]] static auto structure(uint32_t type, std::vector<Value> fields) -> Value
    {
      Value value;
      value.storage_ = StructStorage{type, std::make_shared<std::vector<Value>>(std::move(fields))};
      return value;
    }

    [[nodiscard]] auto isInteger() const -> bool { return std::holds_alternative<int64_t>(storage_); }
    [[nodiscard]] auto isString() const -> bool { return std::holds_alternative<std::string>(storage_); }
    [[nodiscard]] auto isArray() const -> bool { return std::holds_alternative<ArrayStorage>(storage_); }
    [[nodiscard]] auto isTuple() const -> bool { return std::holds_alternative<TupleStorage>(storage_); }
    [[nodiscard]] auto isStruct() const -> bool { return std::holds_alternative<StructStorage>(storage_); }
    [[nodiscard]] auto asInteger() const -> int64_t
    {
      if (!isInteger()) throw std::runtime_error("runtime value is not an i64");
      return std::get<int64_t>(storage_);
    }
    [[nodiscard]] auto asArray() const -> const std::vector<Value> &
    {
      if (!isArray()) throw std::runtime_error("runtime value is not an array");
      return *std::get<ArrayStorage>(storage_).elements;
    }
    [[nodiscard]] auto asArrayMut() -> std::vector<Value> &
    {
      if (!isArray()) throw std::runtime_error("runtime value is not an array");
      return *std::get<ArrayStorage>(storage_).elements;
    }
    [[nodiscard]] auto asTuple() const -> const std::vector<Value> &
    {
      if (!isTuple()) throw std::runtime_error("runtime value is not a tuple");
      return *std::get<TupleStorage>(storage_).elements;
    }
    [[nodiscard]] auto asTupleMut() -> std::vector<Value> &
    {
      if (!isTuple()) throw std::runtime_error("runtime value is not a tuple");
      return *std::get<TupleStorage>(storage_).elements;
    }
    [[nodiscard]] auto asStructType() const -> uint32_t
    {
      if (!isStruct()) throw std::runtime_error("runtime value is not a struct");
      return std::get<StructStorage>(storage_).type;
    }
    [[nodiscard]] auto asStruct() const -> const std::vector<Value> &
    {
      if (!isStruct()) throw std::runtime_error("runtime value is not a struct");
      return *std::get<StructStorage>(storage_).fields;
    }
    [[nodiscard]] auto asStructMut() -> std::vector<Value> &
    {
      if (!isStruct()) throw std::runtime_error("runtime value is not a struct");
      return *std::get<StructStorage>(storage_).fields;
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
    std::variant<int64_t, std::string, ArrayStorage, TupleStorage, StructStorage> storage_;
  };
} // namespace NG::vnext
