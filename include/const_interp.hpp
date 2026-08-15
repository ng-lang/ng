// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "const_eval.hpp"
#include "typecheck.hpp"
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace NG::const_eval
{
  /// Typed-HIR interpreter for `const fun` (D-013). It evaluates resolved,
  /// type-validated function bodies over canonical `ConstValue`s: locals,
  /// `if`/`const if`, `return`, `loop`/`next`, tail recursion, and calls to
  /// other const functions. It never instantiates STUPID, runtime
  /// `StorageCell`, module instances, or process-global state. Fuel and
  /// recursion depth are budgeted; exceeding them is a deterministic
  /// compile-time error with a source span.
  class ConstInterpreter final
  {
  public:
    /// Handles a const predicate application (`name<types>`) when the
    /// interpreter meets one inside a const function body.
    using PredicateEvaluator = std::function<ConstValueId(const hir::Expression &)>;
    /// Const-time local bindings keyed by module-global `LocalId`.
    using LocalValues = std::unordered_map<uint32_t, ConstValueId>;

    ConstInterpreter(const hir::Module &module, const std::unordered_set<uint32_t> &constFunctions,
                     ConstInterner &interner, PredicateEvaluator predicate);

    /// Evaluates a const-capable call expression to a canonical value. Used by
    /// the checker's const-condition extension and where-clause evaluation;
    /// argument expressions must be compile-time constants over the supplied
    /// (possibly empty) locals and const-parameter bindings.
    [[nodiscard]] auto evaluateCall(const hir::Expression &call, const LocalValues &locals,
                                    const ConstBindings &constBindings, syntax::SourceSpan span) -> ConstValueId;

    /// Evaluates an expression over local const bindings (const fun bodies).
    [[nodiscard]] auto evaluateExpression(const hir::Expression &expression, const LocalValues &locals) -> ConstValueId;

    /// Evaluates a boolean condition (const if) over local const bindings.
    [[nodiscard]] auto evaluateCondition(const hir::Expression &expression, const LocalValues &locals) -> bool;

  private:
    struct Control
    {
      enum class Kind
      {
        Fallthrough,
        Return,
        NextLoop,
        TailRecur,
      };
      Kind kind{Kind::Fallthrough};
      ConstValueId value;
      uint32_t loopTarget{};
      std::vector<ConstValueId> arguments;
    };

    [[nodiscard]] auto runFunction(const hir::Function &function, const std::vector<ConstValueId> &arguments) -> ConstValueId;
    [[nodiscard]] auto runBlock(const hir::Block &block, LocalValues &locals) -> Control;
    [[nodiscard]] auto runLoop(const hir::Statement &statement, LocalValues &locals) -> Control;
    [[nodiscard]] auto asBool(ConstValueId id, syntax::SourceSpan span) const -> bool;
    [[nodiscard]] auto asInteger(ConstValueId id, syntax::SourceSpan span) const -> int64_t;
    void consumeStep(syntax::SourceSpan span) const;

    const hir::Module &module_;
    const std::unordered_set<uint32_t> &constFunctions_;
    ConstInterner &interner_;
    PredicateEvaluator predicate_;
    /// Const-parameter bindings active during the current where-clause
    /// evaluation; restored on nested calls.
    ConstBindings constBindings_;
    mutable uint32_t steps_{};
    size_t depth_{};
  };
} // namespace NG::const_eval
