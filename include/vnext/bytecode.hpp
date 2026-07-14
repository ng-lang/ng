// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/flowir.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace NG::vnext::bytecode
{
  enum class Opcode : uint8_t
  {
    Evaluate,
    BindLocal,
    Return,
    Jump,
    Branch,
    LoopBackedge,
    TailRecur,
  };

  enum class OperandLayout
  {
    Fixed,
    CountPrefixedTail,
  };

  struct OpcodeDescriptor
  {
    Opcode opcode;
    const char *name;
    OperandLayout layout;
    uint8_t fixedOperandCount;
  };

  [[nodiscard]] auto opcodeDescriptor(Opcode opcode) -> const OpcodeDescriptor &;

  struct DecodedInstruction
  {
    Opcode opcode;
    std::vector<uint32_t> operands;
    size_t offset;
  };

  struct Function
  {
    std::vector<uint8_t> code;
    std::vector<uint32_t> parameterLocals;
    std::vector<uint32_t> blockParameterCounts;
    std::vector<std::vector<uint32_t>> blockParameterLocals;
    std::vector<uint32_t> blockOffsets;
  };

  struct BytecodeError : std::runtime_error
  {
    explicit BytecodeError(std::string message) : std::runtime_error(std::move(message)) {}
  };

  class Compiler final
  {
  public:
    [[nodiscard]] auto compile(const flowir::Function &function) const -> Function;
  };

  class Decoder final
  {
  public:
    [[nodiscard]] auto decode(const Function &function) const -> std::vector<DecodedInstruction>;
  };

  class Verifier final
  {
  public:
    void verify(const Function &function) const;
  };
} // namespace NG::vnext::bytecode
