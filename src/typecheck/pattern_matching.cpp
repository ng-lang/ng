
#include <typecheck/pattern_matching.hpp>
#include <typecheck/overload_resolver.hpp>
#include <typecheck/typecheck.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    // Use shared utilities from overload_resolver
    using NG::typecheck::typeMatch;
    using NG::typecheck::stripTypeInstanceSuffix;
    using NG::typecheck::parseTypeInstanceArgs;

    namespace
    {
        auto unwrap(const CheckingRef<TypeInfo> &type) -> CheckingRef<TypeInfo>
        {
            auto current = type;
            while (current)
            {
                if (auto alias = std::dynamic_pointer_cast<TypeAliasType>(current))
                {
                    current = alias->underlyingType;
                    continue;
                }
                if (auto param = std::dynamic_pointer_cast<ParamWithDefaultValueType>(current))
                {
                    current = param->paramType;
                    continue;
                }
                break;
            }
            return current;
        }
    } // anonymous namespace

    auto bindPackPattern(const ast::TypeAnnotation *pattern,
                         const Vec<CheckingRef<TypeInfo>> &actualTypes,
                         Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
        auto name = packPatternName(pattern);
        auto packed = makecheck<VarargsType>(actualTypes);
        if (auto it = bindings.find(name); it != bindings.end())
        {
            if (it->second && it->second->tag() == typeinfo_tag::GENERIC_PARAM)
            {
                it->second = packed;
                return true;
            }
            return it->second && typeMatch(*it->second, *packed);
        }
        bindings[name] = packed;
        return true;
    }

    auto typePatternMatch(const ast::TypeAnnotation *pattern, const CheckingRef<TypeInfo> &actual,
                          const Set<Str> &genericParamNames,
                          Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
        auto typeFromInstanceArg = [](const Str &name,
                                      const Map<Str, CheckingRef<TypeInfo>> &currentBindings) -> CheckingRef<TypeInfo> {
            if (auto it = currentBindings.find(name); it != currentBindings.end())
            {
                return it->second;
            }
            if (auto primitive = PrimitiveType::from(name))
            {
                return primitive;
            }
            if (name.starts_with("ref<") && name.ends_with(">"))
            {
                auto inner = name.substr(4, name.size() - 5);
                if (auto innerPrimitive = PrimitiveType::from(inner))
                {
                    return makecheck<ReferenceType>(innerPrimitive);
                }
            }
            if (name.contains('<') && name.ends_with(">"))
            {
                return makecheck<CustomizedType>(name);
            }
            return makecheck<CustomizedType>(name);
        };

        if (!pattern || !actual)
        {
            return false;
        }
        if (isPackTypePattern(pattern, genericParamNames))
        {
            return bindPackPattern(pattern, Vec<CheckingRef<TypeInfo>>{actual}, bindings);
        }
        if (pattern->constLiteral)
        {
            auto constValue = std::dynamic_pointer_cast<ConstValueType>(unwrap(actual));
            return constValue && constValue->value == pattern->name;
        }
        if (pattern->type == TypeAnnotationType::TUPLE)
        {
            auto tuple = std::dynamic_pointer_cast<TupleType>(unwrap(actual));
            if (!tuple)
            {
                return false;
            }
            Vec<const ast::TypeAnnotation *> patternElements;
            patternElements.reserve(pattern->arguments.size());
            for (auto &arg : pattern->arguments)
            {
                auto child = dynamic_ast_cast<ast::TypeAnnotation>(arg);
                if (!child)
                {
                    return false;
                }
                patternElements.push_back(child.get());
            }
            const bool hasPackTail = !patternElements.empty() &&
                                     isPackTypePattern(patternElements.back(), genericParamNames);
            if (!hasPackTail && patternElements.size() != tuple->elementTypes.size())
            {
                return false;
            }
            if (hasPackTail && tuple->elementTypes.size() + 1 < patternElements.size())
            {
                return false;
            }
            const auto fixedCount = hasPackTail ? patternElements.size() - 1 : patternElements.size();
            for (size_t i = 0; i < fixedCount; ++i)
            {
                if (!typePatternMatch(patternElements[i], tuple->elementTypes[i], genericParamNames, bindings))
                {
                    return false;
                }
            }
            if (hasPackTail)
            {
                Vec<CheckingRef<TypeInfo>> tailTypes;
                tailTypes.reserve(tuple->elementTypes.size() - fixedCount);
                for (size_t i = fixedCount; i < tuple->elementTypes.size(); ++i)
                {
                    tailTypes.push_back(tuple->elementTypes[i]);
                }
                return bindPackPattern(patternElements.back(), tailTypes, bindings);
            }
            return true;
        }
        if (pattern->type == TypeAnnotationType::UNION)
        {
            for (auto &arg : pattern->arguments)
            {
                auto alternative = dynamic_ast_cast<ast::TypeAnnotation>(arg);
                if (!alternative)
                {
                    continue;
                }
                auto candidateBindings = bindings;
                if (typePatternMatch(alternative.get(), actual, genericParamNames, candidateBindings))
                {
                    bindings = std::move(candidateBindings);
                    return true;
                }
            }
            return false;
        }
        if (genericParamNames.contains(pattern->name) && pattern->genericArgs.empty() && pattern->arguments.empty())
        {
            if (auto it = bindings.find(pattern->name); it != bindings.end())
            {
                if (it->second && it->second->tag() == typeinfo_tag::GENERIC_PARAM)
                {
                    it->second = actual;
                    return true;
                }
                return typeMatch(*it->second, *actual);
            }
            bindings[pattern->name] = actual;
            return true;
        }
        if (pattern->name == "ref")
        {
            auto ref = std::dynamic_pointer_cast<ReferenceType>(unwrap(actual));
            return ref && pattern->genericArgs.size() == 1 &&
                   typePatternMatch(pattern->genericArgs[0].get(), ref->referencedType, genericParamNames, bindings);
        }
        if (pattern->name == "array")
        {
            auto array = std::dynamic_pointer_cast<ArrayType>(unwrap(actual));
            if (!array || pattern->genericArgs.size() != 2)
            {
                return false;
            }
            return typePatternMatch(pattern->genericArgs[0].get(), array->elementType, genericParamNames, bindings) &&
                   array->length &&
                   typePatternMatch(pattern->genericArgs[1].get(), array->length, genericParamNames, bindings);
        }
        if (pattern->name == "vector")
        {
            auto vector = std::dynamic_pointer_cast<VectorType>(unwrap(actual));
            return vector && pattern->genericArgs.size() == 1 &&
                   typePatternMatch(pattern->genericArgs[0].get(), vector->elementType, genericParamNames, bindings);
        }
        if (pattern->name == "span")
        {
            auto span = std::dynamic_pointer_cast<SpanType>(unwrap(actual));
            return span && pattern->genericArgs.size() == 1 &&
                   typePatternMatch(pattern->genericArgs[0].get(), span->elementType, genericParamNames, bindings);
        }
        if (pattern->name == "Range")
        {
            auto range = std::dynamic_pointer_cast<RangeType>(unwrap(actual));
            return range && pattern->genericArgs.size() == 1 &&
                   typePatternMatch(pattern->genericArgs[0].get(), range->elementType, genericParamNames, bindings);
        }
        auto actualName = actual->repr();
        if (auto custom = std::dynamic_pointer_cast<CustomizedType>(unwrap(actual)))
        {
            actualName = stripTypeInstanceSuffix(custom->name);
        }
        else if (auto alias = std::dynamic_pointer_cast<TypeAliasType>(unwrap(actual)))
        {
            actualName = stripTypeInstanceSuffix(alias->name);
        }
        else if (auto tagged = std::dynamic_pointer_cast<TaggedUnionType>(unwrap(actual)))
        {
            actualName = stripTypeInstanceSuffix(tagged->name);
        }
        if (pattern->name != actualName)
        {
            return false;
        }
        auto args = parseTypeInstanceArgs(actual->repr());
        if (auto custom = std::dynamic_pointer_cast<CustomizedType>(unwrap(actual)))
        {
            args = parseTypeInstanceArgs(custom->name);
        }
        else if (auto alias = std::dynamic_pointer_cast<TypeAliasType>(unwrap(actual)))
        {
            args = parseTypeInstanceArgs(alias->name);
        }
        else if (auto tagged = std::dynamic_pointer_cast<TaggedUnionType>(unwrap(actual)))
        {
            args = parseTypeInstanceArgs(tagged->name);
        }
        if (pattern->genericArgs.size() != args.size())
        {
            return pattern->genericArgs.empty();
        }
        Vec<CheckingRef<TypeInfo>> actualArgs;
        actualArgs.reserve(args.size());
        for (auto &arg : args)
        {
            actualArgs.push_back(typeFromInstanceArg(arg, bindings));
        }
        return typePatternArgListMatches(pattern->genericArgs, actualArgs, genericParamNames, bindings);
    }

    auto typePatternArgListMatches(const Vec<std::shared_ptr<ast::TypeAnnotation>> &patterns,
                                   const Vec<CheckingRef<TypeInfo>> &actuals,
                                   const Set<Str> &genericParamNames,
                                   Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
        const bool hasPackTail = !patterns.empty() && isPackTypePattern(patterns.back().get(), genericParamNames);
        if (!hasPackTail && patterns.size() != actuals.size())
        {
            return false;
        }
        if (hasPackTail && actuals.size() + 1 < patterns.size())
        {
            return false;
        }
        const auto fixedCount = hasPackTail ? patterns.size() - 1 : patterns.size();
        for (size_t i = 0; i < fixedCount; ++i)
        {
            if (!typePatternMatch(patterns[i].get(), actuals[i], genericParamNames, bindings))
            {
                return false;
            }
        }
        if (!hasPackTail)
        {
            return true;
        }
        Vec<CheckingRef<TypeInfo>> tailTypes;
        tailTypes.reserve(actuals.size() - fixedCount);
        for (size_t i = fixedCount; i < actuals.size(); ++i)
        {
            tailTypes.push_back(actuals[i]);
        }
        return bindPackPattern(patterns.back().get(), tailTypes, bindings);
    }

    // ── Specialization matching ─────────────────────────────────────────

    auto typeSpecializationMatches(const ast::TypeAliasDef &specialization,
                                   const Vec<CheckingRef<TypeInfo>> &typeArgs,
                                   Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
        if (!specialization.specializationPattern) return false;
        auto genericNames = genericParamNameSet(specialization.genericParams);
        return typePatternArgListMatches(specialization.specializationPattern->genericArgs, typeArgs,
                                         genericNames, bindings);
    }

    auto constSpecializationMatches(const ast::ConstDef &specialization,
                                    const Vec<CheckingRef<TypeInfo>> &typeArgs,
                                    Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
        if (!specialization.specializationPattern) return false;
        auto genericNames = genericParamNameSet(specialization.genericParams);
        return typePatternArgListMatches(specialization.specializationPattern->genericArgs, typeArgs,
                                         genericNames, bindings);
    }

    // ── Pattern specificity ─────────────────────────────────────────────

    auto isGenericPatternWildcard(const ast::TypeAnnotation *annotation, const Set<Str> &genericParamNames) -> bool
    {
        return annotation && annotation->genericArgs.empty() && annotation->arguments.empty() &&
               genericParamNames.contains(annotation->name);
    }

    auto typePatternChildren(const ast::TypeAnnotation *annotation) -> Vec<const ast::TypeAnnotation *>
    {
        Vec<const ast::TypeAnnotation *> children;
        if (!annotation) return children;
        for (auto &arg : annotation->genericArgs) children.push_back(arg.get());
        for (auto &arg : annotation->arguments)
        {
            if (auto child = dynamic_ast_cast<ast::TypeAnnotation>(arg))
                children.push_back(child.get());
        }
        return children;
    }

    auto typePatternSpecificity(const ast::TypeAnnotation *annotation, const Set<Str> &genericParamNames) -> size_t
    {
        if (!annotation || isGenericPatternWildcard(annotation, genericParamNames)) return 0;
        size_t score = 1;
        for (auto *child : typePatternChildren(annotation))
            score += typePatternSpecificity(child, genericParamNames);
        return score;
    }
} // namespace NG::typecheck
