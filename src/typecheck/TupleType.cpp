#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{

    auto TupleType::tag() const -> typeinfo_tag
    {
        return typeinfo_tag::TUPLE;
    }

    auto TupleType::repr() const -> Str
    {
        Str result = "(";
        for (size_t i = 0; i < elementTypes.size(); ++i)
        {
            if (i > 0)
            {
                result += ", ";
            }
            result += elementTypes[i]->repr();
        }
        result += ")";
        return result;
    }

    auto TupleType::match(const TypeInfo &other) const -> bool
    {
        if (other.tag() != typeinfo_tag::TUPLE)
        {
            return false;
        }

        const auto &otherTuple = static_cast<const TupleType &>(other);
        if (elementTypes.size() != otherTuple.elementTypes.size())
        {
            return false;
        }

        for (size_t i = 0; i < elementTypes.size(); ++i)
        {
            if (!elementTypes[i]->match(*otherTuple.elementTypes[i]))
            {
                return false;
            }
        }

        return true;
    }
}