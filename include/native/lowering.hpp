// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "flowir.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace NG::native
{
  struct LoweringError : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };

  /// Lowers one FlowIR function to QBE IL text.
  ///
  /// M1 subset: scalar literals (integer/boolean/float), prefix and binary
  /// arithmetic/comparisons, locals as stack slots, block parameters as phi
  /// instructions, branch/jump/loop-backedge/tail-recur terminators, and
  /// return. Aggregates, calls, refs, and trait dispatch arrive in M2/M3.
  [[nodiscard]] auto lower(const flowir::Function &function) -> std::string;

  /// Lowers a module of FlowIR functions (native placeholders are skipped).
  [[nodiscard]] auto lowerModule(const std::vector<flowir::Function> &functions) -> std::string;
} // namespace NG::native
