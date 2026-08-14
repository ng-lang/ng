// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/typecheck.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
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
    ExtractTuple,
    /// Creates a scoped reference value over a local-rooted place.
    MakeRef,
    /// Reads the referent of a reference value.
    LoadRef,
    /// Writes through a place: a local-rooted path or a reference-rooted path.
    AssignPlace,
    /// Splices tuple values and plain elements into one tuple (spreads and
    /// variadic argument packing).
    TupleSplice,
    /// Slices an array by a runtime range value.
    Slice,
    /// Reads the variant ordinal of an enum value as i64.
    EnumVariantIndex,
    /// Reads the payload of an enum value (unit for payloadless variants).
    ExtractEnumPayload,
  };

  /// One step of a lowered place path. Member steps carry a product field
  /// ordinal; Index steps carry the value id of the already-lowered index.
  struct PlaceStep
  {
    enum class Kind : uint8_t
    {
      Member,
      Index,
    };
    Kind kind{};
    int64_t field{};
    ValueId indexValue{};
  };

  struct Instruction
  {
    InstructionKind kind;
    ValueId result;
    std::optional<hir::LocalId> local;
    std::optional<ValueId> source;
    hir::ExpressionKind expressionKind;
    std::string text;
    int64_t payload{};
    std::optional<hir::DefId> callTarget;
    std::vector<ValueId> operands;
    /// Place metadata for MakeRef / AssignPlace: the root is either a local in
    /// the current frame or a reference value, followed by zero or more steps.
    std::optional<hir::LocalId> placeRootLocal;
    std::optional<ValueId> placeRootRef;
    bool placeMutable{};
    std::vector<PlaceStep> placeSteps;
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
    std::vector<hir::LocalId> parameterLocals;
    size_t parameterCount{};
    std::vector<Instruction> instructions;
    std::optional<Terminator> terminator;
  };

  struct Function
  {
    hir::DefId source;
    std::string name;
    bool nativeFunction{};
    BlockId entry;
    std::vector<hir::LocalId> parameterLocals;
    std::vector<Block> blocks;
    std::unordered_map<uint32_t, typecheck::TypeId> valueTypes;
    std::unordered_map<uint32_t, typecheck::TypeId> localTypes;
    std::vector<typecheck::TypeDescriptor> typeDescriptors;
  };

  /// Lowers resolved/type-validated control structure to a CFG. This is a new
  /// backend-neutral IR; it does not emit or depend on legacy ORGASM bytecode.
  class Lowerer final
  {
  public:
    [[nodiscard]] auto lower(const hir::Function &function) -> Function;
    [[nodiscard]] auto lower(const hir::Function &function, const typecheck::TypeCheckResult &types) -> Function;
  };

  struct VerificationError : std::runtime_error
  {
    explicit VerificationError(std::string message) : std::runtime_error(std::move(message)) {}
  };

  class Verifier final
  {
  public:
    void verify(const Function &function) const;
  };
} // namespace NG::vnext::flowir
