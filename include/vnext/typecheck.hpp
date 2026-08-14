// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/const_eval.hpp"
#include "vnext/hir.hpp"
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NG::vnext::typecheck
{
  struct TypeId
  {
    uint32_t value{};
    auto operator==(const TypeId &) const -> bool = default;
  };

  namespace builtin
  {
    inline constexpr TypeId I64{1};
    inline constexpr TypeId U8{2};
    inline constexpr TypeId Bool{3};
    inline constexpr TypeId Unit{4};
    inline constexpr TypeId String{5};
    inline constexpr TypeId I8{6};
    inline constexpr TypeId I16{7};
    inline constexpr TypeId I32{8};
    inline constexpr TypeId U16{9};
    inline constexpr TypeId U32{10};
    inline constexpr TypeId U64{11};
    inline constexpr TypeId F32{12};
    inline constexpr TypeId F64{13};
  } // namespace builtin

  /// True for the D-008 fixed-width integer builtin types (i8–i64, u8–u64).
  [[nodiscard]] inline auto isIntegerBuiltin(TypeId type) -> bool
  {
    return type == builtin::I8 || type == builtin::I16 || type == builtin::I32 || type == builtin::I64 ||
           type == builtin::U8 || type == builtin::U16 || type == builtin::U32 || type == builtin::U64;
  }

  /// True for the D-008 floating-point builtin types (f32, f64).
  [[nodiscard]] inline auto isFloatBuiltin(TypeId type) -> bool
  {
    return type == builtin::F32 || type == builtin::F64;
  }

  /// True for any numeric builtin type (integer or float).
  [[nodiscard]] inline auto isNumericBuiltin(TypeId type) -> bool
  {
    return isIntegerBuiltin(type) || isFloatBuiltin(type);
  }

  struct TypeError : std::runtime_error
  {
    syntax::SourceSpan span;

    TypeError(std::string message, syntax::SourceSpan sourceSpan)
      : std::runtime_error(std::move(message)), span(sourceSpan)
    {
    }
  };

  enum class TypeKind
  {
    Builtin,
    DynamicArray,
    FixedArray,
    DependentArray,
    Reference,
    RawPointer,
    TypePack,
    Range,
    Tuple,
    /// `A | B | C` union of member types (legacy 19).
    Union,
    Struct,
    Enum,
    TypeParameter,
    /// `type Name;` (abstract) or `type Name = native;` (native opaque handle).
    Opaque,
    /// `F<_>` generic parameter: a type constructor of kind `* -> *`.
    TypeConstructor,
    /// `F<T>` application of a type constructor to one type argument.
    TypeApplication,
    /// Declaration-only trait placeholder (D-011 stage 2); not a value type,
    /// only usable under `ref<...>` as a trait view.
    Trait,
    /// `ref<Trait>` dynamic view: a reference to a value implementing the
    /// named trait plus a dispatch table index.
    TraitReference,
  };

  struct TypeDescriptor
  {
    TypeKind kind;
    std::string name;
    TypeId element;
    std::optional<uint64_t> length;
    std::vector<TypeId> elements;
    std::optional<uint32_t> nominalId;
    std::vector<std::string> fieldNames;
    std::vector<bool> variantHasPayload;
    std::vector<TypeId> typeArguments;
    std::optional<uint32_t> constParameterIndex{};
    std::string constParameterName;
    bool referenceMutable{};
    /// For Opaque descriptors: true for `type Name;` (no representation),
    /// false for `type Name = native;` (native handle).
    bool abstractType{};
    /// For TypeConstructor descriptors: `F<_, ...>` variadic kind.
    bool variadicConstructor{};
    auto operator==(const TypeDescriptor &) const -> bool = default;
  };

  class TypeInterner final
  {
  public:
    /// Maps const-parameter names to their declaration index inside the
    /// current generic function signature.
    using ConstParamBindings = std::unordered_map<std::string, uint32_t>;
    /// Maps const-parameter declaration indexes to canonical const values
    /// during generic instantiation.
    using ConstSubstitution = std::unordered_map<uint32_t, const_eval::ConstValueId>;

    TypeInterner();

    [[nodiscard]] auto specialize(TypeId type, const std::unordered_map<uint32_t, TypeId> &bindings) -> TypeId;
    [[nodiscard]] auto specialize(TypeId type, const std::unordered_map<uint32_t, TypeId> &bindings,
                                  const ConstSubstitution &constBindings) -> TypeId;
    /// Constructor substitution maps constructor-parameter indexes to their
    /// template base types (struct or enum).
    using ConstructorSubstitution = std::unordered_map<uint32_t, TypeId>;
    [[nodiscard]] auto specialize(TypeId type, const std::unordered_map<uint32_t, TypeId> &bindings,
                                  const ConstSubstitution &constBindings,
                                  const ConstructorSubstitution &constructors) -> TypeId;
    [[nodiscard]] auto internConstInteger(int64_t value) -> const_eval::ConstValueId;
    [[nodiscard]] auto constInterner() -> const_eval::ConstInterner & { return constInterner_; }
    [[nodiscard]] auto resolveInScope(const hir::Type &type, const std::unordered_map<std::string, TypeId> &bindings,
                                      const ConstParamBindings &constBindings = {}) -> TypeId;
    [[nodiscard]] auto resolve(const hir::Type &type) -> TypeId;
    [[nodiscard]] auto internDynamicArray(TypeId element) -> TypeId;
    [[nodiscard]] auto internFixedArray(TypeId element, uint64_t length) -> TypeId;
    [[nodiscard]] auto internDependentArray(TypeId element, uint32_t constParameterIndex, std::string name) -> TypeId;
    [[nodiscard]] auto internReference(TypeId target, bool mutableReference) -> TypeId;
    [[nodiscard]] auto internRawPointer(TypeId target, bool mutablePointee) -> TypeId;
    [[nodiscard]] auto internTuple(const std::vector<TypeId> &elements) -> TypeId;
    [[nodiscard]] auto internUnion(const std::vector<TypeId> &members) -> TypeId;
    [[nodiscard]] auto internTypePack(TypeId element) -> TypeId;
    [[nodiscard]] auto internRange(TypeId element) -> TypeId;
    [[nodiscard]] auto declareTraitType(std::string name) -> TypeId;
    [[nodiscard]] auto internTraitReference(std::string traitName) -> TypeId;
    [[nodiscard]] auto internTypeParameter(std::string name, uint32_t index) -> TypeId;
    [[nodiscard]] auto internTypeConstructor(std::string name, uint32_t index, bool variadic = false) -> TypeId;
    [[nodiscard]] auto internTypeApplication(std::string constructorName, uint32_t constructorIndex,
                                             const std::vector<TypeId> &arguments) -> TypeId;
    [[nodiscard]] auto declareStruct(hir::StructId id, std::string name) -> TypeId;
    void registerStructTemplate(const hir::Struct &structure);
    [[nodiscard]] auto structGenericArity(hir::StructId id) const -> size_t;
    [[nodiscard]] auto declareOpaqueType(const hir::OpaqueType &opaque) -> TypeId;
    [[nodiscard]] auto opaqueTemplateArity(const hir::OpaqueType &opaque) const -> size_t;
    void defineStruct(hir::StructId id, std::vector<std::string> fields, std::vector<TypeId> types);
    [[nodiscard]] auto typeForStruct(hir::StructId id) const -> TypeId;
    /// Looks a declared type up by name without instantiating generics;
    /// used for explicit type-constructor generic arguments (`accept<Box, ...>`).
    [[nodiscard]] auto templateForName(const std::string &name, syntax::SourceSpan span) const -> TypeId;
    [[nodiscard]] auto declareEnum(hir::EnumId id, std::string name, std::vector<std::string> genericParameters = {}) -> TypeId;
    void registerEnumTemplate(const hir::Enum &enumeration);
    void defineEnum(hir::EnumId id, std::vector<std::string> variants, std::vector<TypeId> payloads,
                    std::vector<bool> hasPayload);
    [[nodiscard]] auto typeForEnum(hir::EnumId id) const -> TypeId;
    [[nodiscard]] auto enumGenericArity(hir::EnumId id) const -> size_t;
    [[nodiscard]] auto descriptor(TypeId type) const -> const TypeDescriptor &;
    [[nodiscard]] auto display(TypeId type) const -> std::string;
    [[nodiscard]] auto descriptors() const -> const std::vector<TypeDescriptor> & { return descriptors_; }

  private:
    [[nodiscard]] auto append(TypeDescriptor descriptor) -> TypeId;
    [[nodiscard]] auto resolveWithBindings(const hir::Type &type, const std::unordered_map<std::string, TypeId> &bindings,
                                           const ConstParamBindings &constBindings) -> TypeId;
    /// Resolves the built-in tuple introspection type constructors
    /// `tuple_element<T, I>` and `tuple_concat<A, B>`; returns nullopt for
    /// any other constructor name.
    [[nodiscard]] auto resolveTupleIntrospection(const hir::Type &type,
                                                 const std::unordered_map<std::string, TypeId> &bindings,
                                                 const ConstParamBindings &constBindings) -> std::optional<TypeId>;
    [[nodiscard]] auto evaluateArrayLength(const hir::TypeArgument &argument) -> uint64_t;
    /// Instantiates a generic struct template with concrete arguments,
    /// interning a per-instance descriptor that shares the nominal id.
    [[nodiscard]] auto internStructInstance(TypeId templateType, const std::vector<TypeId> &arguments) -> TypeId;
    /// Instantiates a parameterized opaque template with concrete arguments.
    [[nodiscard]] auto internOpaqueInstance(TypeId templateType, const std::vector<TypeId> &arguments) -> TypeId;
    std::vector<TypeDescriptor> descriptors_;
    std::unordered_map<std::string, TypeId> namedTypes_;
    std::unordered_map<uint32_t, TypeId> structTypes_;
    std::unordered_map<uint32_t, std::vector<std::string>> structGenericParameters_;
    std::unordered_map<uint32_t, const hir::Struct *> structTemplates_;
    std::unordered_map<uint32_t, const hir::OpaqueType *> opaqueTemplates_;
    std::unordered_map<uint32_t, TypeId> enumTypes_;
    std::unordered_map<uint32_t, std::vector<std::string>> enumGenericParameters_;
    std::unordered_map<uint32_t, const hir::Enum *> enumTemplates_;
    const_eval::ConstInterner constInterner_;
  };

  struct FunctionType
  {
    std::vector<std::string> parameters;
    std::string returnType;
  };

  struct FunctionTypeIds
  {
    std::vector<TypeId> parameters;
    std::vector<TypeId> genericParameters;
    std::vector<std::string> genericParameterNames;
    std::vector<TypeId> packParameters;
    std::vector<std::string> packParameterNames;
    std::vector<TypeId> constructorParameters;
    std::vector<std::string> constructorParameterNames;
    /// Declaration order of every explicit generic parameter kind (Type,
    /// TypeConstructor, Const); call-site generic arguments map positionally.
    std::vector<syntax::GenericParameterKind> explicitParameterOrder;
    std::vector<TypeId> constParameters;
    std::vector<std::string> constParameterNames;
    TypeId returnType;
  };

  /// Immutable type side tables for a resolved HIR module. The table is keyed
  /// by HIR node identity or stable resolved ids; neither syntax nor HIR nodes
  /// are checker-mutated.
  struct TypeCheckResult
  {
    std::unordered_map<const hir::Expression *, std::string> expressionTypes;
    std::unordered_map<const hir::Expression *, TypeId> expressionTypeIds;
    std::unordered_map<uint32_t, std::string> localTypes;
    std::unordered_map<uint32_t, TypeId> localTypeIds;
    std::unordered_map<uint32_t, FunctionType> functionTypes;
    std::unordered_map<uint32_t, FunctionTypeIds> functionTypeIds;
    std::unordered_map<const hir::Expression *, hir::DefId> callTargets;
    /// Number of trailing call arguments packed into the callee's tuple
    /// parameter (variadic calls).
    std::unordered_map<const hir::Expression *, size_t> callPackArgCounts;
    /// Static tuple type of a variadic call's packed trailing arguments.
    std::unordered_map<const hir::Expression *, TypeId> callPackTupleTypes;
    /// Syntactic argument offsets that are tuple spreads at a call site.
    std::unordered_map<const hir::Expression *, std::vector<size_t>> callSpreadPositions;
    /// Fold calls (`f(acc, xs...)`): spread/accumulator positions and element
    /// type, lowered to reduction loops.
    std::unordered_map<const hir::Expression *, std::vector<size_t>> callFoldSpreadPositions;
    std::unordered_map<const hir::Expression *, std::vector<size_t>> callFoldAccumulatorPositions;
    /// Drop calls to emit at return statements: (local, drop method DefId).
    std::unordered_map<const hir::Statement *, std::vector<std::pair<uint32_t, uint32_t>>> returnDrops;
    /// Drop calls to emit at function fall-through, keyed by function id.
    std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> fallthroughDrops;
    /// Drop calls to emit when a nested block scope exits, keyed by block.
    std::unordered_map<const hir::Block *, std::vector<std::pair<uint32_t, uint32_t>>> blockDrops;
    /// Declared functions whose bodies cannot be lowered type-erased
    /// (variadic originals); the driver emits inert placeholders.
    std::unordered_set<uint32_t> placeholderFunctions;
    /// Whether the receiver of a method call must be borrowed mutably.
    std::unordered_map<const hir::Expression *, bool> methodReceiverMutable;
    /// The typed reference form of a method receiver (`Self ref` / `Self ref mut`).
    std::unordered_map<const hir::Expression *, TypeId> methodReceiverRefTypes;
    /// Per-`const if` branch selection: true means the consequence was chosen.
    /// The inactive branch is resolved but never typechecked or lowered.
    std::unordered_map<const hir::Statement *, bool> constIfSelections;
    /// Monomorphized generic function instances (D-002 instance model): the
    /// driver lowers these alongside the module's declared functions.
    std::vector<hir::Function> instances;
    /// Declared functions containing deferred (abstract) method calls; their
    /// bodies cannot be lowered type-erased and are skipped by the driver.
    std::unordered_set<uint32_t> deferredMethodFunctions;
    /// Derived `clone()` calls: expression -> receiver type; lowered to a
    /// shared borrow followed by a deep-copying load.
    std::unordered_map<const hir::Expression *, TypeId> derivedCloneCalls;
    /// Trait-view coercions (`ref<Trait>`): expression -> (trait, concrete type).
    std::unordered_map<const hir::Expression *, std::pair<std::string, TypeId>> traitViewCoercions;
    /// Dynamic method calls through trait views: expression -> (trait, method index).
    std::unordered_map<const hir::Expression *, std::pair<std::string, size_t>> traitViewCalls;
    /// Dispatch tables: trait name -> concrete type id -> method DefIds in
    /// trait declaration order.
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::vector<hir::DefId>>> traitViewTables;
    std::vector<TypeDescriptor> typeDescriptors;

    [[nodiscard]] auto typeOf(const hir::Expression &expression) const -> const std::string &
    {
      return expressionTypes.at(&expression);
    }

    [[nodiscard]] auto typeIdOf(const hir::Expression &expression) const -> TypeId
    {
      return expressionTypeIds.at(&expression);
    }
  };

  /// First typed-HIR validation slice. It is intentionally a side-table-style
  /// pass over resolved HIR: syntax/HIR nodes remain free of checker mutation.
  class TypeChecker final
  {
  public:
    [[nodiscard]] auto check(const hir::Module &module) -> TypeCheckResult;
  };
} // namespace NG::vnext::typecheck
