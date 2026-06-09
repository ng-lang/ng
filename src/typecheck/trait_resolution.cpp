
#include <typecheck/trait_resolution.hpp>
#include <typecheck/typecheck.hpp>
#include <ast.hpp>
#include <algorithm>

namespace NG::typecheck
{
    // Forward declarations for functions defined in typecheck.cpp
    auto unwrap(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>;
    auto is_self_type(const CheckingRef<TypeInfo> &type) -> bool;
    auto is_ref_self_type(const CheckingRef<TypeInfo> &type) -> bool;

    namespace
    {
        constexpr inline bool isPrimitive(typeinfo_tag tag) noexcept
        {
            auto c = code(tag);
            return c >= code(typeinfo_tag::PRIMITIVES) && c < code(typeinfo_tag::COLLECTION_TYPE);
        }
    }

    static constexpr const char *COPY_TRAIT_NAME = "Copy";
    static constexpr const char *CLONE_TRAIT_NAME = "Clone";

    namespace
    {
        auto traitImpliesRecursive(const TraitType &candidate, const Str &requiredName,
                                   Set<Str> &seen, const TypeEnvironment &env) -> bool
        {
            if (!seen.insert(candidate.name).second) return false;
            for (auto &superTrait : candidate.superTraits)
            {
                if (!superTrait) continue;
                if (superTrait->name == requiredName) return true;
                auto superIt = env.locals.find(superTrait->name);
                if (superIt != env.locals.end())
                {
                    if (auto st = std::dynamic_pointer_cast<TraitType>(superIt->second))
                    {
                        if (traitImpliesRecursive(*st, requiredName, seen, env)) return true;
                    }
                }
            }
            return false;
        }
    }

    auto traitImplies(const Str &candidateName, const Str &requiredName,
                      const TypeEnvironment &env) -> bool
    {
        if (candidateName == requiredName) return true;
        auto it = env.locals.find(candidateName);
        auto trait = it == env.locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(it->second);
        if (!trait) return false;
        Set<Str> seen;
        return traitImpliesRecursive(*trait, requiredName, seen, env);
    }

    auto typeSatisfiesAutoTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                                const Map<Str, Vec<Str>> &trait_impls_by_type,
                                const TypeEnvironment &env,
                                Set<Str> &seen) -> bool
    {
        auto candidate = unwrap(type);
        if (!candidate) return false;
        if (isPrimitive(candidate->tag()) || candidate->tag() == typeinfo_tag::UNIT ||
            candidate->tag() == typeinfo_tag::BOOL || candidate->tag() == typeinfo_tag::STRING)
        {
            return true;
        }
        switch (candidate->tag())
        {
        case typeinfo_tag::REFERENCE:
            return typeSatisfiesAutoTrait(static_cast<const ReferenceType &>(*candidate).referencedType,
                                          trait, trait_impls_by_type, env, seen);
        case typeinfo_tag::TUPLE:
            return std::ranges::all_of(static_cast<const TupleType &>(*candidate).elementTypes,
                                       [&](const auto &element) {
                                           return typeSatisfiesAutoTrait(element, trait, trait_impls_by_type, env, seen);
                                       });
        case typeinfo_tag::ARRAY:
            return typeSatisfiesAutoTrait(static_cast<const ArrayType &>(*candidate).elementType,
                                          trait, trait_impls_by_type, env, seen);
        case typeinfo_tag::VECTOR:
            return typeSatisfiesAutoTrait(static_cast<const VectorType &>(*candidate).elementType,
                                          trait, trait_impls_by_type, env, seen);
        case typeinfo_tag::SPAN:
            return typeSatisfiesAutoTrait(static_cast<const SpanType &>(*candidate).elementType,
                                          trait, trait_impls_by_type, env, seen);
        default:
            break;
        }
        auto custom = std::dynamic_pointer_cast<CustomizedType>(candidate);
        if (!custom) return false;
        if (auto implIt = trait_impls_by_type.find(custom->name); implIt != trait_impls_by_type.end())
        {
            if (std::ranges::any_of(implIt->second, [&](const Str &implemented) {
                return implemented == trait.name || traitImplies(implemented, trait.name, env);
            }))
            {
                return true;
            }
        }
        if (!seen.insert(custom->name + "::" + trait.name).second) return true;
        for (const auto &[_, fieldType] : custom->properties)
        {
            if (!typeSatisfiesAutoTrait(fieldType, trait, trait_impls_by_type, env, seen))
                return false;
        }
        return true;
    }

    auto typeSatisfiesTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                            const Map<Str, Vec<Str>> &trait_impls_by_type,
                            const Set<Str> &activeAutoTraits,
                            const Set<Str> &activeDerivedTraitImplKeys,
                            const TypeEnvironment &env) -> bool
    {
        if (activeAutoTraits.contains(trait.name))
        {
            Set<Str> seen;
            return typeSatisfiesAutoTrait(type, trait, trait_impls_by_type, env, seen);
        }
        auto candidate = unwrap(type);
        if (!candidate)
        {
            return false;
        }
        if (trait.name == COPY_TRAIT_NAME || trait.name == CLONE_TRAIT_NAME)
        {
            if (isPrimitive(candidate->tag()) || candidate->tag() == typeinfo_tag::UNIT ||
                candidate->tag() == typeinfo_tag::BOOL || candidate->tag() == typeinfo_tag::STRING)
            {
                return true;
            }
            switch (candidate->tag())
            {
            case typeinfo_tag::REFERENCE:
                return trait.name == COPY_TRAIT_NAME ||
                       typeSatisfiesTrait(static_cast<ReferenceType &>(*candidate).referencedType,
                                          trait, trait_impls_by_type, activeAutoTraits,
                                          activeDerivedTraitImplKeys, env);
            case typeinfo_tag::TUPLE:
                return std::ranges::all_of(static_cast<TupleType &>(*candidate).elementTypes,
                                           [&](const auto &element) {
                                               return typeSatisfiesTrait(element, trait, trait_impls_by_type,
                                                                        activeAutoTraits, activeDerivedTraitImplKeys, env);
                                           });
            case typeinfo_tag::ARRAY:
                return typeSatisfiesTrait(static_cast<ArrayType &>(*candidate).elementType,
                                          trait, trait_impls_by_type, activeAutoTraits,
                                          activeDerivedTraitImplKeys, env);
            case typeinfo_tag::SPAN:
                return typeSatisfiesTrait(static_cast<SpanType &>(*candidate).elementType,
                                          trait, trait_impls_by_type, activeAutoTraits,
                                          activeDerivedTraitImplKeys, env);
            case typeinfo_tag::VECTOR:
                return false;
            default:
                break;
            }
        }
        if (candidate->tag() == typeinfo_tag::REFERENCE)
        {
            candidate = unwrap(static_cast<ReferenceType &>(*candidate).referencedType);
        }
        if (candidate && candidate->tag() == typeinfo_tag::GENERIC_PARAM)
        {
            auto &generic = static_cast<GenericParamType &>(*candidate);
            return generic.bound == trait.name || traitImplies(generic.bound, trait.name, env);
        }
        if (!candidate || candidate->tag() != typeinfo_tag::CUSTOMIZED)
        {
            return false;
        }
        auto &custom = static_cast<CustomizedType &>(*candidate);
        if (auto implIt = trait_impls_by_type.find(custom.name); implIt != trait_impls_by_type.end())
        {
            if (std::ranges::any_of(implIt->second, [&](const Str &implemented) {
                return implemented == trait.name || traitImplies(implemented, trait.name, env);
            }))
            {
                return true;
            }
        }
        if (activeDerivedTraitImplKeys.contains(custom.name + "::" + trait.name))
        {
            return true;
        }
        if (trait.name == COPY_TRAIT_NAME || trait.name == CLONE_TRAIT_NAME)
        {
            return false;
        }
        auto &methods = trait.allMethods.empty() ? trait.methods : trait.allMethods;
        for (auto &[methodName, methodType] : methods)
        {
            if (!custom.memberFunctions.contains(methodName) &&
                (!custom.traitMemberFunctions.contains(trait.name) ||
                 !custom.traitMemberFunctions.at(trait.name).contains(methodName)))
            {
                return false;
            }
        }
        return true;
    }

    auto typeCanDeriveTrait(const CheckingRef<TypeInfo> &type, const Str &traitName,
                            const Map<Str, Vec<Str>> &trait_impls_by_type,
                            const TypeEnvironment &env,
                            Set<Str> &seen) -> bool
    {
        auto candidate = unwrap(type);
        if (!candidate) return false;
        if (isPrimitive(candidate->tag()) || candidate->tag() == typeinfo_tag::UNIT ||
            candidate->tag() == typeinfo_tag::BOOL || candidate->tag() == typeinfo_tag::STRING)
        {
            return true;
        }
        switch (candidate->tag())
        {
        case typeinfo_tag::REFERENCE:
            return traitName == COPY_TRAIT_NAME ||
                   typeCanDeriveTrait(static_cast<const ReferenceType &>(*candidate).referencedType,
                                      traitName, trait_impls_by_type, env, seen);
        case typeinfo_tag::TUPLE:
            return std::ranges::all_of(static_cast<const TupleType &>(*candidate).elementTypes,
                                       [&](const auto &element) {
                                           return typeCanDeriveTrait(element, traitName, trait_impls_by_type, env, seen);
                                       });
        case typeinfo_tag::ARRAY:
            return typeCanDeriveTrait(static_cast<const ArrayType &>(*candidate).elementType,
                                      traitName, trait_impls_by_type, env, seen);
        case typeinfo_tag::SPAN:
            return typeCanDeriveTrait(static_cast<const SpanType &>(*candidate).elementType,
                                      traitName, trait_impls_by_type, env, seen);
        default:
            break;
        }
        auto custom = std::dynamic_pointer_cast<CustomizedType>(candidate);
        if (!custom) return false;
        if (auto implIt = trait_impls_by_type.find(custom->name); implIt != trait_impls_by_type.end())
        {
            if (std::ranges::any_of(implIt->second, [&](const Str &implemented) {
                return implemented == traitName || traitImplies(implemented, traitName, env);
            }))
            {
                return true;
            }
        }
        if (!seen.insert(custom->name + "::" + traitName).second) return true;
        for (auto &[methodName, _] : custom->memberFunctions)
        {
            (void)methodName;
        }
        return true;
    }

    auto isObjectSafeTrait(const TraitType &trait) -> bool
    {
        for (auto &[name, type] : trait.methods)
        {
            if (!type) continue;
            auto funcType = std::dynamic_pointer_cast<FunctionType>(type);
            if (!funcType) return false;
            // Check that the first parameter is Self or ref<Self>
            if (funcType->parametersType.empty()) return false;
            if (!is_self_type(funcType->parametersType.front()) && !is_ref_self_type(funcType->parametersType.front()))
            {
                return false;
            }
        }
        return true;
    }
} // namespace NG::typecheck
