// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "flowir.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NG::native
{
  struct LoweringError : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };

  /// DefId -> display-name table used to build callable QBE symbols for the
  /// whole module (the callee's name is not visible from a call site alone).
  using FunctionNames = std::unordered_map<uint32_t, std::string>;

  /// (trait type id << 32 | concrete type id) -> method DefIds in trait
  /// declaration order (the bytecode module's vtable shape).
  using VtableMap = std::unordered_map<uint64_t, std::vector<uint32_t>>;

  /// Lowers one FlowIR function to QBE IL text. `names` supplies the display
  /// names of callees so calls can build collision-free symbols.
  ///
  /// M1 subset: scalar literals (integer/boolean/float), prefix and binary
  /// arithmetic/comparisons, locals as stack slots, block parameters as phi
  /// instructions, branch/jump/loop-backedge/tail-recur terminators, and
  /// return. M2 adds direct calls (delivered) and aggregates (pending).
  [[nodiscard]] auto lower(const flowir::Function &function, const FunctionNames &names = {}) -> std::string;

  /// Lowers a module of FlowIR functions (native placeholders are skipped).
  /// `vtables` emits per-(trait, concrete) dispatch tables and the runtime
  /// key -> vtable lookup for `ref<Trait>` dynamic calls.
  [[nodiscard]] auto lowerModule(const std::vector<flowir::Function> &functions,
                                 const VtableMap &vtables = {}) -> std::string;
} // namespace NG::native
