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
} // namespace NG::typecheck
