// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/bytecode.hpp"
#include <cstddef>

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
  };

  /// Minimal frame-local vNext VM control core. Value execution is introduced
  /// incrementally; this slice establishes verified jump/backedge/tail-recur
  /// dispatch without recursion through the host call stack.
  class VM final
  {
  public:
    [[nodiscard]] auto run(const bytecode::Function &function, size_t fuel = 100000) const -> RunResult;
  };
} // namespace NG::vnext::vm
