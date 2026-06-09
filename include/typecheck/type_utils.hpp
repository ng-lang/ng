#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    // ── Generic parameter utilities ─────────────────────────────────────

    inline auto genericParamNameSet(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Set<Str>
    {
        Set<Str> names;
        for (auto &param : genericParams)
        {
            names.insert(param->name);
        }
        return names;
    }

    inline auto typeParamBoundName(const ast::GenericParam &param) -> Str
    {
        return param.bound ? param.bound->repr() : "";
    }

    inline auto genericParamKindArities(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Vec<size_t>
    {
        Vec<size_t> kindArities;
        kindArities.reserve(genericParams.size());
        for (auto &param : genericParams)
        {
            kindArities.push_back(param->kindArity);
        }
        return kindArities;
    }

    inline auto genericParamKindVariadicTails(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Vec<bool>
    {
        Vec<bool> tails;
        tails.reserve(genericParams.size());
        for (auto &param : genericParams)
        {
            tails.push_back(param->kindVariadicTail);
        }
        return tails;
    }

    inline auto genericParamIsConst(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Vec<bool>
    {
        Vec<bool> flags;
        flags.reserve(genericParams.size());
        for (auto &param : genericParams)
        {
            flags.push_back(param->isConst);
        }
        return flags;
    }

    // ── Generic type constructor utilities ──────────────────────────────

    inline auto genericTypeConstructorFixedArity(const GenericTypeDef &genericType) -> size_t
    {
        auto packIt = std::find(genericType.typeParamIsPack.begin(), genericType.typeParamIsPack.end(), true);
        if (packIt == genericType.typeParamIsPack.end())
        {
            return genericType.typeParamNames.size();
        }
        return static_cast<size_t>(std::distance(genericType.typeParamIsPack.begin(), packIt));
    }

    inline auto genericTypeConstructorVariadicTail(const GenericTypeDef &genericType) -> bool
    {
        return std::any_of(genericType.typeParamIsPack.begin(), genericType.typeParamIsPack.end(), [](bool isPack) {
            return isPack;
        });
    }

    // ── Type instance name utilities ────────────────────────────────────

    inline auto stripTypeInstanceSuffix(const Str &typeName) -> Str
    {
        auto lt = typeName.find('<');
        return lt == Str::npos ? typeName : typeName.substr(0, lt);
    }

    inline auto parseTypeInstanceArgs(const Str &name) -> Vec<Str>
    {
        auto lt = name.find('<');
        if (lt == Str::npos || !name.ends_with('>'))
        {
            return {};
        }
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

    // ── Overload resolution ─────────────────────────────────────────────

    // Forward declarations for functions defined in typecheck.cpp
    auto typeMatches(const TypeInfo &a, const TypeInfo &b) -> bool;
    auto typeSatisfiesTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                            const Map<Str, Vec<ast::FunctionDef *>> &activeConstFunctions = {}) -> bool;

    inline auto functionApplyWithCoercions(const FunctionType &funcType,
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

} // namespace NG::typecheck
