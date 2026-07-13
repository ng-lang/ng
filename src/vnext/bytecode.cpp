// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/bytecode.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace NG::vnext::bytecode
{
  namespace
  {
    constexpr std::array DESCRIPTORS{
        OpcodeDescriptor{Opcode::Evaluate, "evaluate", OperandLayout::Fixed, 4},
        OpcodeDescriptor{Opcode::BindLocal, "bind_local", OperandLayout::Fixed, 3},
        OpcodeDescriptor{Opcode::Return, "return", OperandLayout::CountPrefixedTail, 0},
        OpcodeDescriptor{Opcode::Jump, "jump", OperandLayout::CountPrefixedTail, 1},
        OpcodeDescriptor{Opcode::Branch, "branch", OperandLayout::Fixed, 3},
        OpcodeDescriptor{Opcode::LoopBackedge, "loop_backedge", OperandLayout::CountPrefixedTail, 1},
        OpcodeDescriptor{Opcode::TailRecur, "tail_recur", OperandLayout::CountPrefixedTail, 0},
    };

    void appendU32(std::vector<uint8_t> &code, uint32_t value)
    {
      for (size_t index = 0; index < 4; ++index)
      {
        code.push_back(static_cast<uint8_t>(value >> (index * 8)));
      }
    }

    [[nodiscard]] auto readU32(const std::vector<uint8_t> &code, size_t &offset) -> uint32_t
    {
      if (code.size() - offset < 4)
      {
        throw BytecodeError("truncated u32 operand");
      }
      uint32_t value{};
      for (size_t index = 0; index < 4; ++index)
      {
        value |= static_cast<uint32_t>(code[offset++]) << (index * 8);
      }
      return value;
    }

    void appendInstruction(std::vector<uint8_t> &code, Opcode opcode, const std::vector<uint32_t> &operands)
    {
      code.push_back(static_cast<uint8_t>(opcode));
      for (const auto operand : operands)
      {
        appendU32(code, operand);
      }
    }
  } // namespace

  auto opcodeDescriptor(Opcode opcode) -> const OpcodeDescriptor &
  {
    for (const auto &descriptor : DESCRIPTORS)
    {
      if (descriptor.opcode == opcode)
      {
        return descriptor;
      }
    }
    throw BytecodeError("unknown opcode");
  }

  auto Compiler::compile(const flowir::Function &flow) const -> Function
  {
    Function result;
    result.blockParameterCounts.reserve(flow.blocks.size());
    for (const auto &block : flow.blocks)
    {
      result.blockParameterCounts.push_back(static_cast<uint32_t>(block.parameterCount));
      result.blockOffsets.push_back(static_cast<uint32_t>(result.code.size()));
      for (const auto &instruction : block.instructions)
      {
        if (instruction.kind == flowir::InstructionKind::Evaluate)
        {
          const uint64_t payload = static_cast<uint64_t>(instruction.payload);
          appendInstruction(result.code, Opcode::Evaluate,
                            {instruction.result.value, static_cast<uint32_t>(instruction.expressionKind),
                             static_cast<uint32_t>(payload), static_cast<uint32_t>(payload >> 32)});
        }
        else
        {
          appendInstruction(result.code, Opcode::BindLocal,
                            {instruction.result.value, instruction.local->value, instruction.source->value});
        }
      }

      const auto &terminator = *block.terminator;
      switch (terminator.kind)
      {
      case flowir::TerminatorKind::Return:
      {
        std::vector<uint32_t> operands{static_cast<uint32_t>(terminator.arguments.size())};
        for (const auto value : terminator.arguments) operands.push_back(value.value);
        appendInstruction(result.code, Opcode::Return, operands);
        break;
      }
      case flowir::TerminatorKind::Jump:
      {
        std::vector<uint32_t> operands{terminator.targets[0].value, static_cast<uint32_t>(terminator.arguments.size())};
        for (const auto value : terminator.arguments) operands.push_back(value.value);
        appendInstruction(result.code, Opcode::Jump, operands);
        break;
      }
      case flowir::TerminatorKind::Branch:
        appendInstruction(result.code,
                          Opcode::Branch,
                          {terminator.arguments[0].value, terminator.targets[0].value, terminator.targets[1].value});
        break;
      case flowir::TerminatorKind::LoopBackedge:
      {
        std::vector<uint32_t> operands{terminator.targets[0].value, static_cast<uint32_t>(terminator.arguments.size())};
        for (const auto value : terminator.arguments) operands.push_back(value.value);
        appendInstruction(result.code, Opcode::LoopBackedge, operands);
        break;
      }
      case flowir::TerminatorKind::TailRecur:
      {
        std::vector<uint32_t> operands{static_cast<uint32_t>(terminator.arguments.size())};
        for (const auto value : terminator.arguments) operands.push_back(value.value);
        appendInstruction(result.code, Opcode::TailRecur, operands);
        break;
      }
      }
    }
    return result;
  }

  auto Decoder::decode(const Function &function) const -> std::vector<DecodedInstruction>
  {
    std::vector<DecodedInstruction> instructions;
    size_t offset{};
    while (offset < function.code.size())
    {
      const size_t instructionOffset = offset;
      const auto opcode = static_cast<Opcode>(function.code[offset++]);
      const auto &descriptor = opcodeDescriptor(opcode);
      std::vector<uint32_t> operands;
      if (descriptor.layout == OperandLayout::Fixed)
      {
        for (size_t index = 0; index < descriptor.fixedOperandCount; ++index) operands.push_back(readU32(function.code, offset));
      }
      else
      {
        for (size_t index = 0; index < descriptor.fixedOperandCount; ++index) operands.push_back(readU32(function.code, offset));
        const uint32_t count = readU32(function.code, offset);
        operands.push_back(count);
        for (size_t index = 0; index < count; ++index) operands.push_back(readU32(function.code, offset));
      }
      instructions.push_back(DecodedInstruction{.opcode = opcode, .operands = std::move(operands), .offset = instructionOffset});
    }
    return instructions;
  }

  void Verifier::verify(const Function &function) const
  {
    const auto instructions = Decoder{}.decode(function);
    if (function.blockOffsets.size() != function.blockParameterCounts.size())
    {
      throw BytecodeError("bytecode block offset table does not match block table");
    }
    for (const auto offset : function.blockOffsets)
    {
      const bool isInstructionBoundary = std::any_of(instructions.begin(), instructions.end(),
                                                     [offset](const auto &instruction) { return instruction.offset == offset; });
      if (!isInstructionBoundary)
      {
        throw BytecodeError("bytecode block offset is not an instruction boundary");
      }
    }
    for (const auto &instruction : instructions)
    {
      const auto targetAndCount = [&function, &instruction](bool hasTarget) {
        const uint32_t target = instruction.operands[0];
        if (target >= function.blockParameterCounts.size())
        {
          throw BytecodeError("bytecode branch target is out of range");
        }
        const uint32_t count = instruction.operands[1];
        if (count != function.blockParameterCounts[target])
        {
          throw BytecodeError("bytecode branch argument count does not match target block parameters");
        }
        static_cast<void>(hasTarget);
      };
      if (instruction.opcode == Opcode::Jump || instruction.opcode == Opcode::LoopBackedge)
      {
        targetAndCount(true);
      }
      else if (instruction.opcode == Opcode::Branch)
      {
        if (instruction.operands[1] >= function.blockParameterCounts.size() ||
            instruction.operands[2] >= function.blockParameterCounts.size())
        {
          throw BytecodeError("bytecode branch target is out of range");
        }
      }
    }
  }
} // namespace NG::vnext::bytecode
