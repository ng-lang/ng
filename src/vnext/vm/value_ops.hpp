// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/bytecode.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace NG::vnext::vm::detail
{
  /// Evaluates one decoded value-producing instruction against a frame's
  /// value/register and local-binding stores. Control-transfer instructions
  /// intentionally remain owned by the single- and module-VM dispatch loops.
  void evaluateInstruction(const bytecode::DecodedInstruction &instruction, std::vector<int64_t> &values,
                           const std::unordered_map<uint32_t, int64_t> &locals);
} // namespace NG::vnext::vm::detail
