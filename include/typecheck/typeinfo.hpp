
#pragma once

#include <common.hpp>
#include <memory>
#include <ast.hpp>

namespace NG::typecheck
{
    using NG::ast::TypeAnnotationType;
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

    struct TypeInfo : NG::NonCopyable
    {
        virtual auto tag() const -> typeinfo_tag = 0;
        virtual auto repr() const -> Str = 0;
        virtual auto match(const TypeInfo &other) const -> bool = 0;
        virtual ~TypeInfo() = 0;
    };

    template <class T>
    using CheckingRef = std::shared_ptr<T>;

    template <class T, class... Args>
    inline auto makecheck(Args &&...args) -> CheckingRef<T>
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

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

    enum collection_type_tag
    {
        UNKNOWN,
        ARRAY = 0x11,
        TUPLE = 0x21,
        REFERENCE = 0x31,
        FUNCTION = 0x51,
        PARAM_WITH_DEFAULT_VALUE = 0x52,
    };

    struct Untyped : TypeInfo
    {
        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;

        auto match(const TypeInfo &other) const -> bool override;
        ~Untyped() override;
    };

    struct CollectionType : TypeInfo
    {
        virtual auto collection_tag() const -> collection_type_tag = 0;
        auto tag() const -> typeinfo_tag override
        {
            return typeinfo_tag::COLLECTION;
        }
    };

    struct PrimitiveType : TypeInfo
    {

        primitive_tag type;

        PrimitiveType(primitive_tag tag) : type(tag)
        {
        }
        static auto from(const TypeAnnotationType &annotation) -> CheckingRef<PrimitiveType>;
        static auto from(const Str &primitive_type) -> CheckingRef<PrimitiveType>;

        auto primitive() const -> primitive_tag;
        auto tag() const -> typeinfo_tag override;
        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    struct FunctionType : CollectionType
    {
        CheckingRef<TypeInfo> returnType;
        Vec<CheckingRef<TypeInfo>> parametersType;

        FunctionType(CheckingRef<TypeInfo> returnType, Vec<CheckingRef<TypeInfo>> parametersType)
            : returnType(returnType), parametersType(parametersType)
        {
        }

        auto collection_tag() const -> collection_type_tag override;
        auto applyWith(const Vec<CheckingRef<TypeInfo>> &types) const -> bool;

        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

    struct ParamWithDefaultValueType : CollectionType
    {
        CheckingRef<TypeInfo> paramType;

        ParamWithDefaultValueType(CheckingRef<TypeInfo> paramType)
            : paramType(paramType)
        {
        }

        auto collection_tag() const -> collection_type_tag override;

        auto repr() const -> Str override;
        auto match(const TypeInfo &other) const -> bool override;
    };

}