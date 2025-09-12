#include <typecheck/typeinfo.hpp>
#include <debug.hpp>

namespace NG::typecheck
{

    auto PrimitiveType::from(const TypeAnnotationType &annotation) -> CheckingRef<PrimitiveType>
    {
        switch (annotation)
        {
        case TypeAnnotationType::BUILTIN_UNIT:
            return makecheck<PrimitiveType>(typeinfo_tag::UNIT);
        case TypeAnnotationType::BUILTIN_BOOL:
            return makecheck<PrimitiveType>(typeinfo_tag::BOOL);
        case TypeAnnotationType::BUILTIN_STRING:
            return makecheck<PrimitiveType>(typeinfo_tag::STRING);
        case TypeAnnotationType::BUILTIN_UBYTE:
        case TypeAnnotationType::BUILTIN_U8:
            return makecheck<PrimitiveType>(typeinfo_tag::U8);
        case TypeAnnotationType::BUILTIN_BYTE:
        case TypeAnnotationType::BUILTIN_I8:
            return makecheck<PrimitiveType>(typeinfo_tag::I8);
        case TypeAnnotationType::BUILTIN_USHORT:
        case TypeAnnotationType::BUILTIN_U16:
            return makecheck<PrimitiveType>(typeinfo_tag::U16);
        case TypeAnnotationType::BUILTIN_SHORT:
        case TypeAnnotationType::BUILTIN_I16:
            return makecheck<PrimitiveType>(typeinfo_tag::I16);
        case TypeAnnotationType::BUILTIN_UINT:
        case TypeAnnotationType::BUILTIN_U32:
            return makecheck<PrimitiveType>(typeinfo_tag::U32);
        case TypeAnnotationType::BUILTIN_INT:
        case TypeAnnotationType::BUILTIN_I32:
            return makecheck<PrimitiveType>(typeinfo_tag::I32);
        case TypeAnnotationType::BUILTIN_UPTR:
        case TypeAnnotationType::BUILTIN_ULONG:
        case TypeAnnotationType::BUILTIN_U64:
            return makecheck<PrimitiveType>(typeinfo_tag::U64);
        case TypeAnnotationType::BUILTIN_IPTR:
        case TypeAnnotationType::BUILTIN_LONG:
        case TypeAnnotationType::BUILTIN_I64:
            return makecheck<PrimitiveType>(typeinfo_tag::I64);
        case TypeAnnotationType::BUILTIN_HALF:
        case TypeAnnotationType::BUILTIN_F16:
            return makecheck<PrimitiveType>(typeinfo_tag::F16);
        case TypeAnnotationType::BUILTIN_FLOAT:
        case TypeAnnotationType::BUILTIN_F32:
            return makecheck<PrimitiveType>(typeinfo_tag::F32);
        case TypeAnnotationType::BUILTIN_DOUBLE:
        case TypeAnnotationType::BUILTIN_F64:
            return makecheck<PrimitiveType>(typeinfo_tag::F64);
        case TypeAnnotationType::BUILTIN_QUADRUPLE:
        case TypeAnnotationType::BUILTIN_F128:
            return makecheck<PrimitiveType>(typeinfo_tag::F128);
        default:
            return {};
        }
    }

    auto PrimitiveType::from(const Str &primitive_type) -> CheckingRef<PrimitiveType>
    {
        if (primitive_type == "unit")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::UNIT);
        }
        if (primitive_type == "bool")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::BOOL);
        }
        if (primitive_type == "i8" || primitive_type == "byte")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::I8);
        }
        if (primitive_type == "i16" || primitive_type == "short")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::I16);
        }
        if (primitive_type == "i32" || primitive_type == "int")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::I32);
        }
        if (primitive_type == "i64" || primitive_type == "long" || primitive_type == "iptr")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::I64);
        }
        if (primitive_type == "i128")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::I128);
        }
        if (primitive_type == "u8" || primitive_type == "ubyte")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::U8);
        }
        if (primitive_type == "u16" || primitive_type == "ushort")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::U16);
        }
        if (primitive_type == "u32" || primitive_type == "uint")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::U32);
        }
        if (primitive_type == "u64" || primitive_type == "ulong" || primitive_type == "uptr")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::U64);
        }
        if (primitive_type == "u128")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::U128);
        }
        if (primitive_type == "f16" || primitive_type == "half")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::F16);
        }
        if (primitive_type == "f32" || primitive_type == "float")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::F32);
        }
        if (primitive_type == "f64" || primitive_type == "double")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::F64);
        }
        if (primitive_type == "f128" || primitive_type == "quadruple")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::F128);
        }
        if (primitive_type == "f256")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::F256);
        }
        if (primitive_type == "string")
        {
            return makecheck<PrimitiveType>(typeinfo_tag::STRING);
        }
        return {};
    }

    auto PrimitiveType::primitive() const -> typeinfo_tag
    {
        return type;
    }
    auto PrimitiveType::tag() const -> typeinfo_tag
    {
        return type;
    }
    auto PrimitiveType::repr() const -> Str
    {
        switch (type)
        {
        case typeinfo_tag::UNIT:
            return "unit";
        case typeinfo_tag::BOOL:
            return "bool";
        case typeinfo_tag::I8:
            return "i8";
        case typeinfo_tag::I16:
            return "i16";
        case typeinfo_tag::I32:
            return "i32";
        case typeinfo_tag::I64:
            return "i64";
        case typeinfo_tag::I128:
            return "i128";
        case typeinfo_tag::U8:
            return "u8";
        case typeinfo_tag::U16:
            return "u16";
        case typeinfo_tag::U32:
            return "u32";
        case typeinfo_tag::U64:
            return "u64";
        case typeinfo_tag::U128:
            return "u128";
        case typeinfo_tag::F16:
            return "f16";
        case typeinfo_tag::F32:
            return "f32";
        case typeinfo_tag::F64:
            return "f64";
        case typeinfo_tag::F128:
            return "f128";
        case typeinfo_tag::F256:
            return "f256";
        case typeinfo_tag::STRING:
            return "string";
        default:
            throw TypeCheckingException("Invalid primitive type");
        }
    }
    auto PrimitiveType::match(const TypeInfo &other) const -> bool
    {
        const PrimitiveType &otherPrimitive = static_cast<const PrimitiveType &>(other);
        if (this->type == otherPrimitive.type)
        {
            return true;
        }
        auto this_tag = code(this->type);
        auto other_tag = code(otherPrimitive.type);
        auto this_category = (this_tag & 0xF0);
        auto other_category = (other_tag & 0xF0);
        if (this_category == other_category)
        {
            if (this_tag > code(typeinfo_tag::SIGNED) && this_tag < code(typeinfo_tag::STRING))
            {
                return other_tag <= this_tag;
            }
        }
        else if (this_category == code(typeinfo_tag::SIGNED) && other_category == code(typeinfo_tag::UNSIGNED))
        {
            return (this_tag & 0x0F) > (other_tag & 0x0F);
        }
        return false;
    }
}