// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/bytecode.hpp"
#include "vnext/value.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace NG::vnext::vm::detail
{
  /// Frame local bindings are canonical shared cells: references capture the
  /// cell, so writes through a reference stay visible to every reader of the
  /// binding even after the binding is rebound.
  using LocalCells = std::unordered_map<uint32_t, std::shared_ptr<Value>>;

  /// Evaluates one decoded value-producing instruction against a frame's
  /// value/register and local-binding stores. Reads deep-copy (D-015) so value
  /// slots never alias binding storage. Control-transfer instructions
  /// intentionally remain owned by the single- and module-VM dispatch loops.
  void evaluateInstruction(const bytecode::DecodedInstruction &instruction, const std::vector<std::string> &stringConstants,
                           std::vector<Value> &values, const LocalCells &locals);
  void makeRefInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values,
                          const LocalCells &locals);
  void loadRefInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values);
  void assignPlaceInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values,
                              const LocalCells &locals);
  void loadVariantInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values);
  void extractPayloadInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values);
} // namespace NG::vnext::vm::detail
