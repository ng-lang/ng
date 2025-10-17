#pragma once

#include "types.hpp"
#include <string>
#include <variant>

namespace ng::orgasm {

// Instruction opcodes
enum class OpCode {
  // Data/Stack Operations
  CAST,
  LOAD_PARAM,
  LOAD_CONST,
  LOAD_VALUE,
  STORE_VALUE,
  LOAD_VOLATILE,
  STORE_VOLATILE,
  LOAD_STR,
  LOAD_ARRAY,
  PUSH_PARAM,
  PUSH_SELF,
  LOAD_SYMBOL,
  INVOKE_METHOD,
  CALL,
  CALL_IMPORT,
  IGNORE,

  // Arithmetic/Logic
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  GT,
  LT,
  GE,
  LE,
  EQ,
  NE,
  SHL,
  SHR,
  AND,
  OR,
  XOR,
  NOT,

  // Control Flow
  BR,
  GOTO,
  RETURN,

  // Tuple Operations
  TUPLE_CREATE,
  TUPLE_DESTROY,
  TUPLE_GET,
  TUPLE_SET,

  // SIMD Operations
  VADD,
  VSUB,
  VMUL,
  VLOAD,
  VSTORE,

  // Atomic Operations
  ATOMIC_ADD,
  ATOMIC_SUB,
  ATOMIC_AND,
  ATOMIC_OR,
  ATOMIC_CAS,
  FENCE,
};

// Operand types
struct ConstOperand {
  int index;
};

struct ValueOperand {
  int index;
};

struct ParamOperand {
  int index;
};

struct ArrayOperand {
  int index;
};

struct StringOperand {
  int index;
};

struct SymbolOperand {
  int index;
};

struct FunctionOperand {
  int index;
};

struct ImportOperand {
  int index;
};

struct LabelOperand {
  std::string name;
};

struct AddressOperand {
  int address; // Local instruction address
};

struct OffsetOperand {
  int offset; // Byte offset for tuple operations
};

using Operand =
    std::variant<ConstOperand, ValueOperand, ParamOperand, ArrayOperand,
                 StringOperand, SymbolOperand, FunctionOperand, ImportOperand,
                 LabelOperand, AddressOperand, OffsetOperand, int>;

// Instruction representation
struct Instruction {
  int address;                   // Local instruction address
  OpCode opcode;                 // Instruction type
  PrimitiveType type;            // Type annotation (for typed instructions)
  std::vector<Operand> operands; // Operands
  std::string comment;           // Optional comment

  Instruction(int addr, OpCode op)
      : address(addr), opcode(op), type(PrimitiveType::UNIT) {}

  Instruction(int addr, OpCode op, PrimitiveType t)
      : address(addr), opcode(op), type(t) {}
};

} // namespace ng::orgasm
