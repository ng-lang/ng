// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/vm.hpp"

#include <unordered_map>

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
    while (executed < fuel)
    {
      const auto &instruction = instructions.at(programCounter++);
      ++executed;
      switch (instruction.opcode)
      {
      case bytecode::Opcode::Evaluate:
      case bytecode::Opcode::BindLocal:
        break;
      case bytecode::Opcode::Return:
        return RunResult{.reason = HaltReason::Return, .executedInstructions = executed, .tailRecursions = tailRecursions};
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
    return RunResult{.reason = HaltReason::FuelExhausted, .executedInstructions = executed, .tailRecursions = tailRecursions};
  }
} // namespace NG::vnext::vm
