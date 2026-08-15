// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "bytecode.hpp"
#include "native.hpp"
#include "value.hpp"
#include <cstddef>
#include <optional>
#include <cstdint>
#include <vector>

namespace NG::vm
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
    std::optional<Value> returnValue;
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
    [[nodiscard]] auto run(const bytecode::Module &module, hir::DefId entry, const std::vector<int64_t> &arguments = {},
                           size_t fuel = 100000, const NativeRegistry *natives = nullptr) const -> RunResult;
    [[nodiscard]] auto run(const bytecode::Module &module, hir::DefId entry, const std::vector<Value> &arguments,
                           size_t fuel = 100000, const NativeRegistry *natives = nullptr) const -> RunResult;
  };
} // namespace NG::vm
