
#include <typecheck/overload_resolver.hpp>
#include <typecheck/typecheck.hpp>
#include <typecheck/pattern_matching.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    // Forward declarations for functions defined in typecheck.cpp
    auto typeMatches(const TypeInfo &a, const TypeInfo &b) -> bool;

    auto OverloadResolver::functionApplyWithCoercions(const FunctionType &funcType,
                                                      const Vec<CheckingRef<TypeInfo>> &argumentTypes) -> bool
    {
        auto requiredSize = std::count_if(funcType.parametersType.begin(), funcType.parametersType.end(),
                                          [](const auto &type) {
                                              return type->tag() != typeinfo_tag::PARAM_WITH_DEFAULT_VALUE;
                                          });
        if (argumentTypes.size() < static_cast<size_t>(requiredSize) ||
            argumentTypes.size() > funcType.parametersType.size())
        {
            return false;
        }
        for (size_t i = 0; i < argumentTypes.size(); ++i)
        {
            if (!typeMatches(*funcType.parametersType[i], *argumentTypes[i]))
            {
                return false;
            }
        }
        return true;
    }

    auto OverloadResolver::functionPatternSpecificity(const ast::FunctionDef &candidate) -> size_t
    {
        auto genericNames = genericParamNameSet(candidate.genericParams);
        size_t score = 0;
        for (auto &param : candidate.params)
        {
            if (!param || !param->annotatedType)
            {
                continue;
            }
            auto anno = param->annotatedType.get();
            if (anno->genericArgs.empty() && anno->arguments.empty())
            {
                // Concrete type name — higher specificity
                if (!genericNames.contains(anno->name))
                {
                    score += 10;
                }
                else
                {
                    score += 1; // Generic param — low specificity
                }
            }
            else
            {
                score += 5; // Parameterized type — medium specificity
            }
        }
        return score;
    }

    auto OverloadResolver::functionCandidateMatches(ast::FunctionDef &candidate,
                                                    const Vec<CheckingRef<TypeInfo>> &argumentTypes,
                                                    const Set<Str> &genericParamNames,
                                                    Map<Str, CheckingRef<TypeInfo>> &bindings,
                                                    const Map<Str, Vec<ast::FunctionDef *>> &activeConstFunctions) -> bool
    {
        const bool hasPackTailParam = !candidate.params.empty() &&
                                      candidate.params.back() &&
                                      candidate.params.back()->annotatedType &&
                                      isPackTypePattern(candidate.params.back()->annotatedType.get(), genericParamNames);
        if (!hasPackTailParam && candidate.params.size() != argumentTypes.size())
        {
            return false;
        }
        if (hasPackTailParam && argumentTypes.size() + 1 < candidate.params.size())
        {
            return false;
        }
        for (auto &genericParam : candidate.genericParams)
        {
            if (genericParam)
            {
                if (genericParam->isConst)
                {
                    bindings[genericParam->name] = makecheck<ConstValueType>(
                        genericParam->name, genericParam->constType ? genericParam->constType->repr() : "", true);
                }
                else
                {
                    bindings[genericParam->name] = makecheck<GenericParamType>(
                        genericParam->name, typeParamBoundName(*genericParam), genericParam->isPack,
                        genericParam->kindArity, genericParam->kindVariadicTail);
                }
            }
        }
        const auto fixedCount = hasPackTailParam ? candidate.params.size() - 1 : candidate.params.size();
        for (size_t i = 0; i < fixedCount; ++i)
        {
            if (!candidate.params[i] || !candidate.params[i]->annotatedType)
            {
                continue;
            }
            if (!typePatternMatch(candidate.params[i]->annotatedType.get(), argumentTypes[i], genericParamNames, bindings))
            {
                return false;
            }
        }
        if (hasPackTailParam)
        {
            Vec<CheckingRef<TypeInfo>> packArgs;
            packArgs.insert(packArgs.end(), argumentTypes.begin() + static_cast<std::ptrdiff_t>(fixedCount), argumentTypes.end());
            return bindPackPattern(candidate.params.back()->annotatedType.get(), packArgs, bindings) &&
                   functionCandidateWhereMatches(candidate, bindings, activeConstFunctions);
        }
        return functionCandidateWhereMatches(candidate, bindings, activeConstFunctions);
    }

    auto OverloadResolver::selectGenericFunctionCandidate(ast::GenericDefType &genericDef,
                                                          const Vec<CheckingRef<TypeInfo>> &argumentTypes,
                                                          const Map<Str, Vec<ast::FunctionDef *>> &activeConstFunctions,
                                                          size_t explicitGenericArgCount) -> ast::FunctionDef *
    {
        ast::FunctionDef *best = genericDef.funcDef.get();
        size_t bestScore = 0;
        bool found = false;

        auto evaluate = [&](ast::FunctionDef *candidate) {
            if (!candidate)
            {
                return;
            }
            Map<Str, CheckingRef<TypeInfo>> bindings;
            auto genericNames = genericParamNameSet(candidate->genericParams);
            if (!functionCandidateMatches(*candidate, argumentTypes, genericNames, bindings, activeConstFunctions))
            {
                return;
            }
            auto score = functionPatternSpecificity(*candidate);
            if (!found || score > bestScore)
            {
                best = candidate;
                bestScore = score;
                found = true;
            }
        };

        evaluate(genericDef.funcDef.get());
        for (auto &spec : genericDef.specializations)
        {
            evaluate(spec.get());
        }
        return found ? best : nullptr;
    }

    auto OverloadResolver::functionCandidateWhereMatches(const ast::FunctionDef &candidate,
                                                         Map<Str, CheckingRef<TypeInfo>> &bindings,
                                                         const Map<Str, Vec<ast::FunctionDef *>> &activeConstFunctions) -> bool
    {
        if (candidate.whereBounds.empty())
        {
            return true;
        }
        for (auto &bound : candidate.whereBounds)
        {
            if (!bound)
            {
                return false;
            }
            if (bound->predicate)
            {
                // Evaluate const predicate
                auto subjectIt = bindings.find(bound->predicate->subject);
                if (subjectIt == bindings.end())
                {
                    return false;
                }
                auto constValue = std::dynamic_pointer_cast<ConstValueType>(subjectIt->second);
                if (!constValue)
                {
                    return false;
                }
                auto funcIt = activeConstFunctions.find(bound->predicate->functionName);
                if (funcIt == activeConstFunctions.end())
                {
                    return false;
                }
                bool predicateResult = false;
                for (auto *func : funcIt->second)
                {
                    if (func && func->constExpr && func->body)
                    {
                        // Simplified const predicate evaluation
                        predicateResult = true;
                        break;
                    }
                }
                if (!predicateResult)
                {
                    return false;
                }
                continue;
            }
            if (!bound->subject || !bound->trait || !bound->subject->genericArgs.empty())
            {
                return false;
            }
            auto subIt = bindings.find(bound->subject->name);
            if (subIt == bindings.end())
            {
                return false;
            }
        }
        return true;
    }
} // namespace NG::typecheck
