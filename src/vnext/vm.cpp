// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/vm.hpp"

#include <unordered_map>
#include <vector>

namespace NG::vnext::vm
{
  auto VM::run(const bytecode::Function &function, size_t fuel) const -> RunResult
  {
    bytecode::Verifier{}.verify(function);
    const auto instructions = bytecode::Decoder{}.decode(function);
    std::unordered_map<size_t, size_t> offsetToInstruction;
    for (size_t index = 0; index < instructions.size(); ++index)
    {
      offsetToInstruction.emplace(instructions[index].offset, index);
    }

    const auto blockInstruction = [&function, &offsetToInstruction](uint32_t block) -> size_t {
      const auto offset = function.blockOffsets.at(block);
      const auto instruction = offsetToInstruction.find(offset);
      if (instruction == offsetToInstruction.end())
      {
        throw bytecode::BytecodeError("bytecode block offset is not an instruction boundary");
      }
      return instruction->second;
    };

    size_t programCounter = blockInstruction(0);
    size_t executed{};
    size_t tailRecursions{};
    std::vector<int64_t> values;
    std::unordered_map<uint32_t, int64_t> locals;
    while (executed < fuel)
    {
      const auto &instruction = instructions.at(programCounter++);
      ++executed;
      switch (instruction.opcode)
      {
      case bytecode::Opcode::Evaluate:
      {
        const uint32_t result = instruction.operands[0];
        if (values.size() <= result) values.resize(result + 1);
        const auto kind = static_cast<hir::ExpressionKind>(instruction.operands[1]);
        const uint64_t payload = static_cast<uint64_t>(instruction.operands[2]) |
                                 (static_cast<uint64_t>(instruction.operands[3]) << 32);
        if (kind == hir::ExpressionKind::IntegerLiteral || kind == hir::ExpressionKind::BooleanLiteral)
        {
          values[result] = static_cast<int64_t>(payload);
        }
        else if (kind == hir::ExpressionKind::ResolvedName)
        {
          values[result] = locals.at(static_cast<uint32_t>(payload));
        }
        else
        {
          values[result] = 0;
        }
        break;
      }
      case bytecode::Opcode::BindLocal:
      {
        const uint32_t result = instruction.operands[0];
        const uint32_t local = instruction.operands[1];
        const uint32_t source = instruction.operands[2];
        if (values.size() <= result) values.resize(result + 1);
        values[result] = values.at(source);
        locals[local] = values[result];
        break;
      }
      case bytecode::Opcode::Return:
      {
        std::optional<int64_t> result;
        if (instruction.operands[0] == 1)
        {
          result = values.at(instruction.operands[1]);
        }
        return RunResult{.reason = HaltReason::Return,
                         .executedInstructions = executed,
                         .tailRecursions = tailRecursions,
                         .returnValue = result};
      }
      case bytecode::Opcode::Jump:
      case bytecode::Opcode::LoopBackedge:
        programCounter = blockInstruction(instruction.operands[0]);
        break;
      case bytecode::Opcode::Branch:
        // Runtime value execution is deliberately not part of this control-core
        // slice. Until values are lowered, select the first (true) edge.
        programCounter = blockInstruction(instruction.operands[1]);
        break;
      case bytecode::Opcode::TailRecur:
        ++tailRecursions;
        programCounter = blockInstruction(0);
        break;
      }
    }
    return RunResult{.reason = HaltReason::FuelExhausted,
                     .executedInstructions = executed,
                     .tailRecursions = tailRecursions,
                     .returnValue = std::nullopt};
  }
} // namespace NG::vnext::vm
