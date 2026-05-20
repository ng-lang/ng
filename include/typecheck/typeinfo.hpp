
#pragma once

#include <ast.hpp>
#include <common.hpp>
#include <memory>
#include <utility>

namespace NG::typecheck
{
    using NG::ast::TypeAnnotationType;
    /**
     * @brief The tag for a `TypeInfo` object.
     */
    enum typeinfo_tag
    {
        UNTYPED,
        NONE,
        BOTTOM,
        PRIMITIVE,
        ANY,
        UNIT,

        PRIMITIVES = 0x28,
        BOOL = 0x29,
        SIGNED = 0x30,
        I8 = 0x31,
        I16,
        I32,
        I64,
        I128,
        UNSIGNED = 0x40,
        U8 = 0x41,
        U16,
        U32,
        U64,
        U128,
        FLOATING_POINT = 0x50,
        F16 = 0x51,
        F32,
        F64,
        F128,
        F256,
        STRING = 0x61,

        COLLECTION_TYPE = 0xA0,
        ARRAY = 0xA1,
        VECTOR = 0xA2,
        TUPLE = 0xA3,
        LIST = 0xA4,
        REFERENCE = 0xA5,
        FUNCTION = 0xA6,
        PARAM_WITH_DEFAULT_VALUE = 0xA7,
        CUSTOMIZED = 0xB0,
        TYPE_ALIAS = 0xB1,
        NEW_TYPE = 0xB2,
        TAGGED_UNION = 0xB3,
        VARIANT = 0xB4,
        UNION = 0xB5,
        TRAIT = 0xB6,

        GENERICS = 0xC0,
        GENERIC_PARAM = 0xC1,
        GENERIC_DEF = 0xC2,
        VARARGS = 0xC3,
        GENERIC_TYPE_DEF = 0xC4,
    };

    /**
     * @brief The base class for all type information objects.
     */
    struct TypeInfo : NG::NonCopyable
    {
        /**
         * @brief Returns the tag of the type info.
         *
         * @return The tag of the type info.
         */
        [[nodiscard]] virtual auto tag() const -> typeinfo_tag = 0;
        /**
         * @brief Returns a string representation of the type info.
         *
         * @return A string representation of the type info.
         */
        [[nodiscard]] virtual auto repr() const -> Str = 0;
        /**
         * @brief Matches this type info against another type info.
         *
         * @param other The other type info.
         * @return `true` if the type infos match, `false` otherwise.
         */
        [[nodiscard]] virtual auto match(const TypeInfo &other) const -> bool = 0;
        virtual ~TypeInfo() = 0;
    };

    /**
     * @brief A shared pointer to a `TypeInfo` object.
     */
    template <class T>
    using CheckingRef = std::shared_ptr<T>;

    /**
     * @brief Creates a `CheckingRef` to a `TypeInfo` object.
     *
     * @tparam T The type of the `TypeInfo` object.
     * @tparam Args The types of the arguments to the constructor of the `TypeInfo` object.
     * @param args The arguments to the constructor of the `TypeInfo` object.
     * @return A `CheckingRef` to the new `TypeInfo` object.
     */
    template <class T, class... Args>
    [[nodiscard]] inline auto makecheck(Args &&...args) -> CheckingRef<T>
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief An untyped type.
     */
    struct Untyped : TypeInfo
    {
        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;

        auto match(const TypeInfo &other) const -> bool override;
        ~Untyped() override;
    };

    /**
     * @brief A primitive type.
     */
    struct PrimitiveType : TypeInfo
    {

        typeinfo_tag type; ///< The tag of the primitive type.

        explicit PrimitiveType(typeinfo_tag tag) noexcept : type(tag) {}
        /**
         * @brief Creates a `PrimitiveType` from a `TypeAnnotationType`.
         *
         * @param annotation The `TypeAnnotationType`.
         * @return A `CheckingRef` to the new `PrimitiveType`.
         */
        static auto from(const TypeAnnotationType &annotation) -> CheckingRef<PrimitiveType>;
        /**
         * @brief Creates a `PrimitiveType` from a string.
         *
         * @param primitive_type The string.
         * @return A `CheckingRef` to the new `PrimitiveType`.
         */
        static auto from(const Str &primitive_type) -> CheckingRef<PrimitiveType>;

        /**
         * @brief Returns the tag of the primitive type.
         *
         * @return The tag of the primitive type.
         */
        auto primitive() const -> typeinfo_tag;
        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A function type.
     */
    struct FunctionType : TypeInfo
    {
        CheckingRef<TypeInfo> returnType;          ///< The return type of the function.
        Vec<CheckingRef<TypeInfo>> parametersType; ///< The parameter types of the function.

        FunctionType(CheckingRef<TypeInfo> returnType, Vec<CheckingRef<TypeInfo>> parametersType)
            : returnType(returnType), parametersType(parametersType)
        {
        }
        /**
         * @brief Applies the function with the given types.
         *
         * @param types The types to apply with.
         * @return `true` if the application is successful, `false` otherwise.
         */
        [[nodiscard]] auto applyWith(const Vec<CheckingRef<TypeInfo>> &types) const -> bool;

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A parameter with a default value type.
     */
    struct ParamWithDefaultValueType : TypeInfo
    {
        CheckingRef<TypeInfo> paramType; ///< The type of the parameter.

        ParamWithDefaultValueType(CheckingRef<TypeInfo> paramType) : paramType(paramType) {}

        auto tag() const -> typeinfo_tag override;

        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A array type.
     */
    struct ArrayType : TypeInfo
    {
        CheckingRef<TypeInfo> elementType; ///< The element type of the array.

        ArrayType(CheckingRef<TypeInfo> elementType) : elementType(elementType) {}
        /**
         * @brief Applies the function with the given types.
         *
         * @param types The types to apply with.
         * @return `true` if the application is successful, `false` otherwise.
         */
        [[nodiscard]] auto containing(const TypeInfo &other) const -> bool;

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A tuple type.
     */
    struct TupleType : TypeInfo
    {
        Vec<CheckingRef<TypeInfo>> elementTypes; ///< The element types of the tuple.

        explicit TupleType(Vec<CheckingRef<TypeInfo>> elementTypes) : elementTypes(std::move(elementTypes)) {}

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A reference type.
     */
    struct ReferenceType : TypeInfo
    {
        CheckingRef<TypeInfo> referencedType;

        explicit ReferenceType(CheckingRef<TypeInfo> referencedType) : referencedType(std::move(referencedType)) {}

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A customized type (user-defined object).
     */
    struct CustomizedType : TypeInfo
    {
        Str name;
        Str moduleId;
        bool nativeOpaque = false;
        bool abstract = false;
        Map<Str, CheckingRef<TypeInfo>> properties;
        Map<Str, CheckingRef<FunctionType>> memberFunctions;
        Map<Str, Map<Str, CheckingRef<FunctionType>>> traitMemberFunctions;

        explicit CustomizedType(Str name, bool nativeOpaque = false, bool abstract = false, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), nativeOpaque(nativeOpaque), abstract(abstract) {}

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    struct TraitType : TypeInfo
    {
        Str name;
        Str moduleId;
        Vec<Str> typeParamNames;
        Vec<CheckingRef<TraitType>> superTraits;
        Map<Str, CheckingRef<FunctionType>> methods;
        Map<Str, CheckingRef<FunctionType>> allMethods;
        Map<Str, ast::FunctionDef *> defaultMethods;
        Map<Str, ast::FunctionDef *> allDefaultMethods;
        Map<Str, Str> allMethodOrigins;
        Map<Str, Str> allDefaultOrigins;

        explicit TraitType(Str name, Vec<Str> typeParamNames = {}, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), typeParamNames(std::move(typeParamNames))
        {
        }

        auto tag() const -> typeinfo_tag override { return typeinfo_tag::TRAIT; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A transparent type alias (type A = B;).
     */
    struct TypeAliasType : TypeInfo
    {
        Str name;
        Str moduleId;
        CheckingRef<TypeInfo> underlyingType;

        TypeAliasType(Str name, CheckingRef<TypeInfo> underlying, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), underlyingType(std::move(underlying)) {}

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief An opaque newtype (type A wraps B;).
     */
    struct NewTypeType : TypeInfo
    {
        Str name;
        Str moduleId;
        CheckingRef<TypeInfo> wrappedType;

        NewTypeType(Str name, CheckingRef<TypeInfo> wrapped, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), wrappedType(std::move(wrapped)) {}

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A tagged union type (type Result = Ok(i32) | Err(string);).
     */
    struct TaggedUnionType : TypeInfo
    {
        Str name;
        Str moduleId;
        Map<Str, Vec<CheckingRef<TypeInfo>>> variants;  // variant name -> payload types
        Map<Str, Vec<Str>> variantPayloadNames;          // variant name -> payload field names (if named)

        explicit TaggedUnionType(Str name, Str moduleId = "") : name(std::move(name)), moduleId(std::move(moduleId)) {}

        auto tag() const -> typeinfo_tag override { return TAGGED_UNION; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A single variant type within a tagged union (e.g. Ok(i32)).
     */
    struct VariantType : TypeInfo
    {
        Str unionName;
        Str moduleId;
        Str variantName;
        int32_t variantIndex;
        Vec<CheckingRef<TypeInfo>> payloadTypes;
        Vec<Str> payloadNames;  ///< Named fields (e.g. Ok(value: i32))

        VariantType(Str unionName, Str variantName, int32_t variantIndex, Vec<CheckingRef<TypeInfo>> payloadTypes, Vec<Str> payloadNames = {}, Str moduleId = "")
            : unionName(std::move(unionName)), moduleId(std::move(moduleId)), variantName(std::move(variantName)),
              variantIndex(variantIndex), payloadTypes(std::move(payloadTypes)), payloadNames(std::move(payloadNames)) {}

        auto tag() const -> typeinfo_tag override { return VARIANT; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A structural union type (A | B).
     */
    struct UnionType : TypeInfo
    {
        Vec<CheckingRef<TypeInfo>> types;

        explicit UnionType(Vec<CheckingRef<TypeInfo>> types) : types(std::move(types)) {}

        auto tag() const -> typeinfo_tag override { return UNION; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A generic type parameter (e.g. T in `fn identity<T>(x: T) -> T`).
     *
     * Represents a placeholder type that will be resolved during monomorphization.
     */
    struct GenericParamType : TypeInfo
    {
        Str name;                           ///< The name of the type parameter (e.g. "T")
        Str bound;                          ///< Optional trait/bound constraint (empty if none)
        bool isPack = false;                ///< Whether this is a parameter pack (T...)

        explicit GenericParamType(Str name, Str bound = "", bool isPack = false)
            : name(std::move(name)), bound(std::move(bound)), isPack(isPack) {}

        auto tag() const -> typeinfo_tag override { return GENERIC_PARAM; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A generic function or type definition that has not yet been monomorphized.
     *
     * Stores the AST node and type parameter names so that a specialized copy
     * can be produced when concrete type arguments are known.
     */
    struct GenericDefType : TypeInfo
    {
        using TypeEnv = Map<Str, CheckingRef<TypeInfo>>;

        Str name;                                   ///< The name of the generic definition
        Str moduleId;                               ///< Canonical module that owns this generic definition.
        Vec<Str> typeParamNames;                    ///< Names of type parameters (e.g. ["T", "U"])
        Vec<bool> typeParamIsPack;                  ///< Which type params are packs (parallel to typeParamNames)
        NG::ast::ASTRef<NG::ast::FunctionDef> funcDef; ///< The original AST node (for generic functions)
        TypeEnv capturedLocals;                     ///< Local type environment at definition site
        Map<Str, CheckingRef<TypeInfo>> instances; ///< Monomorphized return types keyed by instantiated name.

        GenericDefType(Str name, Vec<Str> typeParamNames, Vec<bool> typeParamIsPack,
                       NG::ast::ASTRef<NG::ast::FunctionDef> funcDef, TypeEnv capturedLocals, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), typeParamNames(std::move(typeParamNames)),
              typeParamIsPack(std::move(typeParamIsPack)),
              funcDef(std::move(funcDef)), capturedLocals(std::move(capturedLocals)) {}

        auto tag() const -> typeinfo_tag override { return GENERIC_DEF; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    enum class GenericTypeKind : uint8_t
    {
        TYPE_DEF,
        TYPE_ALIAS,
        NEW_TYPE,
        TAGGED_UNION,
    };

    struct GenericTypeDef : TypeInfo
    {
        using TypeEnv = Map<Str, CheckingRef<TypeInfo>>;

        Str name;
        Str moduleId;
        GenericTypeKind kind;
        Vec<Str> typeParamNames;
        Vec<bool> typeParamIsPack;
        NG::ast::ASTRef<NG::ast::TypeDef> typeDef = nullptr;
        NG::ast::ASTRef<NG::ast::TypeAliasDef> typeAliasDef = nullptr;
        Vec<NG::ast::TypeAliasDef *> specializations;
        NG::ast::ASTRef<NG::ast::NewTypeDef> newTypeDef = nullptr;
        NG::ast::ASTRef<NG::ast::TaggedUnionDef> taggedUnionDef = nullptr;
        TypeEnv capturedLocals;
        Map<Str, CheckingRef<TypeInfo>> instances;

        GenericTypeDef(Str name, Vec<Str> typeParamNames, Vec<bool> typeParamIsPack,
                       NG::ast::ASTRef<NG::ast::TypeDef> typeDef, TypeEnv capturedLocals, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), kind(GenericTypeKind::TYPE_DEF), typeParamNames(std::move(typeParamNames)),
              typeParamIsPack(std::move(typeParamIsPack)), typeDef(std::move(typeDef)),
              capturedLocals(std::move(capturedLocals)) {}

        GenericTypeDef(Str name, Vec<Str> typeParamNames, Vec<bool> typeParamIsPack,
                       NG::ast::ASTRef<NG::ast::TypeAliasDef> typeAliasDef, TypeEnv capturedLocals, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), kind(GenericTypeKind::TYPE_ALIAS), typeParamNames(std::move(typeParamNames)),
              typeParamIsPack(std::move(typeParamIsPack)), typeAliasDef(std::move(typeAliasDef)),
              capturedLocals(std::move(capturedLocals)) {}

        GenericTypeDef(Str name, Vec<Str> typeParamNames, Vec<bool> typeParamIsPack,
                       NG::ast::ASTRef<NG::ast::NewTypeDef> newTypeDef, TypeEnv capturedLocals, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), kind(GenericTypeKind::NEW_TYPE), typeParamNames(std::move(typeParamNames)),
              typeParamIsPack(std::move(typeParamIsPack)), newTypeDef(std::move(newTypeDef)),
              capturedLocals(std::move(capturedLocals)) {}

        GenericTypeDef(Str name, Vec<Str> typeParamNames, Vec<bool> typeParamIsPack,
                       NG::ast::ASTRef<NG::ast::TaggedUnionDef> taggedUnionDef, TypeEnv capturedLocals, Str moduleId = "")
            : name(std::move(name)), moduleId(std::move(moduleId)), kind(GenericTypeKind::TAGGED_UNION), typeParamNames(std::move(typeParamNames)),
              typeParamIsPack(std::move(typeParamIsPack)), taggedUnionDef(std::move(taggedUnionDef)),
              capturedLocals(std::move(capturedLocals)) {}

        auto tag() const -> typeinfo_tag override { return GENERIC_TYPE_DEF; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A variadic parameter pack type (e.g. T... in `fn print<T...>(args: T...)`).
     *
     * Contains the element types collected from the remaining arguments.
     * During monomorphization, a parameter pack `T...` bound to args `(i32, string, bool)`
     * produces a VarargsType with elementTypes = [i32, string, bool].
     */
    struct VarargsType : TypeInfo
    {
        Vec<CheckingRef<TypeInfo>> elementTypes; ///< The concrete types captured by the pack

        explicit VarargsType(Vec<CheckingRef<TypeInfo>> elementTypes)
            : elementTypes(std::move(elementTypes)) {}

        /// Convenience: wrap a single element type (e.g. when resolving `T...` annotation)
        explicit VarargsType(CheckingRef<TypeInfo> singleType)
            : elementTypes{std::move(singleType)} {}

        auto tag() const -> typeinfo_tag override { return VARARGS; }
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

} // namespace NG::typecheck
