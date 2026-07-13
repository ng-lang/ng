// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/hir.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace NG::vnext::flowir
{
  struct BlockId
  {
    uint32_t value{};
    auto operator==(const BlockId &) const -> bool = default;
  };

  struct ValueId
  {
    uint32_t value{};
    auto operator==(const ValueId &) const -> bool = default;
  };

  enum class InstructionKind
  {
    Evaluate,
    BindLocal,
  };

  struct Instruction
  {
    InstructionKind kind;
    ValueId result;
    std::optional<hir::LocalId> local;
    hir::ExpressionKind expressionKind;
  };

  enum class TerminatorKind
  {
    Return,
    Jump,
    Branch,
    LoopBackedge,
    TailRecur,
  };

  struct Terminator
  {
    TerminatorKind kind;
    std::vector<BlockId> targets;
    std::vector<ValueId> arguments;
  };

  struct Block
  {
    BlockId id;
    std::vector<Instruction> instructions;
    std::optional<Terminator> terminator;
  };

  struct Function
  {
    hir::DefId source;
    BlockId entry;
    std::vector<Block> blocks;
  };

  /// Lowers resolved/type-validated control structure to a CFG. This is a new
  /// backend-neutral IR; it does not emit or depend on legacy ORGASM bytecode.
  class Lowerer final
  {
  public:
    [[nodiscard]] auto lower(const hir::Function &function) -> Function;
  };
} // namespace NG::vnext::flowir
