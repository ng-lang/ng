// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vm.hpp"
#include "vm/value_ops.hpp"

#include <format>
#include <memory>
#include <unordered_map>
#include <vector>

namespace NG::vm
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
    detail::LocalCells locals;
    for (size_t index = 0; index < arguments.size(); ++index)
    {
      locals.emplace(function.parameterLocals[index], std::make_shared<Value>(Value::integer(arguments[index])));
    }

    const auto jumpToBlock = [&function, &blockInstruction, &values, &locals](uint32_t target,
                                                                                const std::vector<uint32_t> &argumentValues,
                                                                                size_t firstArgument) {
      const auto &parameterLocals = function.blockParameterLocals.at(target);
      for (size_t index = 0; index < parameterLocals.size(); ++index)
      {
        locals[parameterLocals[index]] =
            std::make_shared<Value>(values.at(argumentValues.at(firstArgument + index)).deepCopy());
      }
      return blockInstruction(target);
    };

    while (fuel == 0 || executed < fuel)
    {
      const auto &instruction = instructions.at(programCounter++);
      ++executed;
      switch (instruction.opcode)
      {
      case bytecode::Opcode::Evaluate:
        detail::evaluateInstruction(instruction, function.stringConstants, function.valueTypes, values, locals);
        break;
      case bytecode::Opcode::Call:
        throw bytecode::BytecodeError("direct calls require a bytecode module");
      case bytecode::Opcode::CallTrait:
        throw bytecode::BytecodeError("trait dispatch requires a bytecode module");
      case bytecode::Opcode::MakeTraitView:
        detail::makeTraitViewInstruction(instruction, values, locals);
        break;
      case bytecode::Opcode::BindLocal:
      {
        const uint32_t result = instruction.operands[0];
        const uint32_t local = instruction.operands[1];
        const uint32_t source = instruction.operands[2];
        if (values.size() <= result) values.resize(result + 1);
        values[result] = values.at(source).deepCopy();
        locals[local] = std::make_shared<Value>(values[result].deepCopy());
        break;
      }
      case bytecode::Opcode::ExtractTuple:
      {
        const auto &tuple = values.at(instruction.operands[1]).asTuple();
        const uint32_t index = instruction.operands[2];
        if (index >= tuple.size())
          throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", index, tuple.size()));
        if (values.size() <= instruction.operands[0]) values.resize(instruction.operands[0] + 1);
        values[instruction.operands[0]] = tuple[index].deepCopy();
        break;
      }
      case bytecode::Opcode::MakeRef:
        detail::makeRefInstruction(instruction, values, locals);
        break;
      case bytecode::Opcode::LoadRef:
        detail::loadRefInstruction(instruction, values);
        break;
      case bytecode::Opcode::AssignPlace:
        detail::assignPlaceInstruction(instruction, values, locals);
        break;
      case bytecode::Opcode::LoadVariant:
        detail::loadVariantInstruction(instruction, values);
        break;
      case bytecode::Opcode::RangeStart:
      {
        if (values.size() <= instruction.operands[0]) values.resize(instruction.operands[0] + 1);
        values[instruction.operands[0]] = Value::integer(values.at(instruction.operands[1]).asRange().start);
        break;
      }
      case bytecode::Opcode::ArrayLength:
      {
        const auto &source = values.at(instruction.operands[1]);
        int64_t length = source.isRange() ? source.asRange().end - source.asRange().start
                                          : static_cast<int64_t>(source.asArray().size());
        if (values.size() <= instruction.operands[0]) values.resize(instruction.operands[0] + 1);
        values[instruction.operands[0]] = Value::integer(length);
        break;
      }
      case bytecode::Opcode::AppendArray:
      {
        if (values.size() <= instruction.operands[0]) values.resize(instruction.operands[0] + 1);
        // Deep-copy the source so appending never aliases the receiver.
        values[instruction.operands[0]] = values.at(instruction.operands[1]).deepCopy();
        values[instruction.operands[0]].asArrayMut().push_back(values.at(instruction.operands[2]).deepCopy());
        break;
      }
      case bytecode::Opcode::EnumListLength: detail::enumListLengthInstruction(instruction, values); break;
      case bytecode::Opcode::EnumListGet: detail::enumListGetInstruction(instruction, values); break;
      case bytecode::Opcode::Slice:
      {
        const auto &receiver = values.at(instruction.operands[1]);
        const auto &range = values.at(instruction.operands[2]).asRange();
        const auto &array = receiver.asArray();
        if (range.start < 0 || range.end < range.start || static_cast<uint64_t>(range.end) > array.size())
          throw bytecode::BytecodeError(std::format("array slice out of bounds: [{}..{}) of length {}", range.start, range.end,
                                                    array.size()));
        std::vector<Value> elements;
        for (int64_t index = range.start; index < range.end; ++index) elements.push_back(array[static_cast<size_t>(index)].deepCopy());
        if (values.size() <= instruction.operands[0]) values.resize(instruction.operands[0] + 1);
        values[instruction.operands[0]] = Value::array(std::move(elements));
        break;
      }
      case bytecode::Opcode::SpliceTuple:
      {
        if (values.size() <= instruction.operands[0]) values.resize(instruction.operands[0] + 1);
        std::vector<Value> elements;
        for (size_t index = 0; index < instruction.operands[1]; ++index)
        {
          const auto &operand = values.at(instruction.operands[2 + index]);
          if (operand.isTuple())
            for (const auto &element : operand.asTuple()) elements.push_back(element.deepCopy());
          else
            elements.push_back(operand.deepCopy());
        }
        values[instruction.operands[0]] = Value::tuple(std::move(elements));
        break;
      }
      case bytecode::Opcode::ExtractPayload:
        detail::extractPayloadInstruction(instruction, values);
        break;
      case bytecode::Opcode::Return:
      {
        std::optional<Value> result;
        if (instruction.operands[0] == 1)
        {
          result = values.at(instruction.operands[1]).deepCopy();
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
          locals[function.parameterLocals[index]] =
              std::make_shared<Value>(values.at(instruction.operands.at(index + 1)).deepCopy());
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
               size_t fuel, const NativeRegistry *natives) const -> RunResult
  {
    std::vector<Value> values;
    values.reserve(arguments.size());
    for (const auto argument : arguments) values.push_back(Value::integer(argument));
    return run(module, entry, values, fuel, natives);
  }

  auto VM::run(const bytecode::Module &module, hir::DefId entry, const std::vector<Value> &arguments,
               size_t fuel, const NativeRegistry *natives) const -> RunResult
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
      detail::LocalCells locals;
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
      for (size_t index = 0; index < args.size(); ++index)
        frame.locals.emplace(function.parameterLocals[index], std::make_shared<Value>(args[index].deepCopy()));
      return frame;
    };

    std::vector<Frame> frames;
    frames.push_back(makeFrame(entry.value, arguments, std::nullopt));
    size_t executed{};
    size_t tailRecursions{};
    while (fuel == 0 || executed < fuel)
    {
      auto &frame = frames.back();
      const auto &preparedFunction = prepared.at(frame.functionIndex);
      const auto &function = *preparedFunction.function;
      const auto &instruction = preparedFunction.instructions.at(frame.programCounter++);

      ++executed;
      const auto blockInstruction = [&preparedFunction, &function](uint32_t block) { return preparedFunction.offsets.at(function.blockOffsets.at(block)); };
      const auto bindBlockArguments = [&frame, &function](uint32_t target, const std::vector<uint32_t> &operands, size_t first) {
        for (size_t index = 0; index < function.blockParameterLocals.at(target).size(); ++index)
          frame.locals[function.blockParameterLocals[target][index]] =
              std::make_shared<Value>(frame.values.at(operands.at(first + index)).deepCopy());
      };

      if (instruction.opcode == bytecode::Opcode::Evaluate)
      {
        detail::evaluateInstruction(instruction, function.stringConstants, function.valueTypes, frame.values, frame.locals);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::BindLocal)
      {
        const uint32_t result = instruction.operands[0];
        if (frame.values.size() <= result) frame.values.resize(result + 1);
        frame.values[result] = frame.values.at(instruction.operands[2]).deepCopy();
        frame.locals[instruction.operands[1]] = std::make_shared<Value>(frame.values[result].deepCopy());
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::ExtractTuple)
      {
        const auto &tuple = frame.values.at(instruction.operands[1]).asTuple();
        const uint32_t index = instruction.operands[2];
        if (index >= tuple.size())
          throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", index, tuple.size()));
        if (frame.values.size() <= instruction.operands[0]) frame.values.resize(instruction.operands[0] + 1);
        frame.values[instruction.operands[0]] = tuple[index].deepCopy();
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::MakeRef)
      {
        detail::makeRefInstruction(instruction, frame.values, frame.locals);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::LoadRef)
      {
        detail::loadRefInstruction(instruction, frame.values);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::AssignPlace)
      {
        detail::assignPlaceInstruction(instruction, frame.values, frame.locals);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::LoadVariant)
      {
        detail::loadVariantInstruction(instruction, frame.values);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::RangeStart)
      {
        if (frame.values.size() <= instruction.operands[0]) frame.values.resize(instruction.operands[0] + 1);
        frame.values[instruction.operands[0]] = Value::integer(frame.values.at(instruction.operands[1]).asRange().start);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::ArrayLength)
      {
        const auto &source = frame.values.at(instruction.operands[1]);
        int64_t length = source.isRange() ? source.asRange().end - source.asRange().start
                                          : static_cast<int64_t>(source.asArray().size());
        if (frame.values.size() <= instruction.operands[0]) frame.values.resize(instruction.operands[0] + 1);
        frame.values[instruction.operands[0]] = Value::integer(length);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::AppendArray)
      {
        if (frame.values.size() <= instruction.operands[0]) frame.values.resize(instruction.operands[0] + 1);
        // Deep-copy the source so appending never aliases the receiver.
        frame.values[instruction.operands[0]] = frame.values.at(instruction.operands[1]).deepCopy();
        frame.values[instruction.operands[0]].asArrayMut().push_back(frame.values.at(instruction.operands[2]).deepCopy());
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::EnumListLength)
      {
        detail::enumListLengthInstruction(instruction, frame.values);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::EnumListGet)
      {
        detail::enumListGetInstruction(instruction, frame.values);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::Slice)
      {
        const auto &receiver = frame.values.at(instruction.operands[1]);
        const auto &range = frame.values.at(instruction.operands[2]).asRange();
        const auto &array = receiver.asArray();
        if (range.start < 0 || range.end < range.start || static_cast<uint64_t>(range.end) > array.size())
          throw bytecode::BytecodeError(std::format("array slice out of bounds: [{}..{}) of length {}", range.start, range.end,
                                                    array.size()));
        std::vector<Value> elements;
        for (int64_t index = range.start; index < range.end; ++index) elements.push_back(array[static_cast<size_t>(index)].deepCopy());
        if (frame.values.size() <= instruction.operands[0]) frame.values.resize(instruction.operands[0] + 1);
        frame.values[instruction.operands[0]] = Value::array(std::move(elements));
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::SpliceTuple)
      {
        if (frame.values.size() <= instruction.operands[0]) frame.values.resize(instruction.operands[0] + 1);
        std::vector<Value> elements;
        for (size_t index = 0; index < instruction.operands[1]; ++index)
        {
          const auto &operand = frame.values.at(instruction.operands[2 + index]);
          if (operand.isTuple())
            for (const auto &element : operand.asTuple()) elements.push_back(element.deepCopy());
          else
            elements.push_back(operand.deepCopy());
        }
        frame.values[instruction.operands[0]] = Value::tuple(std::move(elements));
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::ExtractPayload)
      {
        detail::extractPayloadInstruction(instruction, frame.values);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::CallTrait)
      {
        const auto &view = frame.values.at(instruction.operands[1]).asTraitView();
        const uint64_t key = (static_cast<uint64_t>(view.trait) << 32) | view.concrete;
        const auto table = module.vtables.find(key);
        if (table == module.vtables.end()) throw bytecode::BytecodeError("trait dispatch table is missing");
        if (instruction.operands[2] >= table->second.size())
          throw bytecode::BytecodeError("trait method index is out of range");
        const uint32_t target = table->second[instruction.operands[2]];
        std::vector<Value> callArguments;
        callArguments.push_back(Value::reference(view.root, view.steps, false));
        for (uint32_t index = 0; index < instruction.operands[3]; ++index)
          callArguments.push_back(frame.values.at(instruction.operands[4 + index]).deepCopy());
        frames.push_back(makeFrame(target, callArguments, instruction.operands[0]));
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::MakeTraitView)
      {
        detail::makeTraitViewInstruction(instruction, frame.values, frame.locals);
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::Call)
      {
        std::vector<Value> callArguments;
        for (size_t index = 0; index < instruction.operands[2]; ++index)
          callArguments.push_back(frame.values.at(instruction.operands[3 + index]).deepCopy());
        const uint32_t target = instruction.operands[1];
        const auto &targetFunction = module.functions.at(target);
        if (targetFunction.nativeFunction)
        {
          const auto *entry = natives != nullptr ? natives->lookup(targetFunction.name) : nullptr;
          if (entry == nullptr || !entry->function)
            throw bytecode::BytecodeError(std::format("native function `{}` is not registered", targetFunction.name));
          const uint32_t destination = instruction.operands[0];
          std::vector<typecheck::TypeId> parameterTypes;
          if (!entry->signature.parameters.empty())
          {
            // R9 declared signature: validate arity and pass the declared
            // parameter types (the authoritative typing guidance).
            if (callArguments.size() != entry->signature.parameters.size())
              throw bytecode::BytecodeError(std::format("native `{}` expects {} argument(s), got {}",
                                                        targetFunction.name, entry->signature.parameters.size(),
                                                        callArguments.size()));
            parameterTypes = entry->signature.parameters;
          }
          else
          {
            parameterTypes.reserve(targetFunction.parameterLocals.size());
            for (const auto local : targetFunction.parameterLocals)
            {
              const auto found = targetFunction.localTypes.find(local);
              if (found != targetFunction.localTypes.end()) parameterTypes.push_back(found->second);
            }
          }
          if (frame.values.size() <= destination) frame.values.resize(destination + 1);
          frame.values[destination] = entry->function(callArguments, parameterTypes).deepCopy();
          continue;
        }
        frames.push_back(makeFrame(target, callArguments, instruction.operands[0]));
        continue;
      }
      if (instruction.opcode == bytecode::Opcode::Return)
      {
        std::optional<Value> value;
        if (instruction.operands[0] == 1) value = frame.values.at(instruction.operands[1]).deepCopy();
        const auto destination = frame.callerDestination;
        frames.pop_back();
        if (frames.empty()) return RunResult{.reason = HaltReason::Return, .executedInstructions = executed, .tailRecursions = tailRecursions, .returnValue = value};
        auto &caller = frames.back();
        if (caller.values.size() <= *destination) caller.values.resize(*destination + 1);
        caller.values[*destination] = value.value_or(Value{}).deepCopy();
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
      for (size_t index = 0; index < function.parameterLocals.size(); ++index)
        frame.locals[function.parameterLocals[index]] =
            std::make_shared<Value>(frame.values.at(instruction.operands[1 + index]).deepCopy());
      ++tailRecursions;
      frame.programCounter = blockInstruction(0);
    }
    return RunResult{.reason = HaltReason::FuelExhausted, .executedInstructions = executed, .tailRecursions = tailRecursions, .returnValue = std::nullopt};
  }
} // namespace NG::vm
