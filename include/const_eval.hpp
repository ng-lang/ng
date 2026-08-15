// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "hir.hpp"
#include "syntax/const_expr.hpp"
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NG::const_eval
{
  struct ConstValueId
  {
    uint32_t value{};
    auto operator==(const ConstValueId &) const -> bool = default;
  };

  enum class ConstValueKind
  {
    Invalid,
    Unit,
    Bool,
    Integer,
    String,
    Tuple,
  };

  /// A canonical compile-time value. Identity is assigned by the
  /// `ConstInterner`, never by source spelling: `1 + 2` and `3` intern to the
  /// same `ConstValueId`.
  struct ConstValue
  {
    ConstValueKind kind{ConstValueKind::Invalid};
    bool boolValue{};
    int64_t integerValue{};
    std::string stringValue;
    std::vector<ConstValueId> tupleElements;
    auto operator==(const ConstValue &) const -> bool = default;
  };

  /// Interns const values and returns canonical IDs. Equivalent integers and
  /// structurally equal tuples share an ID so const-generic instance keys and
  /// fixed-array types can rely on identity rather than spelling.
  class ConstInterner final
  {
  public:
    ConstInterner();

    [[nodiscard]] auto internUnit() -> ConstValueId;
    [[nodiscard]] auto internBool(bool value) -> ConstValueId;
    [[nodiscard]] auto internInteger(int64_t value) -> ConstValueId;
    [[nodiscard]] auto internString(std::string value) -> ConstValueId;
    [[nodiscard]] auto internTuple(const std::vector<ConstValueId> &elements) -> ConstValueId;

    [[nodiscard]] auto value(ConstValueId id) const -> const ConstValue &;
    [[nodiscard]] auto values() const -> const std::vector<ConstValue> & { return values_; }

  private:
    [[nodiscard]] auto append(ConstValue value) -> ConstValueId;

    std::vector<ConstValue> values_;
    std::unordered_map<int64_t, ConstValueId> integers_;
    std::unordered_map<std::string, ConstValueId> strings_;
  };

  struct ConstEvalError : std::runtime_error
  {
    syntax::SourceSpan span;

    ConstEvalError(std::string message, syntax::SourceSpan sourceSpan)
      : std::runtime_error(std::move(message)), span(sourceSpan)
    {
    }
  };

  /// Bindings from const-parameter names to canonical const values. The initial
  /// type-level domain evaluates integer expressions; identifier resolution is
  /// deferred to this map so the parser never decides const identity.
  using ConstBindings = std::unordered_map<std::string, ConstValueId>;

  /// Optional semantic extension for node kinds the evaluator does not own
  /// (const predicate applications, later const-function calls). Layers above
  /// const evaluation register these; the evaluator itself stays pure.
  using ConstNodeExtension = std::function<ConstValueId(const hir::Expression &)>;

  /// Typed, fuel-limited evaluator over structured const expressions. It never
  /// touches runtime values, module state, native bridges, or the legacy
  /// interpreter. Arithmetic is checked: overflow, division by zero, and
  /// negative array lengths are compile-time errors with source spans.
  class ConstEvaluator final
  {
  public:
    explicit ConstEvaluator(ConstInterner &interner) : interner_(interner) {}

    [[nodiscard]] auto evaluate(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> ConstValueId;
    [[nodiscard]] auto evaluateInteger(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> int64_t;
    [[nodiscard]] auto evaluateArrayLength(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> uint64_t;

    /// Evaluates a resolved, typed `const if` condition to a bool. The subset
    /// covers boolean/integer/string literals, checked integer arithmetic,
    /// comparisons, `!`, and short-circuiting `&&`/`||`. Runtime locals,
    /// calls, and anything else are rejected with source spans unless the
    /// extension handles them.
    [[nodiscard]] auto evaluateBool(const hir::Expression &expression, const ConstNodeExtension &extension = {}) const -> bool;

  private:
    [[nodiscard]] auto evaluateNode(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> ConstValueId;
    [[nodiscard]] auto evaluateHirNode(const hir::Expression &expression, const ConstNodeExtension &extension) const -> ConstValueId;
    [[nodiscard]] auto asInteger(ConstValueId id, syntax::SourceSpan span) const -> int64_t;
    [[nodiscard]] auto asBool(ConstValueId id, syntax::SourceSpan span) const -> bool;
    void consumeFuel(syntax::SourceSpan span) const;

    ConstInterner &interner_;
    mutable uint32_t fuel_{};
  };
} // namespace NG::const_eval
