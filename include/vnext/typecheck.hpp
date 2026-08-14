// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/const_eval.hpp"
#include "vnext/hir.hpp"
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
  } // namespace builtin

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
    Tuple,
    Struct,
    Enum,
    TypeParameter,
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
    [[nodiscard]] auto internTypeParameter(std::string name, uint32_t index) -> TypeId;
    [[nodiscard]] auto declareStruct(hir::StructId id, std::string name) -> TypeId;
    void defineStruct(hir::StructId id, std::vector<std::string> fields, std::vector<TypeId> types);
    [[nodiscard]] auto typeForStruct(hir::StructId id) const -> TypeId;
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
    [[nodiscard]] auto evaluateArrayLength(const hir::TypeArgument &argument) -> uint64_t;
    std::vector<TypeDescriptor> descriptors_;
    std::unordered_map<std::string, TypeId> namedTypes_;
    std::unordered_map<uint32_t, TypeId> structTypes_;
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
    /// Whether the receiver of a method call must be borrowed mutably.
    std::unordered_map<const hir::Expression *, bool> methodReceiverMutable;
    /// The typed reference form of a method receiver (`Self ref` / `Self ref mut`).
    std::unordered_map<const hir::Expression *, TypeId> methodReceiverRefTypes;
    /// Per-`const if` branch selection: true means the consequence was chosen.
    /// The inactive branch is resolved but never typechecked or lowered.
    std::unordered_map<const hir::Statement *, bool> constIfSelections;
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
