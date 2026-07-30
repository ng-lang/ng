// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/bytecode.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace NG::vnext::bytecode
{
  namespace
  {
    constexpr std::array<uint8_t, 4> ArtifactMagic{'N', 'G', 'V', 'X'};
    constexpr uint32_t ArtifactVersion{1};

    void appendU32(std::vector<uint8_t> &output, uint32_t value)
    {
      for (size_t index = 0; index < 4; ++index) output.push_back(static_cast<uint8_t>(value >> (index * 8)));
    }

    [[nodiscard]] auto readU32(const std::vector<uint8_t> &input, size_t &offset) -> uint32_t
    {
      if (input.size() - offset < 4) throw BytecodeError("truncated bytecode artifact");
      uint32_t value{};
      for (size_t index = 0; index < 4; ++index) value |= static_cast<uint32_t>(input[offset++]) << (index * 8);
      return value;
    }

    [[nodiscard]] auto narrowSize(size_t value) -> uint32_t
    {
      if (value > std::numeric_limits<uint32_t>::max()) throw BytecodeError("bytecode artifact field is too large");
      return static_cast<uint32_t>(value);
    }

    void appendStringVector(std::vector<uint8_t> &output, const std::vector<std::string> &values)
    {
      appendU32(output, narrowSize(values.size()));
      for (const auto &value : values)
      {
        appendU32(output, narrowSize(value.size()));
        output.insert(output.end(), value.begin(), value.end());
      }
    }

    [[nodiscard]] auto readStringVector(const std::vector<uint8_t> &input, size_t &offset) -> std::vector<std::string>
    {
      const uint32_t count = readU32(input, offset);
      std::vector<std::string> values;
      values.reserve(count);
      for (uint32_t index = 0; index < count; ++index)
      {
        const uint32_t size = readU32(input, offset);
        if (size > input.size() - offset) throw BytecodeError("truncated bytecode artifact");
        values.emplace_back(input.begin() + static_cast<std::ptrdiff_t>(offset),
                            input.begin() + static_cast<std::ptrdiff_t>(offset + size));
        offset += size;
      }
      return values;
    }

    void appendTypeDescriptors(std::vector<uint8_t> &output, const std::vector<typecheck::TypeDescriptor> &descriptors)
    {
      appendU32(output, narrowSize(descriptors.size()));
      for (const auto &descriptor : descriptors)
      {
        appendU32(output, static_cast<uint32_t>(descriptor.kind));
        appendStringVector(output, {descriptor.name});
        appendU32(output, descriptor.element.value);
        appendU32(output, descriptor.length.has_value() ? 1 : 0);
        if (descriptor.length.has_value())
        {
          appendU32(output, static_cast<uint32_t>(*descriptor.length));
          appendU32(output, static_cast<uint32_t>(*descriptor.length >> 32));
        }
      }
    }

    [[nodiscard]] auto readTypeDescriptors(const std::vector<uint8_t> &input, size_t &offset)
        -> std::vector<typecheck::TypeDescriptor>
    {
      const uint32_t count = readU32(input, offset);
      std::vector<typecheck::TypeDescriptor> descriptors;
      descriptors.reserve(count);
      for (uint32_t index = 0; index < count; ++index)
      {
        const auto kind = static_cast<typecheck::TypeKind>(readU32(input, offset));
        const auto names = readStringVector(input, offset);
        if (names.size() != 1) throw BytecodeError("invalid bytecode type descriptor name");
        const typecheck::TypeId element{readU32(input, offset)};
        const uint32_t hasLength = readU32(input, offset);
        if (hasLength > 1) throw BytecodeError("invalid bytecode type descriptor length flag");
        std::optional<uint64_t> length;
        if (hasLength == 1)
        {
          const uint64_t low = readU32(input, offset);
          length = low | (static_cast<uint64_t>(readU32(input, offset)) << 32);
        }
        descriptors.push_back(typecheck::TypeDescriptor{.kind = kind, .name = names.front(), .element = element, .length = length});
      }
      return descriptors;
    }

    void appendU32Vector(std::vector<uint8_t> &output, const std::vector<uint32_t> &values)
    {
      appendU32(output, narrowSize(values.size()));
      for (const auto value : values) appendU32(output, value);
    }

    [[nodiscard]] auto readU32Vector(const std::vector<uint8_t> &input, size_t &offset) -> std::vector<uint32_t>
    {
      const uint32_t count = readU32(input, offset);
      if (count > (input.size() - offset) / 4) throw BytecodeError("truncated bytecode artifact");
      std::vector<uint32_t> values;
      values.reserve(count);
      for (uint32_t index = 0; index < count; ++index) values.push_back(readU32(input, offset));
      return values;
    }

    void appendTypeMap(std::vector<uint8_t> &output, const std::unordered_map<uint32_t, typecheck::TypeId> &types)
    {
      std::vector<std::pair<uint32_t, typecheck::TypeId>> sorted{types.begin(), types.end()};
      std::sort(sorted.begin(), sorted.end(), [](const auto &left, const auto &right) { return left.first < right.first; });
      appendU32(output, narrowSize(sorted.size()));
      for (const auto &[id, type] : sorted)
      {
        appendU32(output, id);
        appendU32(output, type.value);
      }
    }

    [[nodiscard]] auto readTypeMap(const std::vector<uint8_t> &input, size_t &offset)
        -> std::unordered_map<uint32_t, typecheck::TypeId>
    {
      const uint32_t count = readU32(input, offset);
      if (count > (input.size() - offset) / 8) throw BytecodeError("truncated bytecode artifact");
      std::unordered_map<uint32_t, typecheck::TypeId> types;
      for (uint32_t index = 0; index < count; ++index)
      {
        const uint32_t id = readU32(input, offset);
        if (!types.emplace(id, typecheck::TypeId{readU32(input, offset)}).second)
          throw BytecodeError("duplicate bytecode artifact type metadata");
      }
      return types;
    }

    void appendFunction(std::vector<uint8_t> &output, const Function &function)
    {
      appendU32(output, function.source.value);
      appendU32(output, narrowSize(function.code.size()));
      output.insert(output.end(), function.code.begin(), function.code.end());
      appendStringVector(output, function.stringConstants);
      appendU32Vector(output, function.parameterLocals);
      appendU32Vector(output, function.blockParameterCounts);
      appendU32(output, narrowSize(function.blockParameterLocals.size()));
      for (const auto &locals : function.blockParameterLocals) appendU32Vector(output, locals);
      appendU32Vector(output, function.blockOffsets);
      appendTypeMap(output, function.valueTypes);
      appendTypeMap(output, function.localTypes);
      appendTypeDescriptors(output, function.typeDescriptors);
    }

    [[nodiscard]] auto readFunction(const std::vector<uint8_t> &input, size_t &offset) -> Function
    {
      Function function{.source = hir::DefId{readU32(input, offset)}};
      const uint32_t codeSize = readU32(input, offset);
      if (codeSize > input.size() - offset) throw BytecodeError("truncated bytecode artifact");
      function.code.insert(function.code.end(), input.begin() + static_cast<std::ptrdiff_t>(offset),
                           input.begin() + static_cast<std::ptrdiff_t>(offset + codeSize));
      offset += codeSize;
      function.stringConstants = readStringVector(input, offset);
      function.parameterLocals = readU32Vector(input, offset);
      function.blockParameterCounts = readU32Vector(input, offset);
      const uint32_t blockLocalCount = readU32(input, offset);
      function.blockParameterLocals.reserve(blockLocalCount);
      for (uint32_t index = 0; index < blockLocalCount; ++index) function.blockParameterLocals.push_back(readU32Vector(input, offset));
      function.blockOffsets = readU32Vector(input, offset);
      function.valueTypes = readTypeMap(input, offset);
      function.localTypes = readTypeMap(input, offset);
      function.typeDescriptors = readTypeDescriptors(input, offset);
      Verifier{}.verify(function);
      return function;
    }
  } // namespace

  auto ArtifactCodec::serialize(const Module &module) const -> std::vector<uint8_t>
  {
    std::vector<uint8_t> artifact;
    artifact.insert(artifact.end(), ArtifactMagic.begin(), ArtifactMagic.end());
    appendU32(artifact, ArtifactVersion);
    appendU32(artifact, narrowSize(module.functions.size()));
    for (const auto &function : module.functions)
    {
      Verifier{}.verify(function);
      appendFunction(artifact, function);
    }
    return artifact;
  }

  auto ArtifactCodec::deserialize(const std::vector<uint8_t> &artifact) const -> Module
  {
    if (artifact.size() < ArtifactMagic.size() || !std::equal(ArtifactMagic.begin(), ArtifactMagic.end(), artifact.begin()))
      throw BytecodeError("invalid bytecode artifact magic");
    size_t offset = ArtifactMagic.size();
    if (readU32(artifact, offset) != ArtifactVersion) throw BytecodeError("unsupported bytecode artifact version");
    const uint32_t functionCount = readU32(artifact, offset);
    Module module;
    module.functions.reserve(functionCount);
    for (uint32_t index = 0; index < functionCount; ++index) module.functions.push_back(readFunction(artifact, offset));
    if (offset != artifact.size()) throw BytecodeError("trailing bytes in bytecode artifact");
    return module;
  }
} // namespace NG::vnext::bytecode
