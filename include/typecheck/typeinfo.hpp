
#pragma once

#include <common.hpp>
#include <memory>
#include <ast.hpp>

namespace NG::typecheck
{
    using NG::ast::TypeAnnotationType;
    /**
     * @brief The tag for a `TypeInfo` object.
     */
    enum typeinfo_tag
    {
        UNTYPED,
        PRIMITIVE,
        COLLECTION,
        STRUCTURAL,
        CUSTOMIZED,
        POLYMORPHIC,
        PARAMETER,
        ARGUMENT,
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
     * @brief The tag for a primitive type.
     */
    enum primitive_tag
    {
        NONE = 0x00,
        ANY,
        UNIT,
        BOOL,
        SIGNED = 0x10,
        I8 = 0x11,
        I16,
        I32,
        I64,
        I128,
        UNSIGNED = 0x20,
        U8 = 0x21,
        U16,
        U32,
        U64,
        U128,
        FLOATING_POINT = 0x30,
        F16 = 0x31,
        F32,
        F64,
        F128,
        F256,
        STRING = 0x41,
    };

    /**
     * @brief The tag for a collection type.
     */
    enum collection_type_tag
    {
        UNKNOWN,
        ARRAY = 0x11,
        TUPLE = 0x21,
        REFERENCE = 0x31,
        FUNCTION = 0x51,
        PARAM_WITH_DEFAULT_VALUE = 0x52,
    };

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
     * @brief A collection type.
     */
    struct CollectionType : TypeInfo
    {
        /**
         * @brief Returns the tag of the collection type.
         *
         * @return The tag of the collection type.
         */
        virtual auto collection_tag() const -> collection_type_tag = 0;
        auto tag() const -> typeinfo_tag override
        {
            return typeinfo_tag::COLLECTION;
        }
    };

    /**
     * @brief A primitive type.
     */
    struct PrimitiveType : TypeInfo
    {

        primitive_tag type; ///< The tag of the primitive type.

        explicit PrimitiveType(primitive_tag tag) noexcept : type(tag)
        {
        }
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
        auto primitive() const -> primitive_tag;
        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A function type.
     */
    struct FunctionType : CollectionType
    {
        CheckingRef<TypeInfo> returnType;          ///< The return type of the function.
        Vec<CheckingRef<TypeInfo>> parametersType; ///< The parameter types of the function.

        FunctionType(CheckingRef<TypeInfo> returnType, Vec<CheckingRef<TypeInfo>> parametersType)
            : returnType(returnType), parametersType(parametersType)
        {
        }

        auto collection_tag() const -> collection_type_tag override;
        /**
         * @brief Applies the function with the given types.
         *
         * @param types The types to apply with.
         * @return `true` if the application is successful, `false` otherwise.
         */
        [[nodiscard]] auto applyWith(const Vec<CheckingRef<TypeInfo>> &types) const -> bool;

        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    /**
     * @brief A parameter with a default value type.
     */
    struct ParamWithDefaultValueType : CollectionType
    {
        CheckingRef<TypeInfo> paramType; ///< The type of the parameter.

        ParamWithDefaultValueType(CheckingRef<TypeInfo> paramType)
            : paramType(paramType)
        {
        }

        auto collection_tag() const -> collection_type_tag override;

        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

}