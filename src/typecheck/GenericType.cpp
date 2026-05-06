#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{
    // --- GenericParamType ---

    auto GenericParamType::repr() const -> Str
    {
        if (bound.empty())
            return name;
        return name + ": " + bound;
    }

    auto GenericParamType::match(const TypeInfo &other) const -> bool
    {
        // A generic param matches another generic param with the same name
        if (other.tag() == GENERIC_PARAM)
        {
            return name == static_cast<const GenericParamType &>(other).name;
        }
        // A generic param also matches ANY (unconstrained)
        return other.tag() == ANY;
    }

    // --- GenericDefType ---

    auto GenericDefType::repr() const -> Str
    {
        Str result = name + "<";
        for (size_t i = 0; i < typeParamNames.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += typeParamNames[i];
        }
        result += ">";
        return result;
    }

    auto GenericDefType::match(const TypeInfo &other) const -> bool
    {
        if (other.tag() != GENERIC_DEF)
            return false;
        auto &otherGeneric = static_cast<const GenericDefType &>(other);
        return name == otherGeneric.name && typeParamNames == otherGeneric.typeParamNames;
    }

    // --- VarargsType ---

    auto VarargsType::repr() const -> Str
    {
        Str result = "Varargs<";
        for (size_t i = 0; i < elementTypes.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += elementTypes[i]->repr();
        }
        result += ">";
        return result;
    }

    auto VarargsType::match(const TypeInfo &other) const -> bool
    {
        if (other.tag() != VARARGS)
            return false;
        auto &otherVarargs = static_cast<const VarargsType &>(other);
        if (elementTypes.size() != otherVarargs.elementTypes.size())
            return false;
        for (size_t i = 0; i < elementTypes.size(); ++i)
        {
            if (!elementTypes[i]->match(*otherVarargs.elementTypes[i]))
                return false;
        }
        return true;
    }

} // namespace NG::typecheck
