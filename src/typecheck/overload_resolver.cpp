
#include <typecheck/overload_resolver.hpp>
#include <typecheck/typecheck_utils.hpp>
#include <typecheck/pattern_matching.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    // Forward declarations
    auto typeSatisfiesTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                            const Map<Str, Vec<Str>> &trait_impls_by_type,
                            const Set<Str> &activeAutoTraits,
                            const Set<Str> &activeDerivedTraitImplKeys,
                            const TypeEnvironment &env) -> bool;
    auto isObjectSafeTrait(const TraitType &trait) -> bool;

    auto typeMatches(const TypeInfo &expected, const TypeInfo &actual,
                     const Map<Str, Vec<Str>> &trait_impls_by_type,
                     const Set<Str> &activeAutoTraits,
                     const Set<Str> &activeDerivedTraitImplKeys,
                     const TypeEnvironment &env) -> bool
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
                                  activeAutoTraits, activeDerivedTraitImplKeys, env);
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

    // ── Generic binding extraction ──────────────────────────────────────

    namespace
    {
        auto genericTypeConstructorFixedArity(const GenericTypeDef &genericType) -> size_t
        {
            auto packIt = std::find(genericType.typeParamIsPack.begin(), genericType.typeParamIsPack.end(), true);
            if (packIt == genericType.typeParamIsPack.end()) return genericType.typeParamNames.size();
            return static_cast<size_t>(std::distance(genericType.typeParamIsPack.begin(), packIt));
        }

        auto genericTypeConstructorVariadicTail(const GenericTypeDef &genericType) -> bool
        {
            return std::any_of(genericType.typeParamIsPack.begin(), genericType.typeParamIsPack.end(),
                               [](bool isPack) { return isPack; });
        }

        auto typeKindArity(const CheckingRef<TypeInfo> &type) -> size_t
        {
            if (!type) return 0;
            if (type->tag() == typeinfo_tag::GENERIC_PARAM)
                return static_cast<const GenericParamType &>(*type).kindArity;
            if (type->tag() == typeinfo_tag::GENERIC_TYPE_DEF)
                return genericTypeConstructorFixedArity(static_cast<const GenericTypeDef &>(*type));
            return 0;
        }

        auto typeKindVariadicTail(const CheckingRef<TypeInfo> &type) -> bool
        {
            if (!type) return false;
            if (type->tag() == typeinfo_tag::GENERIC_PARAM)
                return static_cast<const GenericParamType &>(*type).kindVariadicTail;
            if (type->tag() == typeinfo_tag::GENERIC_TYPE_DEF)
                return genericTypeConstructorVariadicTail(static_cast<const GenericTypeDef &>(*type));
            return false;
        }
    }

    void extractGenericBindingsImpl(CheckingRef<TypeInfo> paramType, CheckingRef<TypeInfo> argType,
                                    Map<Str, CheckingRef<TypeInfo>> &substitution, Set<uintptr_t> &seen,
                                    const TypeEnvironment &env)
    {
        if (!paramType || !argType) return;
        auto key = reinterpret_cast<uintptr_t>(paramType.get()) ^ (reinterpret_cast<uintptr_t>(argType.get()) << 1U);
        if (!seen.insert(key).second) return;

        paramType = unwrap(paramType);
        argType = unwrap(argType);

        if (auto paramAlias = std::dynamic_pointer_cast<TypeAliasType>(paramType))
        {
            extractGenericBindingsImpl(paramAlias->underlyingType, argType, substitution, seen, env);
            return;
        }
        if (auto argAlias = std::dynamic_pointer_cast<TypeAliasType>(argType))
        {
            extractGenericBindingsImpl(paramType, argAlias->underlyingType, substitution, seen, env);
            return;
        }

        if (paramType->tag() == typeinfo_tag::CONST_VALUE)
        {
            auto &constParam = static_cast<ConstValueType &>(*paramType);
            if (!constParam.isParam) return;
            if (auto existing = substitution.contains(constParam.value) ? substitution[constParam.value] : nullptr)
            {
                if (existing->tag() == typeinfo_tag::CONST_VALUE)
                {
                    auto existingConst = std::static_pointer_cast<ConstValueType>(existing);
                    if (existingConst->isParam)
                        substitution[constParam.value] = argType;
                    else if (!existing->match(*argType))
                        throw TypeCheckingException("Inconsistent bindings for const generic parameter '" +
                                                    constParam.value + "': " + existing->repr() + " vs " + argType->repr());
                }
            }
            else
            {
                substitution[constParam.value] = argType;
            }
            return;
        }

        if (paramType->tag() == typeinfo_tag::GENERIC_PARAM)
        {
            auto &gp = static_cast<GenericParamType &>(*paramType);
            if (auto existing = substitution.contains(gp.name) ? substitution[gp.name] : nullptr)
            {
                if (existing->tag() == typeinfo_tag::GENERIC_PARAM)
                    substitution[gp.name] = argType;
                else if (argType && argType->tag() != typeinfo_tag::GENERIC_PARAM && !typeMatch(*existing, *argType))
                    throw TypeCheckingException("Inconsistent bindings for generic parameter '" + gp.name +
                                                "': " + existing->repr() + " vs " + argType->repr());
            }
            else
            {
                substitution[gp.name] = argType;
            }
            return;
        }

        if (paramType->tag() == typeinfo_tag::ARRAY && argType->tag() == typeinfo_tag::ARRAY)
        {
            auto &p = static_cast<ArrayType &>(*paramType);
            auto &a = static_cast<ArrayType &>(*argType);
            extractGenericBindingsImpl(p.elementType, a.elementType, substitution, seen, env);
            if (p.length && a.length) extractGenericBindingsImpl(p.length, a.length, substitution, seen, env);
            return;
        }
        if (paramType->tag() == typeinfo_tag::VECTOR && argType->tag() == typeinfo_tag::VECTOR)
        {
            extractGenericBindingsImpl(static_cast<VectorType &>(*paramType).elementType,
                                       static_cast<VectorType &>(*argType).elementType, substitution, seen, env);
            return;
        }
        if (paramType->tag() == typeinfo_tag::SPAN && argType->tag() == typeinfo_tag::SPAN)
        {
            extractGenericBindingsImpl(static_cast<SpanType &>(*paramType).elementType,
                                       static_cast<SpanType &>(*argType).elementType, substitution, seen, env);
            return;
        }
        if (paramType->tag() == typeinfo_tag::TUPLE && argType->tag() == typeinfo_tag::TUPLE)
        {
            auto &p = static_cast<TupleType &>(*paramType);
            auto &a = static_cast<TupleType &>(*argType);
            for (size_t i = 0; i < p.elementTypes.size() && i < a.elementTypes.size(); ++i)
                extractGenericBindingsImpl(p.elementTypes[i], a.elementTypes[i], substitution, seen, env);
            return;
        }
        if (paramType->tag() == typeinfo_tag::REFERENCE && argType->tag() == typeinfo_tag::REFERENCE)
        {
            extractGenericBindingsImpl(static_cast<ReferenceType &>(*paramType).referencedType,
                                       static_cast<ReferenceType &>(*argType).referencedType, substitution, seen, env);
            return;
        }
        if (paramType->tag() == typeinfo_tag::TYPE_CONSTRUCTOR_APPLICATION)
        {
            auto &paramApp = static_cast<TypeConstructorApplicationType &>(*paramType);
            Str argBase;
            Vec<Str> argArgs;
            if (argType)
            {
                Str name;
                switch (argType->tag())
                {
                case typeinfo_tag::CUSTOMIZED:   name = static_cast<const CustomizedType &>(*argType).name; break;
                case typeinfo_tag::TYPE_ALIAS:    name = static_cast<const TypeAliasType &>(*argType).name; break;
                case typeinfo_tag::NEW_TYPE:      name = static_cast<const NewTypeType &>(*argType).name; break;
                case typeinfo_tag::TAGGED_UNION:  name = static_cast<const TaggedUnionType &>(*argType).name; break;
                default: break;
                }
                if (!name.empty()) { argBase = stripTypeInstanceSuffix(name); argArgs = parseTypeInstanceArgs(name); }
            }
            if (argBase.empty() || argArgs.size() != paramApp.typeArgs.size()) return;
            if (auto cp = std::dynamic_pointer_cast<GenericParamType>(paramApp.constructorType))
            {
                if (auto it = env.locals.find(argBase); it != env.locals.end())
                {
                    if (typeKindArity(it->second) == cp->kindArity &&
                        typeKindVariadicTail(it->second) == cp->kindVariadicTail)
                    {
                        auto existing = substitution.contains(cp->name) ? substitution[cp->name] : nullptr;
                        if (!existing || existing->tag() == typeinfo_tag::GENERIC_PARAM)
                            substitution[cp->name] = it->second;
                    }
                }
            }
            for (size_t i = 0; i < paramApp.typeArgs.size() && i < argArgs.size(); ++i)
            {
                CheckingRef<TypeInfo> argConcrete;
                if (auto it = env.locals.find(argArgs[i]); it != env.locals.end()) argConcrete = it->second;
                else if (auto pr = PrimitiveType::from(argArgs[i])) argConcrete = pr;
                else argConcrete = makecheck<CustomizedType>(argArgs[i]);
                extractGenericBindingsImpl(paramApp.typeArgs[i], argConcrete, substitution, seen, env);
            }
            return;
        }
        if (paramType->tag() == typeinfo_tag::CUSTOMIZED && argType->tag() == typeinfo_tag::CUSTOMIZED)
        {
            auto &pc = static_cast<CustomizedType &>(*paramType);
            auto &ac = static_cast<CustomizedType &>(*argType);
            if (stripTypeInstanceSuffix(pc.name) != stripTypeInstanceSuffix(ac.name)) return;
            auto pArgs = parseTypeInstanceArgs(pc.name);
            auto aArgs = parseTypeInstanceArgs(ac.name);
            for (size_t i = 0; i < pArgs.size() && i < aArgs.size(); ++i)
            {
                auto pIt = substitution.find(pArgs[i]);
                CheckingRef<TypeInfo> aConcrete;
                if (auto aIt = env.locals.find(aArgs[i]); aIt != env.locals.end()) aConcrete = aIt->second;
                else aConcrete = PrimitiveType::from(aArgs[i]);
                if (pIt != substitution.end() && aConcrete)
                    extractGenericBindingsImpl(pIt->second, aConcrete, substitution, seen, env);
            }
            return;
        }
        if (paramType->tag() == typeinfo_tag::TAGGED_UNION)
        {
            auto &pu = static_cast<TaggedUnionType &>(*paramType);
            if (argType->tag() == typeinfo_tag::TAGGED_UNION)
            {
                auto &au = static_cast<TaggedUnionType &>(*argType);
                for (const auto &[vn, pp] : pu.variants)
                {
                    if (!au.variants.contains(vn)) continue;
                    const auto &ap = au.variants.at(vn);
                    for (size_t i = 0; i < pp.size() && i < ap.size(); ++i)
                        extractGenericBindingsImpl(pp[i], ap[i], substitution, seen, env);
                }
                return;
            }
            if (argType->tag() == typeinfo_tag::VARIANT)
            {
                auto &av = static_cast<VariantType &>(*argType);
                if (pu.variants.contains(av.variantName))
                {
                    const auto &pp = pu.variants.at(av.variantName);
                    for (size_t i = 0; i < pp.size() && i < av.payloadTypes.size(); ++i)
                        extractGenericBindingsImpl(pp[i], av.payloadTypes[i], substitution, seen, env);
                }
                return;
            }
        }
        if (paramType->tag() == typeinfo_tag::VARARGS)
        {
            auto &pv = static_cast<VarargsType &>(*paramType);
            if (argType->tag() == typeinfo_tag::VARARGS)
            {
                auto &av = static_cast<VarargsType &>(*argType);
                for (size_t i = 0; i < pv.elementTypes.size() && i < av.elementTypes.size(); ++i)
                    extractGenericBindingsImpl(pv.elementTypes[i], av.elementTypes[i], substitution, seen, env);
            }
            else if (argType->tag() == typeinfo_tag::TUPLE)
            {
                auto &at = static_cast<TupleType &>(*argType);
                for (size_t i = 0; i < pv.elementTypes.size() && i < at.elementTypes.size(); ++i)
                    extractGenericBindingsImpl(pv.elementTypes[i], at.elementTypes[i], substitution, seen, env);
            }
        }
    }
} // namespace NG::typecheck
