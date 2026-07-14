// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/bytecode.hpp"
#include <cstddef>
#include <optional>
#include <cstdint>
#include <vector>

namespace NG::vnext::vm
{
  enum class HaltReason
  {
    Return,
    FuelExhausted,
  };

  struct RunResult
  {
    HaltReason reason;
    size_t executedInstructions;
    size_t tailRecursions;
    std::optional<int64_t> returnValue;
  };

  /// Minimal frame-local vNext VM control core. Value execution is introduced
  /// incrementally; this slice establishes verified jump/backedge/tail-recur
  /// dispatch without recursion through the host call stack.
  class VM final
  {
  public:
    [[nodiscard]] auto run(const bytecode::Function &function, size_t fuel = 100000) const -> RunResult;
    [[nodiscard]] auto run(const bytecode::Function &function, const std::vector<int64_t> &arguments,
                           size_t fuel = 100000) const -> RunResult;
  };
} // namespace NG::vnext::vm
