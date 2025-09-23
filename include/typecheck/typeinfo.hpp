
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

        explicit PrimitiveType(typeinfo_tag tag) noexcept : type(tag)
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

        ParamWithDefaultValueType(CheckingRef<TypeInfo> paramType)
            : paramType(paramType)
        {
        }

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

        ArrayType(CheckingRef<TypeInfo> elementType)
            : elementType(elementType)
        {
        }
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

        explicit TupleType(Vec<CheckingRef<TypeInfo>> elementTypes)
            : elementTypes(std::move(elementTypes))
        {
        }

        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

}
