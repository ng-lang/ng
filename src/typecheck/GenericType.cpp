#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{
    // --- GenericParamType ---

    auto GenericParamType::repr() const -> Str
    {
        Str base = name;
        if (kindArity > 0 || kindVariadicTail)
        {
            base += "<";
            for (size_t i = 0; i < kindArity; ++i)
            {
                if (i > 0)
                    base += ", ";
                base += "_";
            }
            if (kindVariadicTail)
            {
                if (kindArity > 0)
                    base += ", ";
                base += "...";
            }
            base += ">";
        }
        if (bound.empty())
            return base;
        return base + ": " + bound;
    }

    auto GenericParamType::match(const TypeInfo &other) const -> bool
    {
        // A generic param matches another generic param with the same name
        if (other.tag() == GENERIC_PARAM)
        {
            auto &otherGeneric = static_cast<const GenericParamType &>(other);
            return name == otherGeneric.name && kindArity == otherGeneric.kindArity &&
                   kindVariadicTail == otherGeneric.kindVariadicTail;
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

    auto GenericTypeDef::repr() const -> Str
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

    auto GenericTypeDef::match(const TypeInfo &other) const -> bool
    {
        if (other.tag() != GENERIC_TYPE_DEF)
            return false;
        auto &otherGeneric = static_cast<const GenericTypeDef &>(other);
        return name == otherGeneric.name && typeParamNames == otherGeneric.typeParamNames && kind == otherGeneric.kind;
    }

    auto TypeConstructorApplicationType::repr() const -> Str
    {
        Str result = constructorType ? constructorType->repr() : "?";
        result += "<";
        for (size_t i = 0; i < typeArgs.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += typeArgs[i] ? typeArgs[i]->repr() : "?";
        }
        result += ">";
        return result;
    }

    auto TypeConstructorApplicationType::match(const TypeInfo &other) const -> bool
    {
        if (other.tag() != TYPE_CONSTRUCTOR_APPLICATION)
            return false;
        auto &otherApp = static_cast<const TypeConstructorApplicationType &>(other);
        if (!constructorType || !otherApp.constructorType || !constructorType->match(*otherApp.constructorType))
            return false;
        if (typeArgs.size() != otherApp.typeArgs.size())
            return false;
        for (size_t i = 0; i < typeArgs.size(); ++i)
        {
            if (!typeArgs[i] || !otherApp.typeArgs[i] || !typeArgs[i]->match(*otherApp.typeArgs[i]))
                return false;
        }
        return true;
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
