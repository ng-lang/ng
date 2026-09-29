// AI-generated code; reviewed for this repository's vNext rewrite.
// Internal header for FlowIR lowering. FunctionLowerer lowers one type-checked
// HIR function to a CFG; its definitions are split by concern:
//   flowir.cpp         entry point, emission primitives, blocks, statements,
//                      structured control flow, and the verifier
//   flowir_expr.cpp    expressions, places, calls, derive(Eq/Show)
//   flowir_loops.cpp   runtime loops: map/filter comprehensions, folds, and
//                      list literals
//   flowir_switch.cpp  enum, scalar-literal, and union switches
// Not part of the public API.
#pragma once

#include "flowir.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NG::flowir::detail
{
  using typecheck::TypeId;

  /// Encodes an integer literal's text as the int64 bit payload carried by
  /// `Evaluate` instructions. Unsigned literals above i64::max keep their
  /// two's-complement bits; the checker has already validated the text.
  [[nodiscard]] auto integerLiteralPayload(std::string_view text, TypeId type) -> int64_t;

  /// Operator codes carried in the `payload` of Binary / Prefix `Evaluate`
  /// instructions and decoded by the native backend; 0 for other operators.
  [[nodiscard]] auto binaryOperatorCode(std::string_view op) -> int64_t;
  [[nodiscard]] auto prefixOperatorCode(std::string_view op) -> int64_t;

  /// Ordinal of `name` among a descriptor's field (or variant) names.
  [[nodiscard]] auto fieldOrdinal(const typecheck::TypeDescriptor &descriptor, std::string_view name)
      -> std::optional<size_t>;

  /// EnumLiteral payload: enum type id in the low word, variant ordinal in
  /// the high word.
  [[nodiscard]] inline auto enumLiteralPayload(TypeId enumType, uint32_t variant) -> int64_t
  {
    return static_cast<int64_t>(enumType.value) | (static_cast<int64_t>(variant) << 32);
  }

  /// `...operand` spread marker inside tuple/array literals.
  [[nodiscard]] inline auto isSpread(const hir::Expression &expression) -> bool
  {
    return expression.kind == hir::ExpressionKind::Prefix && expression.text == "...";
  }

  class FunctionLowerer final
  {
  public:
    explicit FunctionLowerer(const typecheck::TypeCheckResult &types) : types_(types) {}

    [[nodiscard]] auto lower(const hir::Function &source) -> Function;

  private:
    struct LoweredPlace
    {
      std::optional<hir::LocalId> rootLocal;
      std::optional<ValueId> rootRef;
      std::vector<PlaceStep> steps;
    };

    /// A counted loop carrying `(index, carried)` as header block parameters.
    struct IndexLoop
    {
      BlockId header;
      BlockId exit;
      hir::LocalId carriedLocal;
      ValueId index;
    };

    // ---- Emission primitives (flowir.cpp) ----------------------------------
    [[nodiscard]] auto appendBlock() -> BlockId;
    [[nodiscard]] auto block() -> Block & { return function_.blocks[current_.value]; }
    [[nodiscard]] auto terminated() -> bool { return block().terminator.has_value(); }
    /// Appends `instruction` to the current block under a fresh result id and
    /// records the result type when one is known.
    auto emit(Instruction instruction, std::optional<TypeId> type) -> ValueId;
    auto integerConstant(int64_t value) -> ValueId;
    /// A scalar literal of `kind` (integer, float, bool, or string) typed `type`.
    auto literal(std::string text, hir::ExpressionKind kind, TypeId type) -> ValueId;
    auto readLocal(hir::LocalId local, TypeId type) -> ValueId;
    auto binary(std::string_view op, ValueId left, ValueId right, TypeId type) -> ValueId;
    auto bindLocal(hir::LocalId local, ValueId source, TypeId type) -> ValueId;
    auto extractMember(ValueId source, size_t field, TypeId type) -> ValueId;
    auto extractTuple(ValueId source, size_t element, TypeId type) -> ValueId;
    void jump(BlockId target, std::vector<ValueId> arguments = {});
    /// Falls through to `target` unless the current block already terminated.
    void jumpIfOpen(BlockId target);
    void branch(ValueId condition, BlockId whenTrue, BlockId whenFalse);
    void backedge(BlockId header, std::vector<ValueId> arguments);
    [[nodiscard]] auto typeOf(const hir::Expression &expression) const -> TypeId;
    [[nodiscard]] auto descriptorOf(TypeId type) const -> const typecheck::TypeDescriptor &;
    [[nodiscard]] auto kindOf(const hir::Expression &expression) const -> typecheck::TypeKind;
    /// The already-lowered index values along a place path, in step order.
    [[nodiscard]] static auto indexOperands(const std::vector<PlaceStep> &steps) -> std::vector<ValueId>;

    // ---- Blocks, statements, structured control flow (flowir.cpp) ----------
    void reserveSyntheticLocalIds(const hir::Function &source);
    void lowerBlock(const hir::Block &source);
    void emitBlockDrops(const hir::Block &source);
    void lowerDropCalls(const std::vector<std::pair<uint32_t, uint32_t>> &drops);
    void lowerStatement(const hir::Statement &statement);
    void lowerLet(const hir::Statement &statement);
    void lowerTupleDestructuring(const hir::Statement &statement, ValueId initializer);
    void lowerAssignment(const hir::Statement &statement);
    void lowerReturn(const hir::Statement &statement);
    void lowerIf(const hir::Statement &statement);
    void lowerConstIf(const hir::Statement &statement);
    void lowerLoop(const hir::Statement &statement);
    void lowerWhile(const hir::Statement &statement);
    /// Lowers a loop body in `body` with `continue`/`break` bound to
    /// `header`/`exit`; an open body end falls through to `fallthrough`.
    void lowerLoopBody(const hir::Statement &statement, BlockId header, BlockId body, BlockId exit,
                       BlockId fallthrough);
    /// `continue` / `break`: drop the current block's scoped values and jump
    /// to the loop's recorded header or exit.
    void lowerLoopExit(const hir::Statement &statement, const std::unordered_map<uint32_t, BlockId> &targets);
    void lowerNext(const hir::Statement &statement);

    // ---- Expressions, places, calls, derives (flowir_expr.cpp) -------------
    [[nodiscard]] auto lowerExpression(const hir::Expression &expression) -> ValueId;
    /// The generic `Evaluate` form: operands, operator/literal payload, and
    /// call target for everything without a dedicated lowering.
    [[nodiscard]] auto lowerEvaluate(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto isDirectCall(const hir::Expression &expression) const -> bool;
    [[nodiscard]] auto lowerOperands(const hir::Expression &expression, bool directCall) -> std::vector<ValueId>;
    [[nodiscard]] auto evaluatePayload(const hir::Expression &expression, std::vector<ValueId> &operands) -> int64_t;
    [[nodiscard]] auto lowerReadThroughReference(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerTraitViewCoercion(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerSignedIntegerLiteral(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerLogicalExpression(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerStructEq(const hir::Expression &expression, TypeId structType, bool negate) -> ValueId;
    [[nodiscard]] auto lowerStructEqDirect(ValueId left, ValueId right, TypeId structType, bool negate) -> ValueId;
    [[nodiscard]] auto lowerReferenceExpression(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerSpanExpression(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerTupleSplice(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerMethodCall(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerMethodReceiver(const hir::Expression &call, const hir::Expression &receiver) -> ValueId;
    [[nodiscard]] auto buildShowString(ValueId structValue, TypeId structType) -> ValueId;
    [[nodiscard]] auto lowerPlace(const hir::Expression &expression) -> LoweredPlace;

    // ---- Runtime loops (flowir_loops.cpp) ----------------------------------
    /// Jumps from the current block into a loop header carrying
    /// `(initialIndex, initialCarried)`, tests `index <test> bound` there, and
    /// leaves the builder in the loop body.
    [[nodiscard]] auto openIndexLoop(ValueId initialIndex, ValueId initialCarried, TypeId carriedType,
                                     std::string_view test, ValueId bound) -> IndexLoop;
    /// Reads element `index` of a spread source (range, recursive list, or
    /// array).
    [[nodiscard]] auto loadSpreadElement(const hir::Expression &sourceExpression, ValueId source, ValueId index)
        -> ValueId;
    [[nodiscard]] auto lowerListLiteral(const hir::Expression &expression, TypeId listType) -> ValueId;
    [[nodiscard]] auto lowerMapLiteral(const hir::Expression &expression) -> ValueId;
    [[nodiscard]] auto lowerFoldCall(const hir::Expression &expression) -> ValueId;

    // ---- Switches (flowir_switch.cpp) --------------------------------------
    void lowerSwitch(const hir::Statement &statement);
    void lowerEnumSwitch(const hir::Statement &statement, ValueId scrutinee);
    void lowerLiteralSwitch(const hir::Statement &statement, ValueId scrutinee);
    void lowerUnionSwitch(const hir::Statement &statement, ValueId scrutinee);
    /// Shared switch CFG: one block per case, a tail block for `otherwise`,
    /// and an exit join. `emitTest(case, caseBlock, next)` runs in the case's
    /// check block and must branch to `caseBlock` or `next`; `bindCase(case)`
    /// runs at the top of each case block before its body.
    void lowerSwitchCases(const hir::Statement &statement,
                          const std::function<void(size_t, BlockId, BlockId)> &emitTest,
                          const std::function<void(size_t)> &bindCase);
    /// Branches to `caseBlock` when any of `count` alternatives matches,
    /// testing each alternative after the first in a fresh check block.
    void branchOnAny(size_t count, const std::function<ValueId(size_t)> &test, BlockId caseBlock, BlockId next);

    Function function_;
    BlockId current_{};
    uint32_t nextValue_{};
    uint32_t nextSyntheticLocal_{};
    const typecheck::TypeCheckResult &types_;
    const hir::Block *currentHirBlock_{};
    const hir::Block *functionBody_{};
    std::optional<ValueId> fallthroughReturn_;
    std::unordered_map<uint32_t, BlockId> loopHeaders_;
    std::unordered_map<uint32_t, BlockId> loopExits_;
  };
} // namespace NG::flowir::detail
