
#include <typecheck/overload_resolver.hpp>
#include <typecheck/pattern_matching.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    // Forward declarations
    auto unwrap(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>;
    auto typeSatisfiesTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                            const Map<Str, Vec<Str>> &trait_impls_by_type,
                            const Set<Str> &activeAutoTraits,
                            const Set<Str> &activeDerivedTraitImplKeys,
                            const Map<Str, CheckingRef<TypeInfo>> &locals) -> bool;
    auto isObjectSafeTrait(const TraitType &trait) -> bool;

    namespace
    {
        inline const TypeInfo &unwrapAlias(const TypeInfo &t)
        {
            if (t.tag() == typeinfo_tag::TYPE_ALIAS)
                return unwrapAlias(*static_cast<const TypeAliasType &>(t).underlyingType);
            return t;
        }
    }

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

    auto typeMatches(const TypeInfo &expected, const TypeInfo &actual,
                     const Map<Str, Vec<Str>> &trait_impls_by_type,
                     const Set<Str> &activeAutoTraits,
                     const Set<Str> &activeDerivedTraitImplKeys,
                     const Map<Str, CheckingRef<TypeInfo>> &locals) -> bool
    {
        if (typeMatch(expected, actual)) return true;
        // Check ref-trait coercion: ref<Concrete> can match ref<Trait>
        const auto &unwrappedExpected = unwrapAlias(expected);
        const auto &unwrappedActual = unwrapAlias(actual);
        if (unwrappedExpected.tag() != typeinfo_tag::REFERENCE || unwrappedActual.tag() != typeinfo_tag::REFERENCE)
            return false;
        const auto &expectedRef = static_cast<const ReferenceType &>(unwrappedExpected);
        const auto &actualRef = static_cast<const ReferenceType &>(unwrappedActual);
        if (!expectedRef.referencedType || !actualRef.referencedType) return false;
        auto unwrappedRef = unwrap(expectedRef.referencedType);
        if (!unwrappedRef || unwrappedRef->tag() != typeinfo_tag::TRAIT) return false;
        auto trait = std::static_pointer_cast<TraitType>(unwrappedRef);
        if (!isObjectSafeTrait(*trait)) return false;
        return typeSatisfiesTrait(actualRef.referencedType, *trait, trait_impls_by_type,
                                  activeAutoTraits, activeDerivedTraitImplKeys, locals);
    }

    auto stripTypeInstanceSuffix(const Str &typeName) -> Str
    {
        auto lt = typeName.find('<');
        return lt == Str::npos ? typeName : typeName.substr(0, lt);
    }

    auto parseTypeInstanceArgs(const Str &name) -> Vec<Str>
    {
        auto lt = name.find('<');
        if (lt == Str::npos || !name.ends_with('>')) return {};
        auto inner = name.substr(lt + 1, name.size() - lt - 2);
        Vec<Str> args;
        int depth = 0;
        Str current;
        for (char c : inner)
        {
            if (c == '<') depth++;
            else if (c == '>') depth--;
            else if (c == ',' && depth == 0)
            {
                args.push_back(current);
                current.clear();
                continue;
            }
            current += c;
        }
        if (!current.empty()) args.push_back(current);
        return args;
    }

    auto functionPatternSpecificity(const ast::FunctionDef &candidate) -> size_t
    {
        auto genericNames = genericParamNameSet(candidate.genericParams);
        size_t score = 0;
        for (auto &param : candidate.params)
        {
            if (!param || !param->annotatedType) continue;
            auto anno = param->annotatedType.get();
            if (anno->genericArgs.empty() && anno->arguments.empty())
            {
                score += genericNames.contains(anno->name) ? 1 : 10;
            }
            else
            {
                score += 5;
            }
        }
        for (auto &genericParam : candidate.genericParams)
        {
            if (genericParam && genericParam->bound) ++score;
        }
        score += candidate.whereBounds.size();
        return score;
    }
} // namespace NG::typecheck
