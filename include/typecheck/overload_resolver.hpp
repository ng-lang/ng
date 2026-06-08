#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>
#include <fwd.hpp>

namespace NG::typecheck
{
    using ast::ASTRef;

    // Forward declarations
    auto typePatternMatch(const ast::TypeAnnotation *pattern, const CheckingRef<TypeInfo> &actual,
                          const Set<Str> &genericParamNames,
                          Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;
    auto typePatternArgListMatches(const Vec<std::shared_ptr<ast::TypeAnnotation>> &patterns,
                                   const Vec<CheckingRef<TypeInfo>> &actuals,
                                   const Set<Str> &genericParamNames,
                                   Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;
    auto bindPackPattern(const ast::TypeAnnotation *pattern,
                         const Vec<CheckingRef<TypeInfo>> &actualTypes,
                         Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;
    auto isPackTypePattern(const ast::TypeAnnotation *pattern, const Set<Str> &genericParamNames) -> bool;

    // ── Generic parameter utilities ─────────────────────────────────────

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

    auto stripTypeInstanceSuffix(const Str &typeName) -> Str;
    auto parseTypeInstanceArgs(const Str &name) -> Vec<Str>;

    // ── Type matching utilities ─────────────────────────────────────────

    /// Match types with alias transparency.
    auto typeMatch(const TypeInfo &a, const TypeInfo &b) -> bool;

    /// Check if expected type matches actual type (including ref-trait coercion).
    auto typeMatches(const TypeInfo &expected, const TypeInfo &actual,
                     const Map<Str, Vec<Str>> &trait_impls_by_type,
                     const Set<Str> &activeAutoTraits,
                     const Set<Str> &activeDerivedTraitImplKeys,
                     const Map<Str, CheckingRef<TypeInfo>> &locals) -> bool;

    // ── Generic binding extraction ──────────────────────────────────────

    /// Extract generic parameter bindings by matching parameter type against argument type.
    /// Pure function — no TypeChecker state required.
    void extractGenericBindingsImpl(CheckingRef<TypeInfo> paramType, CheckingRef<TypeInfo> argType,
                                    Map<Str, CheckingRef<TypeInfo>> &substitution, Set<uintptr_t> &seen,
                                    const Map<Str, CheckingRef<TypeInfo>> &locals);

    // ── Overload resolution utilities ───────────────────────────────────

    auto functionPatternSpecificity(const ast::FunctionDef &candidate) -> size_t;
} // namespace NG::typecheck
