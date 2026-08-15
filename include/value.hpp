// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace NG
{
  /// One step of a runtime place path. Member steps select a product field;
  /// Index steps select an aggregate element by an i64 index evaluated when the
  /// reference/place was created.
  struct PlaceStep
  {
    enum class Kind : uint8_t
    {
      Member,
      Index,
    };
    Kind kind{};
    uint32_t field{};
    int64_t index{};
    auto operator==(const PlaceStep &) const -> bool = default;
  };

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
    struct EnumStorage
    {
      uint32_t type{};
      uint32_t variant{};
      std::shared_ptr<std::vector<Value>> payload;
      auto operator==(const EnumStorage &) const -> bool = default;
    };
    struct RangeStorage
    {
      int64_t start{};
      int64_t end{};
      auto operator==(const RangeStorage &) const -> bool = default;
    };
    /// A scoped, non-owning view: the shared cell of the root local binding
    /// plus the steps that reach the referent. Copies of a reference share the
    /// same view; the root cell is the frame's canonical storage for the local.
    struct ReferenceStorage
    {
      std::shared_ptr<Value> root;
      std::vector<PlaceStep> steps;
      bool mutableRef{};
      auto operator==(const ReferenceStorage &) const -> bool = default;
    };
    /// An opaque native handle token (`type X = native;`): an integer token
    /// owned by the embedding; runtime operations pass it through untouched.
    struct OpaqueStorage
    {
      uint64_t token{};
      auto operator==(const OpaqueStorage &) const -> bool = default;
    };
    /// A dynamic trait view (`ref<Trait>`): a shared-borrowed root cell plus
    /// the trait/concrete type ids that select the dispatch table.
    struct TraitViewStorage
    {
      std::shared_ptr<Value> root;
      std::vector<PlaceStep> steps;
      uint32_t trait{};
      uint32_t concrete{};
      auto operator==(const TraitViewStorage &) const -> bool = default;
    };

  public:
    Value() : storage_(int64_t{}) {}
    Value(int64_t integer) : storage_(integer) {}
    Value(double floating) : storage_(floating) {}
    Value(std::string string) : storage_(std::move(string)) {}
    Value(std::vector<Value> elements) : storage_(ArrayStorage{std::make_shared<std::vector<Value>>(std::move(elements))}) {}

    [[nodiscard]] static auto integer(int64_t value) -> Value { return Value{value}; }
    [[nodiscard]] static auto float_(double value) -> Value { return Value{value}; }
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
    [[nodiscard]] static auto enumeration(uint32_t type, uint32_t variant, std::vector<Value> payload) -> Value
    {
      Value value;
      value.storage_ = EnumStorage{type, variant, std::make_shared<std::vector<Value>>(std::move(payload))};
      return value;
    }
    [[nodiscard]] static auto range(int64_t start, int64_t end) -> Value
    {
      Value value;
      value.storage_ = RangeStorage{start, end};
      return value;
    }
    [[nodiscard]] static auto reference(std::shared_ptr<Value> root, std::vector<PlaceStep> steps, bool mutableRef) -> Value
    {
      Value value;
      value.storage_ = ReferenceStorage{std::move(root), std::move(steps), mutableRef};
      return value;
    }
    [[nodiscard]] static auto traitView(std::shared_ptr<Value> root, std::vector<PlaceStep> steps, uint32_t trait,
                                        uint32_t concrete) -> Value
    {
      Value value;
      value.storage_ = TraitViewStorage{std::move(root), std::move(steps), trait, concrete};
      return value;
    }
    [[nodiscard]] static auto opaque(uint64_t token) -> Value
    {
      Value value;
      value.storage_ = OpaqueStorage{token};
      return value;
    }

    [[nodiscard]] auto isInteger() const -> bool { return std::holds_alternative<int64_t>(storage_); }
    [[nodiscard]] auto isDouble() const -> bool { return std::holds_alternative<double>(storage_); }
    [[nodiscard]] auto isString() const -> bool { return std::holds_alternative<std::string>(storage_); }
    [[nodiscard]] auto isArray() const -> bool { return std::holds_alternative<ArrayStorage>(storage_); }
    [[nodiscard]] auto isTuple() const -> bool { return std::holds_alternative<TupleStorage>(storage_); }
    [[nodiscard]] auto isStruct() const -> bool { return std::holds_alternative<StructStorage>(storage_); }
    [[nodiscard]] auto isEnum() const -> bool { return std::holds_alternative<EnumStorage>(storage_); }
    [[nodiscard]] auto isReference() const -> bool { return std::holds_alternative<ReferenceStorage>(storage_); }
    [[nodiscard]] auto isTraitView() const -> bool { return std::holds_alternative<TraitViewStorage>(storage_); }
    [[nodiscard]] auto isOpaque() const -> bool { return std::holds_alternative<OpaqueStorage>(storage_); }
    [[nodiscard]] auto isRange() const -> bool { return std::holds_alternative<RangeStorage>(storage_); }
    [[nodiscard]] auto asInteger() const -> int64_t
    {
      if (!isInteger()) throw std::runtime_error("runtime value is not an i64");
      return std::get<int64_t>(storage_);
    }
    [[nodiscard]] auto asDouble() const -> double
    {
      if (!isDouble()) throw std::runtime_error("runtime value is not an f64");
      return std::get<double>(storage_);
    }
    /// Numeric view: integers and doubles both expose a double value for
    /// mixed numeric comparisons.
    [[nodiscard]] auto asNumber() const -> double
    {
      return isDouble() ? asDouble() : static_cast<double>(asInteger());
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
    [[nodiscard]] auto asEnumType() const -> uint32_t
    {
      if (!isEnum()) throw std::runtime_error("runtime value is not an enum");
      return std::get<EnumStorage>(storage_).type;
    }
    [[nodiscard]] auto asEnumVariant() const -> uint32_t
    {
      if (!isEnum()) throw std::runtime_error("runtime value is not an enum");
      return std::get<EnumStorage>(storage_).variant;
    }
    [[nodiscard]] auto asEnumPayload() const -> const std::vector<Value> &
    {
      if (!isEnum()) throw std::runtime_error("runtime value is not an enum");
      return *std::get<EnumStorage>(storage_).payload;
    }
    [[nodiscard]] auto asString() const -> const std::string &
    {
      if (!isString()) throw std::runtime_error("runtime value is not a string");
      return std::get<std::string>(storage_);
    }
    [[nodiscard]] auto asReference() const -> const ReferenceStorage &
    {
      if (!isReference()) throw std::runtime_error("runtime value is not a reference");
      return std::get<ReferenceStorage>(storage_);
    }
    [[nodiscard]] auto asTraitView() const -> const TraitViewStorage &
    {
      if (!isTraitView()) throw std::runtime_error("runtime value is not a trait view");
      return std::get<TraitViewStorage>(storage_);
    }
    [[nodiscard]] auto asOpaque() const -> uint64_t
    {
      if (!isOpaque()) throw std::runtime_error("runtime value is not an opaque handle");
      return std::get<OpaqueStorage>(storage_).token;
    }
    [[nodiscard]] auto asRange() const -> const RangeStorage &
    {
      if (!isRange()) throw std::runtime_error("runtime value is not a range");
      return std::get<RangeStorage>(storage_);
    }

    /// Copy-first deep copy (D-015): aggregate storages are recursively
    /// cloned; references stay views over the same root cell.
    [[nodiscard]] auto deepCopy() const -> Value;

    friend auto operator==(const Value &, const Value &) -> bool = default;
    friend auto operator==(const Value &value, int64_t integer) -> bool { return value.isInteger() && value.asInteger() == integer; }
    friend auto operator==(int64_t integer, const Value &value) -> bool { return value == integer; }

  private:
    std::variant<int64_t, double, std::string, ArrayStorage, TupleStorage, StructStorage, EnumStorage, ReferenceStorage,
                 TraitViewStorage, OpaqueStorage, RangeStorage>
        storage_;
  };

  inline auto Value::deepCopy() const -> Value
  {
    if (const auto *array = std::get_if<ArrayStorage>(&storage_))
    {
      std::vector<Value> elements;
      elements.reserve(array->elements->size());
      for (const auto &element : *array->elements) elements.push_back(element.deepCopy());
      return Value::array(std::move(elements));
    }
    if (const auto *tuple = std::get_if<TupleStorage>(&storage_))
    {
      std::vector<Value> elements;
      elements.reserve(tuple->elements->size());
      for (const auto &element : *tuple->elements) elements.push_back(element.deepCopy());
      return Value::tuple(std::move(elements));
    }
    if (const auto *structure = std::get_if<StructStorage>(&storage_))
    {
      std::vector<Value> fields;
      fields.reserve(structure->fields->size());
      for (const auto &field : *structure->fields) fields.push_back(field.deepCopy());
      return Value::structure(structure->type, std::move(fields));
    }
    if (const auto *enumeration = std::get_if<EnumStorage>(&storage_))
    {
      std::vector<Value> payload;
      payload.reserve(enumeration->payload->size());
      for (const auto &value : *enumeration->payload) payload.push_back(value.deepCopy());
      return Value::enumeration(enumeration->type, enumeration->variant, std::move(payload));
    }
    // Scalars, strings, and reference views copy structurally; references keep the same root cell.
    return *this;
  }
} // namespace NG
