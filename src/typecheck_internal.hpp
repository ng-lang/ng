// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: Internal header for the Checker class, shared by all typecheck TUs.
#pragma once

#include "const_interp.hpp"
#include "typecheck.hpp"

#include <algorithm>
#include <charconv>
#include <deque>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace NG::typecheck::detail
{
  [[nodiscard]] inline auto isArrayKind(TypeKind kind) -> bool
  {
    return kind == TypeKind::DynamicArray || kind == TypeKind::FixedArray || kind == TypeKind::DependentArray;
  }

  /// `...operand` spread marker in literals and call arguments.
  [[nodiscard]] inline auto isSpread(const hir::Expression &expression) -> bool
  {
    return expression.kind == hir::ExpressionKind::Prefix && expression.text == "...";
  }

  class Checker final
  {
  public:
    [[nodiscard]] auto check(const hir::Module &module, const const_eval::ConstNativeHost &host = {})
        -> TypeCheckResult;

  private:
    using LocalTypes = std::unordered_map<uint32_t, TypeId>;
    using LoopTypes = std::unordered_map<uint32_t, std::vector<TypeId>>;
    using MutableBindings = std::unordered_set<uint32_t>;

    struct Substitution
    {
      std::unordered_map<uint32_t, TypeId> types;
      std::unordered_map<uint32_t, std::vector<TypeId>> packs;
      TypeInterner::ConstSubstitution consts;
      TypeInterner::ConstructorSubstitution constructors;
    };

    struct ConstMatch
    {
      bool matched{};
      int priority{};
      std::unordered_map<uint32_t, TypeId> bindings;
    };

    struct TraitInfo
    {
      std::string name;
      std::vector<std::string> supertraits;
      bool autoTrait{};
      TypeId selfParameter;
      std::vector<std::string> methodOrder;
      std::unordered_map<std::string, std::vector<TypeId>> methodParameters;
      std::unordered_map<std::string, TypeId> methodReturns;
      std::unordered_map<std::string, hir::DefId> methodDefaults;
    };

    struct ImplInfo
    {
      std::string traitName;
      TypeId target;
      bool generic{};
      std::vector<TypeId> typeParameters;
      TypeId targetPatternId;
      std::string targetPatternDisplay;
      std::unordered_map<std::string, hir::DefId> methods;
    };

    struct BorrowState
    {
      struct Counts
      {
        size_t shared{};
        size_t mutableRefs{};
      };
      struct Loan
      {
        const hir::Expression *site{};
        uint32_t root{};
        // INC-17 (M2b): a scoped reference can have several live bearers. A
        // `ref x` binding is the primary bearer; a closure capturing that
        // binding becomes an anonymous borrower and is appended here, so the
        // loan only releases once every bearer is dead. Bearer 0 marks an
        // untied expression-level loan (released at statement end).
        std::vector<uint32_t> bearers{};
        bool mutableLoan{};
      };
      std::vector<Loan> loans;
      std::unordered_map<uint32_t, Counts> counts;
      static constexpr size_t MaxShared{16};

      void add(const hir::Expression *site, uint32_t root, uint32_t bearer, bool mutableLoan)
      {
        loans.push_back(Loan{site, root, {bearer}, mutableLoan});
        rebuildCounts();
      }

      void remove(const hir::Expression *site)
      {
        for (auto it = loans.begin(); it != loans.end(); ++it)
        {
          if (it->site != site)
            continue;
          loans.erase(it);
          rebuildCounts();
          return;
        }
      }

      void rebuildCounts()
      {
        counts.clear();
        for (const auto &loan : loans)
        {
          auto &count = counts[loan.root];
          if (loan.mutableLoan)
            ++count.mutableRefs;
          else
            ++count.shared;
        }
      }

      [[nodiscard]] auto mergedWith(const BorrowState &other) const -> BorrowState
      {
        BorrowState merged = *this;
        for (const auto &loan : other.loans)
        {
          const bool present = std::any_of(merged.loans.begin(), merged.loans.end(),
                                           [&](const Loan &mine) { return mine.site == loan.site; });
          if (!present)
            merged.loans.push_back(loan);
        }
        merged.rebuildCounts();
        return merged;
      }
    };

    struct MoveState
    {
      std::unordered_map<uint32_t, bool> whole;
      std::unordered_map<uint32_t, std::unordered_set<uint32_t>> fields;
      /// The consuming name/member nodes. Call arguments are inferred more
      /// than once (overload scoring, spread analysis, expected-type
      /// checking); re-checking the node that performed a move is the same
      /// use, not a use after move.
      std::unordered_set<const hir::Expression *> sites;

      [[nodiscard]] auto movedBy(const hir::Expression &expression) const -> bool
      {
        return sites.contains(&expression);
      }
      [[nodiscard]] auto isWholeMoved(uint32_t local) const -> bool
      {
        return whole.contains(local) && (!fields.contains(local) || fields.at(local).empty());
      }
      [[nodiscard]] auto isFieldMoved(uint32_t local, uint32_t field) const -> bool
      {
        if (whole.contains(local))
          return !fields.contains(local) || !fields.at(local).contains(field);
        return fields.contains(local) && fields.at(local).contains(field);
      }
      [[nodiscard]] auto hasMovedField(uint32_t local) const -> bool
      {
        return fields.contains(local) && !fields.at(local).empty();
      }
      void markWhole(uint32_t local)
      {
        whole[local] = true;
        fields.erase(local);
      }
      void markField(uint32_t local, uint32_t field)
      {
        if (!whole.contains(local))
          fields[local].insert(field);
      }
      void reinitialize(uint32_t local)
      {
        whole.erase(local);
        fields.erase(local);
      }
      void reinitializeField(uint32_t local, uint32_t field)
      {
        if (whole.contains(local))
          fields[local].insert(field);
        else
          fields[local].erase(field);
      }
      [[nodiscard]] auto mergedWith(const MoveState &other) const -> MoveState
      {
        MoveState merged = *this;
        for (const auto &[local, _] : other.whole)
          merged.markWhole(local);
        for (const auto &[local, movedFields] : other.fields)
          for (const auto field : movedFields)
            merged.markField(local, field);
        merged.sites.insert(other.sites.begin(), other.sites.end());
        return merged;
      }
    };

    struct ConstDeclChecked
    {
      const hir::ConstDeclaration *declaration;
      std::vector<TypeId> parameters;
      std::vector<TypeId> pattern;
      TypeId targetType;
    };

    struct CallSpreadInfo
    {
      size_t expandedCount{};
      std::vector<size_t> spreadPositions;
      std::vector<TypeId> expandedTypes;
      std::vector<bool> spreadSlot;
      std::vector<const hir::Expression *> argumentExpressions;
    };

    struct FoldInfo
    {
      bool fold{};
      size_t spreadPosition{};
      size_t accumulatorPosition{};
      TypeId elementType{};
    };

    struct PatternMatchState
    {
      bool containsParameter{};
      bool repeated{};
      std::unordered_map<uint32_t, TypeId> bindings;
    };

    // ---- typecheck_register.cpp ----
    void registerTraits();
    void collectSupertraits(const std::string &traitName, std::vector<const TraitInfo *> &out,
                            std::unordered_set<std::string> &seen) const;
    void resolveImplMethodSignature(hir::DefId id, const hir::Function &function, TypeId target);
    void collectDropMovedFields(const hir::Block &block, uint32_t selfLocal, std::unordered_set<std::string> &fields);
    void collectDropMovedFields(const hir::Statement &statement, uint32_t selfLocal,
                                std::unordered_set<std::string> &fields);
    void collectDropMovedFields(const hir::Expression &expression, uint32_t selfLocal,
                                std::unordered_set<std::string> &fields);
    [[nodiscard]] auto dropEdge(uint32_t local, TypeId type, syntax::SourceSpan span) -> std::pair<uint32_t, uint32_t>;
    void registerImpls();
    [[nodiscard]] auto abiSafetyIssue(TypeId type) const -> std::string;
    void validateExternSignature(const hir::Function &function, const FunctionTypeIds &signature);
    void validateReprCStruct(const hir::Struct &structure);

    /// One array-literal spread (`xs...` value spread or `f(xs)...` map spread).
    struct ArraySpread
    {
      TypeId element;
      bool map{};
    };

    // ---- typecheck_infer.cpp: synthesis-mode inference ----
    [[nodiscard]] auto infer(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferName(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferArrayLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferArraySpread(const hir::Expression &spread, const LocalTypes &locals) -> ArraySpread;
    [[nodiscard]] auto inferMapSpread(const hir::Expression &spread, const hir::Expression &call, bool filter,
                                      const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferStructLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferEnumLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferTupleLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferGenericApplication(const hir::Expression &expression) -> TypeId;
    [[nodiscard]] auto inferPrefix(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferReference(const hir::Expression &expression, TypeId operand, const LocalTypes &locals)
        -> TypeId;
    [[nodiscard]] auto inferSpan(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferBinary(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferIndex(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferMember(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferClosureLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    /// Registers a `ref` / `ref mut` / `span` loan of `root` at `site` (once
    /// per site), rejecting conflicting live loans.
    void recordBorrow(const hir::Expression &site, uint32_t root, bool mutableBorrow, std::string_view name);
    /// The element type of a recursive `List<T>`-shaped enum (a two-field
    /// Cons payload whose tail recurses by `ref` or `Box`), if any.
    [[nodiscard]] auto listElementType(TypeId list) const -> std::optional<TypeId>;
    /// Scoped views (`ref`, `span`) cannot be stored in aggregates.
    void rejectScopedView(TypeId type, syntax::SourceSpan span, std::string_view container) const;

    // ---- typecheck_expected.cpp: checking-mode inference ----
    [[nodiscard]] auto inferExpected(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                     std::string_view context) -> TypeId;
    [[nodiscard]] auto inferExpectedEnum(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
        -> TypeId;
    /// A constructor of a generic enum (`Option.Some(4)`): it cannot be
    /// synthesized standalone and needs an expected type or unification.
    [[nodiscard]] auto isGenericEnumLiteral(const hir::Expression &expression) const -> bool;
    [[nodiscard]] auto inferExpectedStruct(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
        -> TypeId;
    /// Unsuffixed / matching-suffix numeric literals (and `-lit` / `+lit`)
    /// adopting an expected numeric type.
    [[nodiscard]] auto adoptNumericLiteral(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
        -> std::optional<TypeId>;
    [[nodiscard]] auto inferExpectedList(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
        -> std::optional<TypeId>;
    [[nodiscard]] auto inferExpectedUnion(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
        -> TypeId;
    [[nodiscard]] auto inferExpectedTraitView(const hir::Expression &expression, TypeId expected,
                                              const LocalTypes &locals, std::string_view context) -> TypeId;
    [[nodiscard]] auto inferExpectedTuple(const hir::Expression &expression, TypeId expected, const LocalTypes &locals)
        -> TypeId;
    [[nodiscard]] auto inferExpectedClosure(const hir::Expression &expression, TypeId expected,
                                            const LocalTypes &locals, std::string_view context) -> TypeId;
    /// Calls to named functions under an expected type: folds, and generic
    /// calls whose return type seeds the substitution. Non-generic calls fall
    /// back to synthesis (nullopt).
    [[nodiscard]] auto inferExpectedCall(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                         std::string_view context) -> std::optional<TypeId>;
    [[nodiscard]] auto inferExpectedGenericCall(const hir::Expression &expression, hir::DefId selected, TypeId expected,
                                                const LocalTypes &locals, std::string_view context) -> TypeId;

    // ---- typecheck_call.cpp: calls, method calls, closures ----
    /// Synthesis-mode call to a named function or closure local. Argument
    /// types are precomputed once (analyzeCallSpread) and closure literals are
    /// deferred until the rest of the substitution is bound.
    [[nodiscard]] auto inferCall(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferClosureCall(const hir::Expression &expression, TypeId calleeType, const LocalTypes &locals)
        -> TypeId;
    [[nodiscard]] auto inferMethodCall(const hir::Expression &expression, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferBoundMethodCall(const hir::Expression &expression, TypeId dispatchType) -> TypeId;
    [[nodiscard]] auto inferTraitViewMethodCall(const hir::Expression &expression, TypeId dispatchType,
                                                size_t firstArgument, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto inferDerivedMethodCall(const hir::Expression &expression, TypeId dispatchType,
                                              size_t firstArgument) -> std::optional<TypeId>;
    [[nodiscard]] auto inferImplMethodCall(const hir::Expression &expression, const hir::Expression &receiver,
                                           TypeId receiverType, TypeId dispatchType, bool passReferenceThrough,
                                           size_t firstArgument, const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto findImplMethod(std::string_view name, TypeId dispatchType, Substitution &matched) const
        -> std::optional<hir::DefId>;
    /// Picks the most specific overload among the callee's candidates;
    /// `requireMatch` rejects a call no candidate accepts.
    [[nodiscard]] auto selectOverload(const hir::Expression &call, const LocalTypes &locals, bool requireMatch)
        -> hir::DefId;
    [[nodiscard]] auto inferFoldCall(const hir::Expression &call, const FoldInfo &fold, hir::DefId target,
                                     const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto analyzeCallSpread(const hir::Expression &expression, size_t firstArgument,
                                         const LocalTypes &locals, CallSpreadInfo &info) -> bool;
    [[nodiscard]] auto analyzeFoldCall(const hir::Expression &expression, const LocalTypes &locals) -> FoldInfo;
    [[nodiscard]] auto explicitSubstitution(const hir::Expression &expression, const FunctionTypeIds &signature)
        -> Substitution;
    [[nodiscard]] static auto fixedParameterCount(const FunctionTypeIds &signature) -> size_t;
    /// Declared type of argument slot `index` (variadic slots map to the pack
    /// element type).
    [[nodiscard]] auto callParameterType(const FunctionTypeIds &signature, size_t index) const -> TypeId;
    /// A bound closure local forwarded as argument `index` records its code
    /// function as a closure target and threads its captures. Returns true
    /// when `argument` is such a closure.
    auto bindForwardedClosure(const hir::Expression &call, const hir::Expression &argument, TypeId argumentType,
                              size_t index, std::vector<std::pair<size_t, hir::DefId>> &closureTargets) -> bool;
    /// INC-14 S3: capture values thread into the specialized callee as leading
    /// arguments, recorded once before the call target commits.
    void threadCallCaptures(const hir::Expression &call, const std::vector<uint32_t> &captures);
    [[nodiscard]] auto closureCaptureLocals(uint32_t closureLocal) const -> std::vector<uint32_t>;
    void bindPackArgument(Substitution &substitution, TypeId packParameter, size_t position, TypeId argumentType,
                          syntax::SourceSpan span) const;
    /// Checking-mode generic argument: enum constructors unify through their
    /// payload (a generic enum literal has no standalone type).
    void unifyGenericArgument(const hir::Expression &argument, TypeId parameterType, Substitution &substitution,
                              const LocalTypes &locals);
    void requireWhereClause(hir::DefId target, const Substitution &substitution, const FunctionTypeIds &signature,
                            syntax::SourceSpan span);
    [[nodiscard]] auto isConcrete(const Substitution &substitution) const -> bool;
    /// By-value arguments consume their places (all slots, or only the spread
    /// slots when `spread` is given); extern-C targets borrow.
    void trackCallConsumption(const hir::Expression &call, hir::DefId target, const FunctionTypeIds &signature,
                              size_t supplied, const CallSpreadInfo *spread, const LocalTypes &locals);
    void recordVariadicPack(const hir::Expression &call, size_t packCount, size_t fixedParameters, size_t supplied,
                            const std::function<TypeId(size_t)> &argumentType);
    /// Types a closure-literal call argument against a (possibly still generic)
    /// closure parameter, records its code function in `closureTargets`, and
    /// unifies the specialized parameter with the inferred literal type.
    auto inferCallClosureArgument(const hir::Expression &closure, TypeId parameterType, size_t index,
                                  const LocalTypes &locals, Substitution &substitution,
                                  std::vector<std::pair<size_t, hir::DefId>> &closureTargets) -> TypeId;
    /// INC-14 S2: checks a closure body against resolved parameter types and
    /// synthesizes its code function. Returns the interned closure type.
    [[nodiscard]] auto inferClosure(const hir::Expression &closure, const std::vector<TypeId> &parameterTypes,
                                    const LocalTypes &locals) -> TypeId;
    [[nodiscard]] auto specificity(TypeId type) const -> int;
    [[nodiscard]] auto specificityGuarded(TypeId type, std::unordered_set<uint32_t> &visited) const -> int;

    // ---- typecheck_check.cpp: functions, blocks, statements ----
    void checkFunction(const hir::Function &function, const const_eval::ConstBindings &constBindings = {});
    void recordFallthroughDrops(const hir::Function &function, const LocalTypes &locals);
    void checkBlock(const hir::Block &block, LocalTypes locals, LoopTypes loops, TypeId returnType,
                    bool nestedScope = false);
    void checkStatement(const hir::Statement &statement, LocalTypes &locals, LoopTypes &loops, TypeId returnType);
    void discardStatementValue(const hir::Statement &statement, const LocalTypes &locals);
    void checkLet(const hir::Statement &statement, LocalTypes &locals);
    void bindLetLocals(const hir::Statement &statement, TypeId type, LocalTypes &locals);
    /// INC-14 S3/S4 + INC-17: a `let` bound to a closure literal records its
    /// code function and captures; a closure capturing a live scoped reference
    /// becomes an extra bearer of that loan.
    void bindClosureLocal(const hir::Statement &statement);
    void checkAssign(const hir::Statement &statement, const LocalTypes &locals);
    /// Scoped views (`ref`, `span` bindings) cannot be reassigned.
    void rejectViewReassignment(TypeId type, syntax::SourceSpan span) const;
    void checkReturn(const hir::Statement &statement, const LocalTypes &locals, TypeId returnType);
    void checkIf(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops, TypeId returnType);
    void checkConstIf(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops,
                      TypeId returnType);
    /// Checks a loop body; moves/loans inside may or may not have happened
    /// after the loop, so they merge with the state before it.
    void checkLoopBody(const hir::Block &body, LocalTypes locals, LoopTypes loops, TypeId returnType);
    void checkNext(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops);

    // ---- typecheck_switch.cpp: switches ----
    /// Joins the move/borrow states of switch branches: each branch starts
    /// from the pre-switch state; afterwards any move/loan of any branch holds.
    struct BranchJoin
    {
      Checker &checker;
      const LoopTypes &loops;
      TypeId returnType;
      MoveState before;
      BorrowState borrowsBefore;
      MoveState merged{};
      BorrowState borrowsMerged{};
      bool anyBranch{};

      void check(const hir::Block &branch, const LocalTypes &locals);
      void finish();
    };
    [[nodiscard]] auto beginBranches(const LoopTypes &loops, TypeId returnType, MoveState before) -> BranchJoin;
    void checkSwitch(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops,
                     TypeId returnType);
    void checkEnumSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                         const LoopTypes &loops, TypeId returnType);
    void checkLiteralSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                            const LoopTypes &loops, TypeId returnType);
    void checkSwitchLiteral(std::string_view text, TypeId scrutinee, syntax::SourceSpan span) const;
    void checkUnionSwitch(const hir::Statement &statement, TypeId scrutinee, const LocalTypes &locals,
                          const LoopTypes &loops, TypeId returnType);

    // ---- typecheck_ownership.cpp: places, moves, borrows, captures ----
    [[nodiscard]] auto lastUseByBlock(const hir::Block &block) -> const std::unordered_map<uint32_t, size_t> &;
    void releaseLoansUsedUpTo(size_t statementIndex, const std::unordered_map<uint32_t, size_t> &lastUse,
                              size_t loansBeforeStatement);
    /// At the end of a nested scope, drops the scope's bearers from every
    /// loan and releases loans with no surviving bearer (INC-17 M2b).
    void releaseScopeBearers(const std::unordered_set<uint32_t> &outerLocals);
    void pruneReleasedLoans();
    [[nodiscard]] static auto isPlace(const hir::Expression &expression) -> bool;
    void requireMutableRoot(const hir::Expression &expression, syntax::SourceSpan span) const;
    void requireMutableDeref(const hir::Expression &expression, const LocalTypes &locals);
    [[nodiscard]] auto isAffine(TypeId type) const -> bool;
    [[nodiscard]] auto isBox(TypeId type) const -> bool;
    [[nodiscard]] auto rootLocalOf(const hir::Expression &expression) const -> std::optional<hir::LocalId>;
    /// D-002 A1.1: the origin root of `expression` — walks through Grouped,
    /// Index, Member, and `ref`/`ref mut`/deref prefixes to the underlying
    /// root local (the only expression whose *origin* can be a parameter).
    [[nodiscard]] static auto originRootLocalOf(const hir::Expression &expression) -> std::optional<hir::LocalId>;
    /// D-002 A1.1: true when returning `expression` (a `T ref` value) is safe
    /// given the enclosing function's declared origin set — i.e. the value's
    /// origin root is a parameter whose ordinal is a declared origin.
    [[nodiscard]] auto returnOriginSatisfied(const hir::Expression &expression) const -> bool;
    /// D-002 A1.2: true when `local` still has an active (non-released) loan of
    /// any kind in the current borrow state. Couples borrow liveness (NLL) with
    /// move/assign checks: moving or reassigning a borrowed binding is unsound.
    [[nodiscard]] bool isBorrowed(uint32_t local) const;
    /// Rejects moving out of `place` (rooted at `root`) while it is borrowed.
    void requireNotBorrowed(uint32_t root, const hir::Expression &place) const;
    /// Rejects assigning through `place` (rooted at `root`) while it is borrowed.
    void requireAssignable(uint32_t root, const hir::Expression &place) const;
    /// Field ordinal of a `root.field` place on a struct/enum local, if any.
    [[nodiscard]] auto memberField(const hir::Expression &member, TypeId rootType) const -> std::optional<uint32_t>;
    void trackConsumption(const hir::Expression &expression, const LocalTypes &locals);
    /// INC-20: a `switch` over an affine enum value consumes the scrutinee
    /// (each arm binds the payload out of it). Mark the owned local/field moved
    /// in `moves` so the enclosing scope drops neither the scrutinee nor its
    /// now-moved affine payloads a second time.
    void consumeSwitchScrutinee(const hir::Expression &expression, const LocalTypes &locals, MoveState &moves);
    /// INC-14 S1 negative contract: a closure body must not reference locals
    /// from an enclosing scope that are not listed in its capture list.
    void validateCaptures(const hir::Expression &closure) const;

    // ---- typecheck_const.cpp ----
    [[nodiscard]] auto evaluateWhereCondition(const hir::Expression &condition, const Substitution &substitution,
                                              const FunctionTypeIds &signature) -> bool;
    [[nodiscard]] auto evaluateConstCall(const hir::Expression &call,
                                         const std::unordered_map<std::string, TypeId> &bindings,
                                         const const_eval::ConstBindings &constBindings) -> const_eval::ConstValueId;
    void checkConstDeclaration(const hir::ConstDeclaration &declaration);
    [[nodiscard]] auto matchConstPatternType(TypeId pattern, TypeId actual, PatternMatchState &state) const -> bool;
    [[nodiscard]] auto matchConstPattern(const ConstDeclChecked &candidate, const std::vector<TypeId> &arguments) const
        -> ConstMatch;
    [[nodiscard]] auto selectConstDeclaration(std::string_view name, const std::vector<TypeId> &arguments,
                                              syntax::SourceSpan span) const -> const ConstDeclChecked *;
    [[nodiscard]] auto evaluateConstDeclaration(const ConstDeclChecked &selected, syntax::SourceSpan span)
        -> const_eval::ConstValueId;
    [[nodiscard]] auto evaluateTupleIntrospection(std::string_view name, const std::vector<TypeId> &arguments,
                                                  syntax::SourceSpan span) -> std::optional<const_eval::ConstValueId>;
    [[nodiscard]] auto evaluateTraitIntrospection(std::string_view name,
                                                  const std::vector<hir::TypeArgument> &arguments,
                                                  syntax::SourceSpan span) -> std::optional<const_eval::ConstValueId>;
    [[nodiscard]] auto evaluateConstApplication(const hir::Expression &expression) -> const_eval::ConstValueId;
    [[nodiscard]] auto unifyPatternArgument(TypeId pattern, TypeId concrete, Substitution &substitution) const -> bool;
    [[nodiscard]] auto matchImplPattern(const ImplInfo &impl, TypeId concreteType, Substitution &substitution) const
        -> bool;
    [[nodiscard]] auto hasImpl(const std::string &traitName, TypeId target) const -> bool;
    [[nodiscard]] auto hasDrop(TypeId type) const -> bool;

    // ---- typecheck_unify.cpp ----
    void unify(TypeId expected, TypeId actual, Substitution &substitution, syntax::SourceSpan span);
    void unifyGuarded(TypeId expected, TypeId actual, Substitution &substitution, syntax::SourceSpan span,
                      std::unordered_set<uint64_t> &visited);
    [[nodiscard]] auto specialize(TypeId type, const Substitution &substitution) -> TypeId;
    void requireType(TypeId expected, TypeId actual, syntax::SourceSpan span, std::string_view context) const;
    [[nodiscard]] auto specializeReturnType(const FunctionTypeIds &signature, const Substitution &substitution,
                                            size_t packCount) -> TypeId;
    [[nodiscard]] auto instantiateFunction(hir::DefId original, const Substitution &substitution, size_t packCount,
                                           syntax::SourceSpan span,
                                           const std::vector<std::pair<size_t, hir::DefId>> &closureTargets = {})
        -> hir::DefId;
    /// Resolves the std list walker used to lower an array spread over a
    /// recursive List enum: matches a module function by name and signature
    /// shape, then instantiates it for the spread's element type (INC-20c S2).
    [[nodiscard]] auto resolveListSpreadCall(std::string_view name, TypeId source, syntax::SourceSpan span)
        -> hir::DefId;
    /// INC-21 S2: post-check growth decisions for one concrete function body.
    /// Computes the may-be-shared array locals (conservative escape analysis)
    /// and, for every `pushBack` call, stamps the in-place vs copy-on-write
    /// flag.
    void applyGrowthDecisions(const hir::Function &function, const LocalTypes &locals);
    [[nodiscard]] auto ensureTraitViewTable(const std::string &traitName, TypeId concreteType) -> bool;
    [[nodiscard]] auto typeContains(TypeId type, TypeId needle) const -> bool;
    [[nodiscard]] auto isGeneric(const FunctionTypeIds &signature) const -> bool;
    void requireConstArguments(const FunctionTypeIds &signature, const Substitution &substitution,
                               std::string_view functionName, syntax::SourceSpan span) const;
    void record(const hir::Expression &expression, TypeId type);
    void recordLocal(hir::LocalId local, TypeId type);
    [[nodiscard]] auto typeFromNumericSuffix(std::string_view suffix) const -> TypeId;
    [[nodiscard]] auto parseIntegerLiteralMagnitude(std::string_view text, TypeId type, syntax::SourceSpan span) const
        -> uint64_t;
    void checkIntegerLiteralMagnitude(uint64_t magnitude, std::string_view text, TypeId type,
                                      syntax::SourceSpan span) const;
    void checkIntegerLiteral(int64_t value, std::string_view text, TypeId type, syntax::SourceSpan span) const;

    // ---- data members ----
    TypeInterner interner_;
    std::unordered_set<uint32_t> reprCStructIds_;
    std::unordered_map<uint32_t, FunctionTypeIds> signatures_;
    std::unordered_map<const hir::Expression *, std::string> expressionTypes_;
    std::unordered_map<const hir::Expression *, TypeId> expressionTypeIds_;
    std::unordered_map<uint32_t, std::string> localDisplayTypes_;
    std::unordered_map<uint32_t, TypeId> localTypeIds_;
    std::unordered_map<uint32_t, FunctionType> functionTypes_;
    std::unordered_map<uint32_t, FunctionTypeIds> functionTypeIds_;
    std::unordered_map<const hir::Expression *, hir::DefId> callTargets_;
    std::unordered_map<const hir::Statement *, bool> constIfSelections_;
    const hir::Module *module_{};
    std::unordered_map<std::string, std::vector<ConstDeclChecked>> constDeclarations_;
    std::unordered_map<std::string, TraitInfo> traits_;
    std::vector<ImplInfo> impls_;
    std::unordered_map<const hir::Expression *, size_t> callPackArgCounts_;
    std::unordered_map<const hir::Expression *, TypeId> callPackTupleTypes_;
    std::unordered_map<const hir::Expression *, std::vector<size_t>> callSpreadPositions_;
    /// Array-spread internals (INC-20c S2): spread node -> std list walker
    /// instances (spreadLength, spreadGet) lowering list spreads as calls.
    std::unordered_map<const hir::Expression *, std::pair<hir::DefId, hir::DefId>> listSpreadCalls_;
    std::unordered_map<const hir::Expression *, std::vector<size_t>> callFoldSpreadPositions_;
    std::unordered_map<const hir::Expression *, std::vector<size_t>> callFoldAccumulatorPositions_;
    /// INC-21 S2: growth-decision flags for `pushBack` call sites (array native
    /// in-place vs copy-on-write), keyed by the call expression.
    std::unordered_map<const hir::Expression *, bool> inPlaceMutationCalls_;

    std::unordered_map<const hir::Statement *, std::vector<std::pair<uint32_t, uint32_t>>> returnDrops_;
    std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> fallthroughDrops_;
    std::unordered_map<const hir::Block *, std::vector<std::pair<uint32_t, uint32_t>>> blockDrops_;
    std::unordered_set<uint32_t> placeholderFunctions_;
    std::unordered_set<uint32_t> derivedCloneTypes_;
    std::unordered_set<uint32_t> derivedEqTypes_;
    std::unordered_set<uint32_t> derivedShowTypes_;
    /// Types that derive Copy: pass-by-value / move of these does not consume
    /// the binding (Copy semantics, D-002 rule 1). INC-17 M2b surfaced that
    /// instance checks clobbered the enclosing move state, hiding an old
    /// conflating of Copy-typed nominals with affine ones.
    std::unordered_set<uint32_t> derivedCopyTypes_;
    std::unordered_map<uint32_t, std::vector<std::string>> dropMovedFields_;
    std::unordered_map<const hir::Expression *, std::pair<std::string, TypeId>> traitViewCoercions_;
    std::unordered_map<const hir::Expression *, TypeId> unionCoercions_;
    std::unordered_map<const hir::Expression *, std::pair<std::string, size_t>> traitViewCalls_;
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::vector<hir::DefId>>> traitViewTables_;
    std::unordered_map<const hir::Expression *, TypeId> derivedCloneCalls_;
    std::unordered_map<const hir::Expression *, TypeId> boxCloneCalls_;
    std::unordered_map<const hir::Expression *, TypeId> derivedShowCalls_;
    /// Scalar type id -> native `toString` DefId (for derive(Show) synthesis).
    std::unordered_map<uint32_t, hir::DefId> showToStringTargets_;
    std::unordered_map<const hir::Expression *, bool> methodReceiverMutable_;
    std::unordered_map<const hir::Expression *, TypeId> methodReceiverRefTypes_;
    std::unordered_map<std::string, hir::DefId> instanceTable_;
    std::deque<hir::Function> instances_;
    /// Closure literal expression -> its synthesized code function (INC-14 S1).
    std::unordered_map<const hir::Expression *, hir::DefId> closureFunctions_;
    /// Closure literal expression -> capture types (INC-14 S3).
    std::unordered_map<const hir::Expression *, std::vector<TypeId>> closureCaptures_;
    /// Closure code function DefId -> capture types (INC-14 S3), for threading
    /// through closure-specialized higher-order instances.
    std::unordered_map<uint32_t, std::vector<TypeId>> closureCaptureTypesByFunction_;
    /// Closure-typed local id -> capture locals {local_id, isMove, type} (INC-14 S3).
    struct CaptureLocalInfo
    {
      uint32_t localId;
      bool isMove;
      TypeId type;
    };
    std::unordered_map<uint32_t, std::vector<CaptureLocalInfo>> closureCapturesByLocal_;
    /// Instance closure-typed param local -> instance capture param locals
    /// (INC-14 S3), so the instance body's closure call threads its own
    /// capture params.
    std::unordered_map<uint32_t, std::vector<uint32_t>> instanceCaptureParamsByLocal_;
    /// Call expression -> capture local IDs to prepend as leading args (INC-14 S3).
    std::unordered_map<const hir::Expression *, std::vector<uint32_t>> callCaptures_;
    /// Closure-typed local id -> its synthesized code function.
    std::unordered_map<uint32_t, hir::DefId> closureLocals_;
    /// Parameter local ids of the current function body; calls through these
    /// resolve as closure calls only when a binding exists (INC-14 S2c).
    std::unordered_set<uint32_t> parameterLocals_;
    /// Parameter local id -> declaration ordinal (D-002 A1.1 origin contracts).
    std::unordered_map<uint32_t, uint32_t> paramOrdinalOf_;
    /// Declared origin ordinals of the current function's scoped-reference
    /// return type (`T ref(a | b)`); empty when the return type has no declared
    /// origins or is not a scoped reference (A1.1).
    std::unordered_set<uint32_t> returnOriginOrdinals_;
    /// Human-readable declared origin list for diagnostics (e.g. `a | b`).
    std::string returnOriginDisplay_;
    /// Closure literals whose synthesized code function would be generic
    /// (param/result/capture type is a type parameter). Model-typed only; the
    /// concrete code function appears at monomorphization (INC-14 S4).
    std::unordered_set<const hir::Expression *> deferredClosures_;
    /// Local ids bound via `let` to a deferred generic closure literal
    /// (INC-14 S4); calls through them defer to the parameter-closure path.
    std::unordered_set<uint32_t> deferredClosureLocals_;
    uint32_t closureCounter_{};
    std::unordered_set<uint32_t> deferredMethodFunctions_;
    hir::DefId currentFunctionId_{};
    uint32_t nextInstanceLocal_{1'000'000};
    std::unordered_set<uint32_t> constFunctions_;
    std::unique_ptr<const_eval::ConstInterpreter> interpreter_;
    const_eval::ConstNativeHost constNativeHost_;
    std::unordered_map<std::string, TypeId> genericBindings_;
    MutableBindings mutableBindings_;
    MoveState moveState_;
    BorrowState borrowState_;
    std::unordered_set<const hir::Expression *> borrowExpressions_;
    std::unordered_set<const hir::Expression *> releasedLoans_;
    std::unordered_map<uint32_t, std::string> defaultMethodTraits_;
    std::unordered_map<const hir::Block *, std::unordered_map<uint32_t, size_t>> lastUseCache_;
    bool inConstGenericFunction_{};
    const_eval::ConstBindings activeConstBindings_;
  };
} // namespace NG::typecheck::detail
