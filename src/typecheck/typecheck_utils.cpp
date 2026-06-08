
#include <typecheck/typecheck_utils.hpp>
#include <typecheck/overload_resolver.hpp>
#include <ast.hpp>
#include <token.hpp>
#include <algorithm>

namespace NG::typecheck
{
    // ── Type unwrapping ────────────────────────────────────────────────

    auto unwrap(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>
    {
        if (!type) return type;
        switch (type->tag())
        {
        case typeinfo_tag::PARAM_WITH_DEFAULT_VALUE:
            return unwrap(static_cast<ParamWithDefaultValueType &>(*type).paramType);
        case typeinfo_tag::TYPE_ALIAS:
            return unwrap(static_cast<TypeAliasType &>(*type).underlyingType);
        default:
            return type;
        }
    }

    // ── Type matching ──────────────────────────────────────────────────

    auto typeMatch(const TypeInfo &a, const TypeInfo &b) -> bool
    {
        const auto &ua = unwrapAlias(a);
        const auto &ub = unwrapAlias(b);

        // Allow unit literal to match any custom (struct) type — acts like null
        auto isUnit = [](const TypeInfo &t) {
            if (auto p = dynamic_cast<const PrimitiveType *>(&t))
                return p->tag() == typeinfo_tag::UNIT;
            return false;
        };
        if (isUnit(ua) && dynamic_cast<const CustomizedType *>(&ub)) return true;
        if (isUnit(ub) && dynamic_cast<const CustomizedType *>(&ua)) return true;

        // Allow CustomizedType to match through ReferenceType
        if (auto custom = dynamic_cast<const CustomizedType *>(&ua))
        {
            if (auto ref = dynamic_cast<const ReferenceType *>(&ub); ref && ref->referencedType)
                return custom->match(*ref->referencedType);
        }
        if (auto custom = dynamic_cast<const CustomizedType *>(&ub))
        {
            if (auto ref = dynamic_cast<const ReferenceType *>(&ua); ref && ref->referencedType)
                return custom->match(*ref->referencedType);
        }

        return ua.match(ub);
    }

    // ── Type display and lookup helpers ────────────────────────────────

    auto findTaggedVariant(const Map<Str, CheckingRef<TypeInfo>> &locals, const Str &variantName)
        -> std::optional<TaggedVariantLookup>
    {
        for (const auto &[_, type] : locals)
        {
            auto unionType = std::dynamic_pointer_cast<TaggedUnionType>(type);
            if (!unionType || !unionType->variants.contains(variantName))
            {
                continue;
            }

            Vec<Str> payloadNames;
            if (unionType->variantPayloadNames.contains(variantName))
            {
                payloadNames = unionType->variantPayloadNames.at(variantName);
            }

            return TaggedVariantLookup{
                .unionType = unionType,
                .payloadTypes = unionType->variants.at(variantName),
                .payloadNames = payloadNames,
            };
        }

        return std::nullopt;
    }

    auto widenVariantToUnionType(const Map<Str, CheckingRef<TypeInfo>> &locals, CheckingRef<TypeInfo> type)
        -> CheckingRef<TypeInfo>
    {
        auto unwrapped = unwrap(type);
        auto variantType = std::dynamic_pointer_cast<VariantType>(unwrapped);
        if (!variantType)
        {
            return type;
        }

        auto it = locals.find(variantType->unionName);
        if (it != locals.end() && it->second && it->second->tag() == typeinfo_tag::TAGGED_UNION)
        {
            return it->second;
        }

        return type;
    }

    auto formatTypeInstanceName(const Str &baseName, const Vec<CheckingRef<TypeInfo>> &args) -> Str
    {
        auto safeTypeName = [](const CheckingRef<TypeInfo> &type, const auto &self) -> Str {
            if (!type) return "?";
            switch (type->tag())
            {
            case typeinfo_tag::TAGGED_UNION: return static_cast<const TaggedUnionType &>(*type).name;
            case typeinfo_tag::VARIANT:
            {
                const auto &variant = static_cast<const VariantType &>(*type);
                return variant.unionName + "." + variant.variantName;
            }
            case typeinfo_tag::CUSTOMIZED: return static_cast<const CustomizedType &>(*type).name;
            case typeinfo_tag::TYPE_ALIAS: return static_cast<const TypeAliasType &>(*type).name;
            case typeinfo_tag::NEW_TYPE:   return static_cast<const NewTypeType &>(*type).name;
            case typeinfo_tag::REFERENCE:
                return "ref<" + self(static_cast<const ReferenceType &>(*type).referencedType, self) + ">";
            case typeinfo_tag::ARRAY:
            {
                const auto &array = static_cast<const ArrayType &>(*type);
                if (array.length) return "array<" + self(array.elementType, self) + ", " + self(array.length, self) + ">";
                return "array<" + self(array.elementType, self) + ", ?>";
            }
            case typeinfo_tag::VECTOR:
                return "vector<" + self(static_cast<const VectorType &>(*type).elementType, self) + ">";
            case typeinfo_tag::SPAN:
                return "span<" + self(static_cast<const SpanType &>(*type).elementType, self) + ">";
            case typeinfo_tag::RANGE:
                return "Range<" + self(static_cast<const RangeType &>(*type).elementType, self) + ">";
            case typeinfo_tag::TUPLE:
            {
                const auto &tuple = static_cast<const TupleType &>(*type);
                Str out = "(";
                for (size_t i = 0; i < tuple.elementTypes.size(); ++i)
                {
                    if (i > 0) out += ", ";
                    out += self(tuple.elementTypes[i], self);
                }
                return out + ")";
            }
            default: return type->repr();
            }
        };

        Str result = baseName + "<";
        for (size_t i = 0; i < args.size(); ++i)
        {
            if (i > 0) result += ", ";
            result += safeTypeName(args[i], safeTypeName);
        }
        result += ">";
        return result;
    }

    auto typeKindName(const TypeInfo &type) -> Str
    {
        switch (type.tag())
        {
        case typeinfo_tag::BOOL:          return "bool";
        case typeinfo_tag::STRING:        return "string";
        case typeinfo_tag::ARRAY:         return "array";
        case typeinfo_tag::VECTOR:        return "vector";
        case typeinfo_tag::SPAN:          return "span";
        case typeinfo_tag::TUPLE:         return "tuple";
        case typeinfo_tag::FUNCTION:      return "function";
        case typeinfo_tag::CUSTOMIZED:    return "object";
        case typeinfo_tag::TYPE_ALIAS:    return "alias";
        case typeinfo_tag::NEW_TYPE:      return "newtype";
        case typeinfo_tag::TAGGED_UNION:  return "tagged_union";
        case typeinfo_tag::VARIANT:       return "variant";
        case typeinfo_tag::UNION:         return "union";
        case typeinfo_tag::GENERIC_PARAM: return "generic_param";
        default:
            if (isPrimitive(type.tag())) return "primitive";
            return "type";
        }
    }

    // ── Sequence type utilities ─────────────────────────────────────────

    auto builtin_sequence_element_type(const CheckingRef<TypeInfo> &type) -> CheckingRef<TypeInfo>
    {
        auto unwrapped = unwrap(type);
        if (!unwrapped) return nullptr;
        switch (unwrapped->tag())
        {
        case typeinfo_tag::ARRAY:  return static_cast<const ArrayType &>(*unwrapped).elementType;
        case typeinfo_tag::VECTOR: return static_cast<const VectorType &>(*unwrapped).elementType;
        case typeinfo_tag::SPAN:   return static_cast<const SpanType &>(*unwrapped).elementType;
        case typeinfo_tag::RANGE:  return static_cast<const RangeType &>(*unwrapped).elementType;
        case typeinfo_tag::VARARGS:
        {
            const auto &varargs = static_cast<const VarargsType &>(*unwrapped);
            if (varargs.elementTypes.empty()) return nullptr;
            auto elementType = varargs.elementTypes.front();
            for (auto &nextType : varargs.elementTypes)
            {
                if (!typeMatch(*elementType, *nextType)) return makecheck<Untyped>();
            }
            return elementType;
        }
        default: return nullptr;
        }
    }

    // ── Const value utilities ───────────────────────────────────────────

    auto const_value_equals_size(const CheckingRef<TypeInfo> &type, size_t value) -> bool
    {
        auto constValue = std::dynamic_pointer_cast<ConstValueType>(unwrap(type));
        if (!constValue || constValue->isParam) return true;
        try { return std::stoull(constValue->value) == value; }
        catch (...) { return false; }
    }

    // ── Self type predicates ────────────────────────────────────────────

    auto is_self_type(const CheckingRef<TypeInfo> &type) -> bool
    {
        auto generic = std::dynamic_pointer_cast<GenericParamType>(unwrap(type));
        return generic && generic->name == "Self";
    }

    auto is_ref_self_type(const CheckingRef<TypeInfo> &type) -> bool
    {
        auto ref = std::dynamic_pointer_cast<ReferenceType>(unwrap(type));
        return ref && is_self_type(ref->referencedType);
    }

    auto contains_non_receiver_self(const CheckingRef<TypeInfo> &type) -> bool
    {
        auto unwrapped = unwrap(type);
        if (!unwrapped) return false;
        if (is_self_type(unwrapped)) return true;
        switch (unwrapped->tag())
        {
        case typeinfo_tag::REFERENCE:
            return contains_non_receiver_self(static_cast<const ReferenceType &>(*unwrapped).referencedType);
        case typeinfo_tag::ARRAY:
            return contains_non_receiver_self(static_cast<const ArrayType &>(*unwrapped).elementType);
        case typeinfo_tag::VECTOR:
            return contains_non_receiver_self(static_cast<const VectorType &>(*unwrapped).elementType);
        case typeinfo_tag::SPAN:
            return contains_non_receiver_self(static_cast<const SpanType &>(*unwrapped).elementType);
        case typeinfo_tag::TUPLE:
            return std::ranges::any_of(static_cast<const TupleType &>(*unwrapped).elementTypes,
                                       contains_non_receiver_self);
        case typeinfo_tag::UNION:
            return std::ranges::any_of(static_cast<const UnionType &>(*unwrapped).types,
                                       contains_non_receiver_self);
        default: return false;
        }
    }

    // ── Expression predicates ───────────────────────────────────────────

    auto isReferenceableExpression(const ast::Expression *expr) -> bool
    {
        return dynamic_cast<const ast::IdExpression *>(expr) != nullptr ||
               dynamic_cast<const ast::IdAccessorExpression *>(expr) != nullptr ||
               dynamic_cast<const ast::IndexAccessorExpression *>(expr) != nullptr ||
               (dynamic_cast<const ast::UnaryExpression *>(expr) != nullptr &&
                dynamic_cast<const ast::UnaryExpression *>(expr)->optr != nullptr &&
                dynamic_cast<const ast::UnaryExpression *>(expr)->optr->type == TokenType::TIMES);
    }

    auto isMovableExpression(const ast::Expression *expr) -> bool
    {
        if (dynamic_cast<const ast::IdExpression *>(expr) != nullptr) return true;
        if (dynamic_cast<const ast::IdAccessorExpression *>(expr) != nullptr) return true;
        if (dynamic_cast<const ast::IndexAccessorExpression *>(expr) != nullptr) return true;
        auto unaryExpr = dynamic_cast<const ast::UnaryExpression *>(expr);
        return unaryExpr != nullptr && unaryExpr->optr != nullptr && unaryExpr->optr->type == TokenType::TIMES;
    }

    // ── Place key computation ───────────────────────────────────────────

    auto staticPlaceKey(const ast::Expression *expr) -> std::optional<Str>
    {
        if (auto id = dynamic_cast<const ast::IdExpression *>(expr)) return id->id;
        if (auto idAcc = dynamic_cast<const ast::IdAccessorExpression *>(expr))
        {
            if (!idAcc->arguments.empty()) return std::nullopt;
            auto primary = staticPlaceKey(idAcc->primaryExpression.get());
            if (!primary.has_value()) return std::nullopt;
            return *primary + "." + idAcc->accessor->repr();
        }
        if (auto index = dynamic_cast<const ast::IndexAccessorExpression *>(expr))
        {
            auto primary = staticPlaceKey(index->primary.get());
            if (!primary.has_value()) return std::nullopt;
            if (auto intLit = dynamic_cast<const ast::IntegralValue<int32_t> *>(index->accessor.get()))
                return *primary + "[" + std::to_string(intLit->value) + "]";
            return std::nullopt;
        }
        if (auto index = dynamic_cast<const ast::IndexAssignmentExpression *>(expr))
        {
            auto primary = staticPlaceKey(index->primary.get());
            if (!primary.has_value()) return std::nullopt;
            if (auto intLit = dynamic_cast<const ast::IntegralValue<int32_t> *>(index->accessor.get()))
                return *primary + "[" + std::to_string(intLit->value) + "]";
            return std::nullopt;
        }
        return std::nullopt;
    }

    auto relativeReceiverPlace(const ast::Expression *expr, const Str &receiverName) -> std::optional<Str>
    {
        auto place = staticPlaceKey(expr);
        if (!place.has_value() || movedPlaceRoot(*place) != receiverName) return std::nullopt;
        if (*place == receiverName) return Str{};
        auto relative = place->substr(receiverName.size());
        if (!relative.empty() && (relative.front() == '.' || relative.front() == '['))
            relative.erase(relative.begin());
        return relative;
    }

    auto absoluteReceiverPlace(const Str &receiverPlace, const Str &relativePlace) -> Str
    {
        if (relativePlace.empty()) return receiverPlace;
        if (relativePlace.front() == '[') return receiverPlace + relativePlace;
        return receiverPlace + "." + relativePlace;
    }

    auto borrowedPlaceFromRefExpression(const ast::Expression *expr) -> std::optional<Str>
    {
        auto unary = dynamic_cast<const ast::UnaryExpression *>(expr);
        if (!unary || !unary->optr ||
            (unary->optr->type != TokenType::KEYWORD_REF && unary->optr->type != TokenType::AMPERSAND))
            return std::nullopt;
        return staticPlaceKey(unary->operand.get());
    }

    // ── Move/borrow tracking utilities ──────────────────────────────────

    auto movedPlaceRoot(const Str &place) -> Str
    {
        auto borrowSeparator = place.find("->");
        if (place.starts_with("$borrow:") && borrowSeparator != Str::npos)
            return place.substr(std::string_view{"$borrow:"}.size(),
                                borrowSeparator - std::string_view{"$borrow:"}.size());
        auto dot = place.find('.');
        auto bracket = place.find('[');
        auto end = std::min(dot == Str::npos ? place.size() : dot,
                            bracket == Str::npos ? place.size() : bracket);
        return place.substr(0, end);
    }

    auto isBorrowEntry(const Str &entry) -> bool { return entry.starts_with("$borrow:"); }

    auto borrowedAliasName(const Str &entry) -> Str
    {
        if (!isBorrowEntry(entry)) return {};
        auto start = std::string_view{"$borrow:"}.size();
        auto separator = entry.find("->", start);
        return separator == Str::npos ? Str{} : entry.substr(start, separator - start);
    }

    auto borrowedTargetPlace(const Str &entry) -> Str
    {
        if (!isBorrowEntry(entry)) return {};
        auto separator = entry.find("->");
        return separator == Str::npos ? Str{} : entry.substr(separator + 2);
    }

    auto borrowEntry(Str alias, Str place) -> Str
    {
        return "$borrow:" + std::move(alias) + "->" + std::move(place);
    }

    auto isMovedDescendantOf(const Str &moved, const Str &place) -> bool
    {
        if (isBorrowEntry(moved)) return false;
        return moved.size() > place.size() && moved.starts_with(place) &&
               (moved[place.size()] == '.' || moved[place.size()] == '[');
    }

    auto previousPlaceComponent(const Str &place) -> std::optional<Str>
    {
        auto dot = place.rfind('.');
        auto bracket = place.rfind('[');
        if (dot == Str::npos && bracket == Str::npos) return std::nullopt;
        auto pos = std::max(dot == Str::npos ? size_t{0} : dot,
                            bracket == Str::npos ? size_t{0} : bracket);
        return place.substr(0, pos);
    }

    auto movedAncestorOrSelf(const Set<Str> &moved, const Str &place) -> std::optional<Str>
    {
        auto current = std::optional<Str>{place};
        while (current.has_value() && !current->empty())
        {
            if (moved.contains(*current)) return current;
            current = previousPlaceComponent(*current);
        }
        return std::nullopt;
    }

    auto hasMovedDescendant(const Set<Str> &moved, const Str &place) -> bool
    {
        return std::ranges::any_of(moved, [&](const auto &entry) { return isMovedDescendantOf(entry, place); });
    }

    auto placesConflict(const Str &lhs, const Str &rhs) -> bool
    {
        return lhs == rhs || isMovedDescendantOf(lhs, rhs) || isMovedDescendantOf(rhs, lhs);
    }

    auto borrowedConflict(const Set<Str> &state, const Str &place) -> std::optional<Str>
    {
        for (const auto &entry : state)
        {
            if (!isBorrowEntry(entry)) continue;
            auto target = borrowedTargetPlace(entry);
            if (!target.empty() && placesConflict(target, place)) return target;
        }
        return std::nullopt;
    }

    auto borrowedAliasTarget(const Set<Str> &state, const Str &alias) -> std::optional<Str>
    {
        for (const auto &entry : state)
        {
            if (isBorrowEntry(entry) && borrowedAliasName(entry) == alias)
                return borrowedTargetPlace(entry);
        }
        return std::nullopt;
    }

    void clearMovedPlace(Set<Str> &moved, const Str &place)
    {
        for (auto it = moved.begin(); it != moved.end();)
        {
            if (*it == place || isMovedDescendantOf(*it, place) ||
                (isBorrowEntry(*it) && borrowedAliasName(*it) == place))
                it = moved.erase(it);
            else
                ++it;
        }
    }

    // ── Scope utilities ─────────────────────────────────────────────────

    auto scopeNames(const Map<Str, CheckingRef<TypeInfo>> &scope) -> Set<Str>
    {
        Set<Str> names;
        for (const auto &[name, _] : scope) names.insert(name);
        return names;
    }

    auto filterMovedBindings(const Set<Str> &moved, const Set<Str> &allowed) -> Set<Str>
    {
        Set<Str> filtered;
        for (const auto &name : moved)
        {
            if (isBorrowEntry(name))
            {
                if (allowed.contains(borrowedAliasName(name))) filtered.insert(name);
            }
            else if (allowed.contains(name) || allowed.contains(movedPlaceRoot(name)))
            {
                filtered.insert(name);
            }
        }
        return filtered;
    }

} // namespace NG::typecheck
