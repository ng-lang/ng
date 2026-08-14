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
        OpcodeDescriptor{Opcode::Evaluate, "evaluate", OperandLayout::CountPrefixedTail, 4},
        OpcodeDescriptor{Opcode::Call, "call", OperandLayout::CountPrefixedTail, 2},
        OpcodeDescriptor{Opcode::BindLocal, "bind_local", OperandLayout::Fixed, 3},
        OpcodeDescriptor{Opcode::ExtractTuple, "extract_tuple", OperandLayout::Fixed, 3},
        OpcodeDescriptor{Opcode::MakeRef, "make_ref", OperandLayout::CountPrefixedTail, 3},
        OpcodeDescriptor{Opcode::LoadRef, "load_ref", OperandLayout::Fixed, 2},
        OpcodeDescriptor{Opcode::AssignPlace, "assign_place", OperandLayout::CountPrefixedTail, 3},
        OpcodeDescriptor{Opcode::LoadVariant, "load_variant", OperandLayout::Fixed, 2},
        OpcodeDescriptor{Opcode::ExtractPayload, "extract_payload", OperandLayout::Fixed, 2},
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
    Function result{.source = flow.source};
    result.valueTypes = flow.valueTypes;
    result.localTypes = flow.localTypes;
    result.typeDescriptors = flow.typeDescriptors;
    for (const auto local : flow.parameterLocals) result.parameterLocals.push_back(local.value);
    result.blockParameterCounts.reserve(flow.blocks.size());
    result.blockParameterLocals.reserve(flow.blocks.size());
    for (const auto &block : flow.blocks)
    {
      result.blockParameterCounts.push_back(static_cast<uint32_t>(block.parameterCount));
      std::vector<uint32_t> locals;
      for (const auto local : block.parameterLocals) locals.push_back(local.value);
      result.blockParameterLocals.push_back(std::move(locals));
      result.blockOffsets.push_back(static_cast<uint32_t>(result.code.size()));
      for (const auto &instruction : block.instructions)
      {
        if (instruction.kind == flowir::InstructionKind::Evaluate)
        {
          if (instruction.callTarget.has_value())
          {
            std::vector<uint32_t> operands{instruction.result.value, instruction.callTarget->value,
                                           static_cast<uint32_t>(instruction.operands.size())};
            for (const auto value : instruction.operands) operands.push_back(value.value);
            appendInstruction(result.code, Opcode::Call, operands);
          }
          else
          {
            int64_t encodedPayload = instruction.payload;
            if (instruction.expressionKind == hir::ExpressionKind::StringLiteral)
            {
              const auto existing = std::find(result.stringConstants.begin(), result.stringConstants.end(), instruction.text);
              if (existing == result.stringConstants.end())
              {
                result.stringConstants.push_back(instruction.text);
                encodedPayload = static_cast<int64_t>(result.stringConstants.size() - 1);
              }
              else
              {
                encodedPayload = static_cast<int64_t>(std::distance(result.stringConstants.begin(), existing));
              }
            }
            const uint64_t payload = static_cast<uint64_t>(encodedPayload);
            std::vector<uint32_t> operands{instruction.result.value, static_cast<uint32_t>(instruction.expressionKind),
                                           static_cast<uint32_t>(payload), static_cast<uint32_t>(payload >> 32),
                                           static_cast<uint32_t>(instruction.operands.size())};
            for (const auto value : instruction.operands) operands.push_back(value.value);
            appendInstruction(result.code, Opcode::Evaluate, operands);
          }
        }
        else if (instruction.kind == flowir::InstructionKind::BindLocal)
        {
          appendInstruction(result.code, Opcode::BindLocal,
                            {instruction.result.value, instruction.local->value, instruction.source->value});
        }
        else if (instruction.kind == flowir::InstructionKind::ExtractTuple)
        {
          appendInstruction(result.code, Opcode::ExtractTuple,
                            {instruction.result.value, instruction.source->value, static_cast<uint32_t>(instruction.payload)});
        }
        else if (instruction.kind == flowir::InstructionKind::MakeRef)
        {
          std::vector<uint32_t> operands{instruction.result.value, instruction.placeRootLocal->value,
                                         instruction.placeMutable ? 1u : 0u,
                                         static_cast<uint32_t>(instruction.placeSteps.size() * 2)};
          for (const auto &step : instruction.placeSteps)
          {
            operands.push_back(step.kind == flowir::PlaceStep::Kind::Member ? 0u : 1u);
            operands.push_back(step.kind == flowir::PlaceStep::Kind::Member ? static_cast<uint32_t>(step.field)
                                                                            : step.indexValue.value);
          }
          appendInstruction(result.code, Opcode::MakeRef, operands);
        }
        else if (instruction.kind == flowir::InstructionKind::LoadRef)
        {
          appendInstruction(result.code, Opcode::LoadRef, {instruction.result.value, instruction.operands[0].value});
        }
        else if (instruction.kind == flowir::InstructionKind::EnumVariantIndex)
        {
          appendInstruction(result.code, Opcode::LoadVariant, {instruction.result.value, instruction.source->value});
        }
        else if (instruction.kind == flowir::InstructionKind::ExtractEnumPayload)
        {
          appendInstruction(result.code, Opcode::ExtractPayload, {instruction.result.value, instruction.source->value});
        }
        else
        {
          std::vector<uint32_t> operands{instruction.placeRootLocal.has_value() ? 0u : 1u,
                                         instruction.placeRootLocal.has_value() ? instruction.placeRootLocal->value
                                                                                : instruction.placeRootRef->value,
                                         instruction.operands.back().value,
                                         static_cast<uint32_t>(instruction.placeSteps.size() * 2)};
          for (const auto &step : instruction.placeSteps)
          {
            operands.push_back(step.kind == flowir::PlaceStep::Kind::Member ? 0u : 1u);
            operands.push_back(step.kind == flowir::PlaceStep::Kind::Member ? static_cast<uint32_t>(step.field)
                                                                            : step.indexValue.value);
          }
          appendInstruction(result.code, Opcode::AssignPlace, operands);
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

  auto ModuleCompiler::compile(const std::vector<flowir::Function> &functions) const -> Module
  {
    Module module;
    module.functions.reserve(functions.size());
    Compiler compiler;
    for (const auto &function : functions)
    {
      module.functions.push_back(compiler.compile(function));
    }
    return module;
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
    if (function.blockOffsets.size() != function.blockParameterCounts.size() ||
        function.blockParameterLocals.size() != function.blockParameterCounts.size())
    {
      throw BytecodeError("bytecode block metadata tables do not match");
    }
    for (size_t index = 0; index < function.blockParameterCounts.size(); ++index)
    {
      if (function.blockParameterLocals[index].size() != function.blockParameterCounts[index])
      {
        throw BytecodeError("bytecode block parameter locals do not match parameter count");
      }
    }
    if ((!function.valueTypes.empty() || !function.localTypes.empty()) && function.typeDescriptors.empty())
      throw BytecodeError("bytecode typed function has no type descriptors");
    const auto verifyTypeId = [&function](typecheck::TypeId type) {
      if (type.value == 0 || type.value >= function.typeDescriptors.size())
        throw BytecodeError("bytecode type metadata is out of range");
    };
    for (const auto &[_, type] : function.valueTypes) verifyTypeId(type);
    for (const auto &[_, type] : function.localTypes) verifyTypeId(type);
    for (size_t index = 1; index < function.typeDescriptors.size(); ++index)
    {
      const auto &descriptor = function.typeDescriptors[index];
      if (descriptor.kind != typecheck::TypeKind::Builtin && descriptor.kind != typecheck::TypeKind::DynamicArray &&
          descriptor.kind != typecheck::TypeKind::FixedArray && descriptor.kind != typecheck::TypeKind::DependentArray &&
          descriptor.kind != typecheck::TypeKind::Reference && descriptor.kind != typecheck::TypeKind::RawPointer &&
          descriptor.kind != typecheck::TypeKind::Tuple &&
          descriptor.kind != typecheck::TypeKind::Struct && descriptor.kind != typecheck::TypeKind::Enum &&
          descriptor.kind != typecheck::TypeKind::TypeParameter)
        throw BytecodeError("bytecode type descriptor kind is invalid");
      if (descriptor.kind == typecheck::TypeKind::Reference || descriptor.kind == typecheck::TypeKind::RawPointer)
        verifyTypeId(descriptor.element);
      if (descriptor.kind == typecheck::TypeKind::DynamicArray || descriptor.kind == typecheck::TypeKind::FixedArray ||
          descriptor.kind == typecheck::TypeKind::DependentArray)
      {
        verifyTypeId(descriptor.element);
        if (descriptor.kind == typecheck::TypeKind::DynamicArray && descriptor.length.has_value())
          throw BytecodeError("bytecode dynamic array descriptor has a fixed length");
        if (descriptor.kind == typecheck::TypeKind::FixedArray && !descriptor.length.has_value())
          throw BytecodeError("bytecode fixed array descriptor has no length");
        if (descriptor.kind == typecheck::TypeKind::DependentArray && descriptor.length.has_value())
          throw BytecodeError("bytecode dependent array descriptor has a fixed length");
      }
      if (descriptor.kind == typecheck::TypeKind::Tuple || descriptor.kind == typecheck::TypeKind::Struct ||
          descriptor.kind == typecheck::TypeKind::Enum)
      {
        if (!descriptor.length.has_value() || *descriptor.length != descriptor.elements.size())
          throw BytecodeError("bytecode product descriptor length mismatch");
        if ((descriptor.kind == typecheck::TypeKind::Struct || descriptor.kind == typecheck::TypeKind::Enum) &&
            descriptor.fieldNames.size() != descriptor.elements.size())
          throw BytecodeError("bytecode nominal descriptor member count mismatch");
        if (descriptor.kind == typecheck::TypeKind::Enum && descriptor.variantHasPayload.size() != descriptor.elements.size())
          throw BytecodeError("bytecode enum descriptor payload flag count mismatch");
        for (const auto element : descriptor.elements) verifyTypeId(element);
      }
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
      if (!function.valueTypes.empty())
      {
        const auto requireValueType = [&function](uint32_t value) -> typecheck::TypeId {
          if (const auto type = function.valueTypes.find(value); type != function.valueTypes.end()) return type->second;
          throw BytecodeError("bytecode value is missing type metadata");
        };
        const auto stepType = [&function](typecheck::TypeId current, uint32_t kind, uint32_t payload) -> typecheck::TypeId {
          if (current.value == 0 || current.value >= function.typeDescriptors.size())
            throw BytecodeError("bytecode place step type is out of range");
          const auto &descriptor = function.typeDescriptors[current.value];
          if (kind == 0)
          {
            if (descriptor.kind == typecheck::TypeKind::DynamicArray || descriptor.kind == typecheck::TypeKind::FixedArray ||
                descriptor.kind == typecheck::TypeKind::DependentArray)
              return descriptor.element;
            if (descriptor.kind != typecheck::TypeKind::Struct && descriptor.kind != typecheck::TypeKind::Tuple)
              throw BytecodeError("bytecode place member step receiver is not a product type");
            if (payload >= descriptor.elements.size()) throw BytecodeError("bytecode place member step is out of range");
            return descriptor.elements[payload];
          }
          if (descriptor.kind != typecheck::TypeKind::DynamicArray && descriptor.kind != typecheck::TypeKind::FixedArray &&
              descriptor.kind != typecheck::TypeKind::DependentArray)
            throw BytecodeError("bytecode place index step receiver is not an aggregate type");
          return descriptor.element;
        };
        if (instruction.opcode == Opcode::Evaluate)
        {
          static_cast<void>(requireValueType(instruction.operands[0]));
          const auto kind = static_cast<hir::ExpressionKind>(instruction.operands[1]);
          const auto resultType = requireValueType(instruction.operands[0]);
          const auto requireOperandType = [&instruction, &requireValueType](size_t index, typecheck::TypeId expected) {
            if (requireValueType(instruction.operands.at(5 + index)) != expected)
              throw BytecodeError("bytecode operation operand type mismatch");
          };
          const auto requireResultType = [resultType](typecheck::TypeId expected) {
            if (resultType != expected) throw BytecodeError("bytecode operation result type mismatch");
          };
          if (kind == hir::ExpressionKind::IntegerLiteral) requireResultType(typecheck::builtin::I64);
          else if (kind == hir::ExpressionKind::StringLiteral) requireResultType(typecheck::builtin::String);
          else if (kind == hir::ExpressionKind::ArrayLiteral)
          {
            if (resultType.value >= function.typeDescriptors.size())
              throw BytecodeError("bytecode value type descriptor is out of range");
            const auto &array = function.typeDescriptors[resultType.value];
            if (array.kind != typecheck::TypeKind::DynamicArray && array.kind != typecheck::TypeKind::FixedArray &&
                array.kind != typecheck::TypeKind::DependentArray)
              throw BytecodeError("bytecode array literal result is not an array type");
            if (array.kind == typecheck::TypeKind::FixedArray && instruction.operands.at(4) != *array.length)
              throw BytecodeError("bytecode fixed array literal length mismatch");
            for (size_t index = 0; index < instruction.operands.at(4); ++index) requireOperandType(index, array.element);
          }
          else if (kind == hir::ExpressionKind::TupleLiteral)
          {
            if (resultType.value >= function.typeDescriptors.size())
              throw BytecodeError("bytecode value type descriptor is out of range");
            const auto &tuple = function.typeDescriptors[resultType.value];
            if (tuple.kind != typecheck::TypeKind::Tuple)
              throw BytecodeError("bytecode tuple literal result is not a tuple type");
            if (instruction.operands.at(4) != tuple.elements.size())
              throw BytecodeError("bytecode tuple literal length mismatch");
            for (size_t index = 0; index < tuple.elements.size(); ++index) requireOperandType(index, tuple.elements[index]);
          }
          else if (kind == hir::ExpressionKind::EnumLiteral)
          {
            if (resultType.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
            const auto &enumeration = function.typeDescriptors[resultType.value];
            if (enumeration.kind != typecheck::TypeKind::Enum) throw BytecodeError("bytecode enum literal result is not an enum type");
            const uint32_t variant = instruction.operands[3];
            if (variant >= enumeration.elements.size()) throw BytecodeError("bytecode enum variant is out of range");
            const size_t expected = enumeration.variantHasPayload[variant] ? 1 : 0;
            if (instruction.operands.at(4) != expected) throw BytecodeError("bytecode enum payload count mismatch");
            if (expected == 1) requireOperandType(0, enumeration.elements[variant]);
          }
          else if (kind == hir::ExpressionKind::StructLiteral)
          {
            if (resultType.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
            const auto &structure = function.typeDescriptors[resultType.value];
            if (structure.kind != typecheck::TypeKind::Struct)
              throw BytecodeError("bytecode struct literal result is not a struct type");
            if (instruction.operands.at(4) != structure.elements.size())
              throw BytecodeError("bytecode struct literal field count mismatch");
            for (size_t index = 0; index < structure.elements.size(); ++index) requireOperandType(index, structure.elements[index]);
          }
          else if (kind == hir::ExpressionKind::BooleanLiteral) requireResultType(typecheck::builtin::Bool);
          else if (kind == hir::ExpressionKind::Member)
          {
            const auto receiver = requireValueType(instruction.operands.at(5));
            if (receiver.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
            const auto &structure = function.typeDescriptors[receiver.value];
            if (structure.kind != typecheck::TypeKind::Struct) throw BytecodeError("bytecode member receiver is not a struct");
            const uint64_t field = static_cast<uint64_t>(instruction.operands[2]) |
                                   (static_cast<uint64_t>(instruction.operands[3]) << 32);
            if (field >= structure.elements.size()) throw BytecodeError("bytecode struct field is out of range");
            requireResultType(structure.elements[field]);
          }
          else if (kind == hir::ExpressionKind::Index)
          {
            const auto receiver = requireValueType(instruction.operands.at(5));
            if (receiver.value >= function.typeDescriptors.size())
              throw BytecodeError("bytecode value type descriptor is out of range");
            const auto &aggregate = function.typeDescriptors[receiver.value];
            requireOperandType(1, typecheck::builtin::I64);
            if (aggregate.kind == typecheck::TypeKind::Tuple)
            {
              const uint64_t index = static_cast<uint64_t>(instruction.operands[2]) |
                                     (static_cast<uint64_t>(instruction.operands[3]) << 32);
              if (index >= aggregate.elements.size()) throw BytecodeError("bytecode tuple projection is out of range");
              requireResultType(aggregate.elements[index]);
            }
            else
            {
              if (aggregate.kind != typecheck::TypeKind::DynamicArray && aggregate.kind != typecheck::TypeKind::FixedArray &&
                  aggregate.kind != typecheck::TypeKind::DependentArray)
                throw BytecodeError("bytecode index receiver is not an aggregate type");
              requireResultType(aggregate.element);
            }
          }
          else if (kind == hir::ExpressionKind::ResolvedName)
          {
            const uint32_t local = instruction.operands[2];
            if (!function.localTypes.contains(local)) throw BytecodeError("bytecode local is missing type metadata");
            if (resultType != function.localTypes.at(local)) throw BytecodeError("bytecode local read type does not match result type");
          }
          else if (kind == hir::ExpressionKind::Grouped)
          {
            if (resultType != requireValueType(instruction.operands.at(5)))
              throw BytecodeError("bytecode operation result type mismatch");
          }
          else if (kind == hir::ExpressionKind::Prefix)
          {
            const uint64_t payload = static_cast<uint64_t>(instruction.operands[2]) | (static_cast<uint64_t>(instruction.operands[3]) << 32);
            if (payload == 4 || payload == 5)
            {
              requireResultType(requireValueType(instruction.operands.at(5)));
            }
            else
            {
              const auto expected = payload == 1 ? typecheck::builtin::Bool : typecheck::builtin::I64;
              requireOperandType(0, expected);
              requireResultType(expected);
            }
          }
          else if (kind == hir::ExpressionKind::Binary)
          {
            const uint64_t payload = static_cast<uint64_t>(instruction.operands[2]) | (static_cast<uint64_t>(instruction.operands[3]) << 32);
            if (payload == 1 && requireValueType(instruction.operands.at(5)) == typecheck::builtin::String)
            {
              requireOperandType(1, typecheck::builtin::String);
              requireResultType(typecheck::builtin::String);
            }
            else if ((payload >= 1 && payload <= 5) || (payload >= 14 && payload <= 18))
            {
              requireOperandType(0, typecheck::builtin::I64);
              requireOperandType(1, typecheck::builtin::I64);
              requireResultType(typecheck::builtin::I64);
            }
            else if (payload >= 8 && payload <= 11)
            {
              requireOperandType(0, typecheck::builtin::I64);
              requireOperandType(1, typecheck::builtin::I64);
              requireResultType(typecheck::builtin::Bool);
            }
            else if (payload == 12 || payload == 13)
            {
              requireOperandType(0, typecheck::builtin::Bool);
              requireOperandType(1, typecheck::builtin::Bool);
              requireResultType(typecheck::builtin::Bool);
            }
            else if (payload == 6 || payload == 7)
            {
              if (requireValueType(instruction.operands.at(5)) != requireValueType(instruction.operands.at(6)))
                throw BytecodeError("bytecode equality operand type mismatch");
              requireResultType(typecheck::builtin::Bool);
            }
          }
        }
        else if (instruction.opcode == Opcode::BindLocal)
        {
          const uint32_t result = instruction.operands[0];
          const uint32_t local = instruction.operands[1];
          const uint32_t source = instruction.operands[2];
          if (!function.localTypes.contains(local)) throw BytecodeError("bytecode local is missing type metadata");
          if (requireValueType(result) != function.localTypes.at(local) ||
              requireValueType(source) != function.localTypes.at(local))
            throw BytecodeError("bytecode local binding type mismatch");
        }
        else if (instruction.opcode == Opcode::ExtractTuple)
        {
          const auto receiver = requireValueType(instruction.operands[1]);
          if (receiver.value >= function.typeDescriptors.size())
            throw BytecodeError("bytecode value type descriptor is out of range");
          const auto &tuple = function.typeDescriptors[receiver.value];
          if (tuple.kind != typecheck::TypeKind::Tuple)
            throw BytecodeError("bytecode tuple extraction source is not a tuple");
          const uint32_t index = instruction.operands[2];
          if (index >= tuple.elements.size()) throw BytecodeError("bytecode tuple extraction is out of range");
          if (requireValueType(instruction.operands[0]) != tuple.elements[index])
            throw BytecodeError("bytecode tuple extraction result type mismatch");
        }
        else if (instruction.opcode == Opcode::MakeRef)
        {
          const auto resultType = requireValueType(instruction.operands[0]);
          if (resultType.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
          const auto &reference = function.typeDescriptors[resultType.value];
          if (reference.kind != typecheck::TypeKind::Reference) throw BytecodeError("bytecode reference result is not a reference type");
          if ((instruction.operands[2] != 0) != reference.referenceMutable)
            throw BytecodeError("bytecode reference mutability does not match result type");
          if (!function.localTypes.contains(instruction.operands[1])) throw BytecodeError("bytecode reference root is missing type metadata");
          typecheck::TypeId current = function.localTypes.at(instruction.operands[1]);
          const uint32_t count = instruction.operands[3];
          if (count % 2 != 0) throw BytecodeError("bytecode reference step encoding is malformed");
          for (uint32_t index = 0; index < count; index += 2)
          {
            if (instruction.operands.at(4 + index) == 1 && requireValueType(instruction.operands.at(4 + index + 1)) != typecheck::builtin::I64)
              throw BytecodeError("bytecode reference index step is not i64");
            current = stepType(current, instruction.operands.at(4 + index), instruction.operands.at(4 + index + 1));
          }
          if (reference.element != current) throw BytecodeError("bytecode reference element type mismatch");
        }
        else if (instruction.opcode == Opcode::LoadRef)
        {
          const auto source = requireValueType(instruction.operands[1]);
          if (source.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
          const auto &reference = function.typeDescriptors[source.value];
          if (reference.kind != typecheck::TypeKind::Reference) throw BytecodeError("bytecode reference load source is not a reference type");
          if (requireValueType(instruction.operands[0]) != reference.element)
            throw BytecodeError("bytecode reference load result type mismatch");
        }
        else if (instruction.opcode == Opcode::AssignPlace)
        {
          const bool refRoot = instruction.operands[0] != 0;
          typecheck::TypeId current{};
          if (refRoot)
          {
            const auto reference = requireValueType(instruction.operands[1]);
            if (reference.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
            const auto &descriptor = function.typeDescriptors[reference.value];
            if (descriptor.kind != typecheck::TypeKind::Reference)
              throw BytecodeError("bytecode place assignment reference root is not a reference type");
            current = descriptor.element;
          }
          else
          {
            if (!function.localTypes.contains(instruction.operands[1])) throw BytecodeError("bytecode place root is missing type metadata");
            current = function.localTypes.at(instruction.operands[1]);
          }
          const uint32_t count = instruction.operands[3];
          if (count % 2 != 0) throw BytecodeError("bytecode place step encoding is malformed");
          for (uint32_t index = 0; index < count; index += 2)
          {
            if (instruction.operands.at(4 + index) == 1 && requireValueType(instruction.operands.at(4 + index + 1)) != typecheck::builtin::I64)
              throw BytecodeError("bytecode place index step is not i64");
            current = stepType(current, instruction.operands.at(4 + index), instruction.operands.at(4 + index + 1));
          }
          if (requireValueType(instruction.operands[2]) != current)
            throw BytecodeError("bytecode place assignment value type mismatch");
        }
        else if (instruction.opcode == Opcode::LoadVariant)
        {
          const auto source = requireValueType(instruction.operands[1]);
          if (source.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
          if (function.typeDescriptors[source.value].kind != typecheck::TypeKind::Enum)
            throw BytecodeError("bytecode variant load source is not an enum type");
          if (requireValueType(instruction.operands[0]) != typecheck::builtin::I64)
            throw BytecodeError("bytecode variant load result is not i64");
        }
        else if (instruction.opcode == Opcode::ExtractPayload)
        {
          const auto source = requireValueType(instruction.operands[1]);
          if (source.value >= function.typeDescriptors.size()) throw BytecodeError("bytecode value type descriptor is out of range");
          const auto &enumeration = function.typeDescriptors[source.value];
          if (enumeration.kind != typecheck::TypeKind::Enum)
            throw BytecodeError("bytecode payload extraction source is not an enum type");
          const auto resultType = requireValueType(instruction.operands[0]);
          if (std::find(enumeration.elements.begin(), enumeration.elements.end(), resultType) == enumeration.elements.end())
            throw BytecodeError("bytecode payload extraction result does not match any variant payload type");
        }
        else if (instruction.opcode == Opcode::Branch)
        {
          if (requireValueType(instruction.operands[0]) != typecheck::builtin::Bool)
            throw BytecodeError("bytecode branch condition is not bool");
        }
      }
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
