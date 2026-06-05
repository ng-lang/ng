#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>
#include <fwd.hpp>

namespace NG::typecheck
{
    using ast::ASTRef;

    // ── Generic parameter utilities (extracted from TypeChecker) ────────

    inline auto genericParamNameSet(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Set<Str>
    {
        Set<Str> names;
        for (auto &param : genericParams) names.insert(param->name);
        return names;
    }

    inline auto typeParamBoundName(const ast::GenericParam &param) -> Str
    {
        return param.bound ? param.bound->repr() : "";
    }

    inline auto genericParamKindArities(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Vec<size_t>
    {
        Vec<size_t> arities;
        arities.reserve(genericParams.size());
        for (auto &param : genericParams) arities.push_back(param->kindArity);
        return arities;
    }

    inline auto genericParamKindVariadicTails(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Vec<bool>
    {
        Vec<bool> tails;
        tails.reserve(genericParams.size());
        for (auto &param : genericParams) tails.push_back(param->kindVariadicTail);
        return tails;
    }

    inline auto genericParamIsConst(const Vec<ASTRef<ast::GenericParam>> &genericParams) -> Vec<bool>
    {
        Vec<bool> flags;
        flags.reserve(genericParams.size());
        for (auto &param : genericParams) flags.push_back(param->isConst);
        return flags;
    }

    // ── Type instance name utilities ────────────────────────────────────

    inline auto stripTypeInstanceSuffix(const Str &typeName) -> Str
    {
        auto lt = typeName.find('<');
        return lt == Str::npos ? typeName : typeName.substr(0, lt);
    }

    auto parseTypeInstanceArgs(const Str &name) -> Vec<Str>;

    // ── Overload resolution ─────────────────────────────────────────────

    auto functionPatternSpecificity(const ast::FunctionDef &candidate) -> size_t;
} // namespace NG::typecheck
