// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/vm.hpp"

#include <unordered_map>
#include <vector>

namespace NG::vnext::vm
{
  auto VM::run(const bytecode::Function &function, size_t fuel) const -> RunResult
  {
    return run(function, {}, fuel);
  }

  auto VM::run(const bytecode::Function &function, const std::vector<int64_t> &arguments, size_t fuel) const -> RunResult
  {
    bytecode::Verifier{}.verify(function);
    if (arguments.size() != function.parameterLocals.size())
    {
      throw bytecode::BytecodeError("bytecode function argument count mismatch");
    }
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
    for (size_t index = 0; index < arguments.size(); ++index)
    {
      locals.emplace(function.parameterLocals[index], arguments[index]);
    }

    const auto jumpToBlock = [&function, &blockInstruction, &values, &locals](uint32_t target,
                                                                                const std::vector<uint32_t> &argumentValues,
                                                                                size_t firstArgument) {
      const auto &parameterLocals = function.blockParameterLocals.at(target);
      for (size_t index = 0; index < parameterLocals.size(); ++index)
      {
        locals[parameterLocals[index]] = values.at(argumentValues.at(firstArgument + index));
      }
      return blockInstruction(target);
    };

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
        else if (kind == hir::ExpressionKind::Grouped)
        {
          values[result] = values.at(instruction.operands[5]);
        }
        else if (kind == hir::ExpressionKind::Binary)
        {
          const int64_t left = values.at(instruction.operands[5]);
          const int64_t right = values.at(instruction.operands[6]);
          switch (payload)
          {
          case 1: values[result] = left + right; break;
          case 2: values[result] = left - right; break;
          case 3: values[result] = left * right; break;
          case 4: values[result] = left / right; break;
          case 5: values[result] = left % right; break;
          case 6: values[result] = left == right; break;
          case 7: values[result] = left != right; break;
          case 8: values[result] = left < right; break;
          case 9: values[result] = left <= right; break;
          case 10: values[result] = left > right; break;
          case 11: values[result] = left >= right; break;
          default: throw bytecode::BytecodeError("unsupported binary operation");
          }
        }
        else
        {
          values[result] = 0;
        }
        break;
      }
      case bytecode::Opcode::Call:
        throw bytecode::BytecodeError("direct calls require a bytecode module");
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
        programCounter = jumpToBlock(instruction.operands[0], instruction.operands, 2);
        break;
      case bytecode::Opcode::Branch:
        programCounter = blockInstruction(values.at(instruction.operands[0]) != 0 ? instruction.operands[1]
                                                                                   : instruction.operands[2]);
        break;
      case bytecode::Opcode::TailRecur:
        for (size_t index = 0; index < function.parameterLocals.size(); ++index)
        {
          locals[function.parameterLocals[index]] = values.at(instruction.operands.at(index + 1));
        }
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
