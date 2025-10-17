#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ng::orgasm {

// Primitive types supported in ORGASM Level-2
enum class PrimitiveType {
  I8,
  I16,
  I32,
  I64,
  I128,
  U8,
  U16,
  U32,
  U64,
  U128,
  F16,
  F32,
  F64,
  F128,
  BOOL,
  CHAR,
  UNIT,
  ADDR,
  // Vector types
  V128,
  V256,
  V512,
  // Atomic types
  ATOMIC_I32,
  ATOMIC_I64,
  ATOMIC_ADDR,
};

// Runtime value representation
struct Value {
  PrimitiveType type;
  std::variant<int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t,
               uint64_t, float, double, bool, char, void *, std::string>
      data;

  Value() : type(PrimitiveType::UNIT), data(nullptr) {}

  explicit Value(int32_t v) : type(PrimitiveType::I32), data(v) {}
  explicit Value(int64_t v) : type(PrimitiveType::I64), data(v) {}
  explicit Value(uint32_t v) : type(PrimitiveType::U32), data(v) {}
  explicit Value(uint64_t v) : type(PrimitiveType::U64), data(v) {}
  explicit Value(float v) : type(PrimitiveType::F32), data(v) {}
  explicit Value(double v) : type(PrimitiveType::F64), data(v) {}
  explicit Value(bool v) : type(PrimitiveType::BOOL), data(v) {}
  explicit Value(void *v) : type(PrimitiveType::ADDR), data(v) {}
  explicit Value(const std::string &v)
      : type(PrimitiveType::ADDR), data(v) {}
};

// Constant definition
struct ConstDef {
  PrimitiveType type;
  Value value;
};

// String definition
struct StringDef {
  std::string value;
};

// Array definition
struct ArrayDef {
  PrimitiveType element_type;
  std::vector<Value> elements;
};

// Variable definition
struct VarDef {
  PrimitiveType type;
  Value initial_value;
};

// Parameter definition
struct ParamDef {
  std::string name;
  PrimitiveType type;
};

// Label definition
struct Label {
  std::string name;
  int offset; // Instruction offset in function
};

} // namespace ng::orgasm
