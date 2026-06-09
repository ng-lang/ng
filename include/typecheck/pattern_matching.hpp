#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    // Check if a type annotation is a pack pattern (e.g., "T...")
    inline auto isPackTypePattern(const ast::TypeAnnotation *pattern, const Set<Str> &genericParamNames) -> bool
    {
        return pattern && pattern->genericArgs.empty() && pattern->arguments.empty() &&
               pattern->name.size() > 3 && pattern->name.ends_with("...") &&
               genericParamNames.contains(pattern->name.substr(0, pattern->name.size() - 3));
    }

    inline auto packPatternName(const ast::TypeAnnotation *pattern) -> Str
    {
        if (!pattern || pattern->name.size() < 3 || !pattern->name.ends_with("..."))
        {
            return {};
        }
        return pattern->name.substr(0, pattern->name.size() - 3);
    }

    auto bindPackPattern(const ast::TypeAnnotation *pattern,
                         const Vec<CheckingRef<TypeInfo>> &actualTypes,
                         Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;

    auto typePatternMatch(const ast::TypeAnnotation *pattern, const CheckingRef<TypeInfo> &actual,
                          const Set<Str> &genericParamNames,
                          Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;

    auto typePatternArgListMatches(const Vec<std::shared_ptr<ast::TypeAnnotation>> &patterns,
                                   const Vec<CheckingRef<TypeInfo>> &actuals,
                                   const Set<Str> &genericParamNames,
                                   Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;

    // ── Specialization matching ─────────────────────────────────────────

    /// Check if a type alias specialization matches the given type arguments.
    auto typeSpecializationMatches(const ast::TypeAliasDef &specialization,
                                   const Vec<CheckingRef<TypeInfo>> &typeArgs,
                                   Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;

    /// Check if a const specialization matches the given type arguments.
    auto constSpecializationMatches(const ast::ConstDef &specialization,
                                    const Vec<CheckingRef<TypeInfo>> &typeArgs,
                                    Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool;

    // ── Pattern specificity ─────────────────────────────────────────────

    /// Check if a type annotation is a generic pattern wildcard.
    auto isGenericPatternWildcard(const ast::TypeAnnotation *annotation, const Set<Str> &genericParamNames) -> bool;

    /// Get child type annotations for specificity calculation.
    auto typePatternChildren(const ast::TypeAnnotation *annotation) -> Vec<const ast::TypeAnnotation *>;

    /// Compute specificity score for a type pattern.
    auto typePatternSpecificity(const ast::TypeAnnotation *annotation, const Set<Str> &genericParamNames) -> size_t;
} // namespace NG::typecheck
