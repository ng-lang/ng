// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/vm.hpp"
#include "vm/value_ops.hpp"

#include <format>
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
    std::vector<Value> values;
    std::unordered_map<uint32_t, Value> locals;
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
        detail::evaluateInstruction(instruction, function.stringConstants, values, locals);
        break;
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
      case bytecode::Opcode::ExtractTuple:
      {
        const auto &tuple = values.at(instruction.operands[1]).asTuple();
        const uint32_t index = instruction.operands[2];
        if (index >= tuple.size())
          throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", index, tuple.size()));
        values[instruction.operands[0]] = tuple[index];
        break;
      }
      case bytecode::Opcode::AssignMember:
        detail::assignMemberInstruction(instruction, values);
        break;
      case bytecode::Opcode::AssignIndex:
        detail::assignIndexInstruction(instruction, values);
        break;
      case bytecode::Opcode::Return:
      {
        std::optional<Value> result;
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

  auto VM::run(const bytecode::Module &module, hir::DefId entry, const std::vector<int64_t> &arguments,
               size_t fuel) const -> RunResult
  {
    std::vector<Value> values;
    values.reserve(arguments.size());
    for (const auto argument : arguments) values.push_back(Value::integer(argument));
    return run(module, entry, values, fuel);
  }

  auto VM::run(const bytecode::Module &module, hir::DefId entry, const std::vector<Value> &arguments,
               size_t fuel) const -> RunResult
  {
    struct Prepared
    {
      const bytecode::Function *function;
      std::vector<bytecode::DecodedInstruction> instructions;
      std::unordered_map<size_t, size_t> offsets;
    };
    struct Frame
    {
      size_t functionIndex;
      size_t programCounter;
      std::vector<Value> values;
      std::unordered_map<uint32_t, Value> locals;
      std::optional<uint32_t> callerDestination;
    };

    std::vector<Prepared> prepared;
    prepared.reserve(module.functions.size());
    for (const auto &function : module.functions)
    {
      bytecode::Verifier{}.verify(function);
      Prepared item{.function = &function, .instructions = bytecode::Decoder{}.decode(function)};
      for (size_t index = 0; index < item.instructions.size(); ++index) item.offsets.emplace(item.instructions[index].offset, index);
      prepared.push_back(std::move(item));
    }
    if (entry.value >= prepared.size()) throw bytecode::BytecodeError("bytecode module entry is out of range");

    const auto makeFrame = [&prepared](size_t functionIndex, const std::vector<Value> &args,
                                       std::optional<uint32_t> destination) -> Frame {
      const auto &function = *prepared.at(functionIndex).function;
      if (args.size() != function.parameterLocals.size()) throw bytecode::BytecodeError("bytecode function argument count mismatch");
      const auto entryOffset = function.blockOffsets.at(0);
      const auto entryInstruction = prepared.at(functionIndex).offsets.at(entryOffset);
      Frame frame{.functionIndex = functionIndex, .programCounter = entryInstruction, .callerDestination = destination};
      for (size_t index = 0; index < args.size(); ++index) frame.locals.emplace(function.parameterLocals[index], args[index]);
      return frame;
    };

    std::vector<Frame> frames;
    frames.push_back(makeFrame(entry.value, arguments, std::nullopt));
    size_t executed{};
    size_t tailRecursions{};
    while (executed < fuel)
    {
      auto &frame = frames.back();
      const auto &preparedFunction = prepared.at(frame.functionIndex);
      const auto &function = *preparedFunction.function;
      const auto &instruction = preparedFunction.instructions.at(frame.programCounter++);
      ++executed;
      const auto blockInstruction = [&preparedFunction, &function](uint32_t block) { return preparedFunction.offsets.at(function.blockOffsets.at(block)); };
      const auto bindBlockArguments = [&frame, &function](uint32_t target, const std::vector<uint32_t> &operands, size_t first) {
        for (size_t index = 0; index < function.blockParameterLocals.at(target).size(); ++index)
          frame.locals[function.blockParameterLocals[target][index]] = frame.values.at(operands.at(first + index));
      };

      if (instruction.opcode == bytecode::Opcode::Evaluate)
      {
        detail::evaluateInstruction(instruction, function.stringConstants, frame.values, frame.locals);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::BindLocal)
      {
        const uint32_t result = instruction.operands[0];
        if (frame.values.size() <= result) frame.values.resize(result + 1);
        frame.values[result] = frame.values.at(instruction.operands[2]);
        frame.locals[instruction.operands[1]] = frame.values[result];
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::ExtractTuple)
      {
        const auto &tuple = frame.values.at(instruction.operands[1]).asTuple();
        const uint32_t index = instruction.operands[2];
        if (index >= tuple.size())
          throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", index, tuple.size()));
        frame.values[instruction.operands[0]] = tuple[index];
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::AssignMember)
      {
        detail::assignMemberInstruction(instruction, frame.values);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::AssignIndex)
      {
        detail::assignIndexInstruction(instruction, frame.values);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::Call)
      {
        std::vector<Value> callArguments;
        for (size_t index = 0; index < instruction.operands[2]; ++index)
          callArguments.push_back(frame.values.at(instruction.operands[3 + index]));
        frames.push_back(makeFrame(instruction.operands[1], callArguments, instruction.operands[0]));
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::Return)
      {
        std::optional<Value> value;
        if (instruction.operands[0] == 1) value = frame.values.at(instruction.operands[1]);
        const auto destination = frame.callerDestination;
        frames.pop_back();
        if (frames.empty()) return RunResult{.reason = HaltReason::Return, .executedInstructions = executed, .tailRecursions = tailRecursions, .returnValue = value};
        auto &caller = frames.back();
        if (caller.values.size() <= *destination) caller.values.resize(*destination + 1);
        caller.values[*destination] = value.value_or(Value{});
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::Jump || instruction.opcode == bytecode::Opcode::LoopBackedge)
      {
        bindBlockArguments(instruction.operands[0], instruction.operands, 2);
        frame.programCounter = blockInstruction(instruction.operands[0]);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::Branch)
      {
        frame.programCounter = blockInstruction(frame.values.at(instruction.operands[0]) != 0 ? instruction.operands[1] : instruction.operands[2]);
        continue;
      }
      for (size_t index = 0; index < function.parameterLocals.size(); ++index) frame.locals[function.parameterLocals[index]] = frame.values.at(instruction.operands[1 + index]);
      ++tailRecursions;
      frame.programCounter = blockInstruction(0);
    }
    return RunResult{.reason = HaltReason::FuelExhausted, .executedInstructions = executed, .tailRecursions = tailRecursions, .returnValue = std::nullopt};
  }
} // namespace NG::vnext::vm
