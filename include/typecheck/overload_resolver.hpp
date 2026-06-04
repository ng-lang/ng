#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>
#include <fwd.hpp>

namespace NG::typecheck
{
    // Forward declarations
    struct TraitImplRecord;

    /**
     * @brief Resolves function overloads by matching argument types to candidates.
     *
     * Extracted from TypeChecker to reduce the god class size and enable
     * independent testing of overload resolution logic.
     */
    struct OverloadResolver
    {
        // Check if argument types can be applied to a function type with coercions.
        static auto functionApplyWithCoercions(const FunctionType &funcType,
                                               const Vec<CheckingRef<TypeInfo>> &argumentTypes) -> bool;

        // Check if a function candidate matches the given argument types.
        // Populates bindings with generic parameter substitutions.
        static auto functionCandidateMatches(ast::FunctionDef &candidate,
                                             const Vec<CheckingRef<TypeInfo>> &argumentTypes,
                                             const Set<Str> &genericParamNames,
                                             Map<Str, CheckingRef<TypeInfo>> &bindings,
                                             const Map<Str, Vec<ast::FunctionDef *>> &activeConstFunctions = {}) -> bool;

        // Select the best generic function candidate from a set of overloads.
        static auto selectGenericFunctionCandidate(ast::GenericDefType &genericDef,
                                                   const Vec<CheckingRef<TypeInfo>> &argumentTypes,
                                                   const Map<Str, Vec<ast::FunctionDef *>> &activeConstFunctions = {},
                                                   size_t explicitGenericArgCount = static_cast<size_t>(-1)) -> ast::FunctionDef *;

        // Compute a specificity score for a function candidate (higher = more specific).
        static auto functionPatternSpecificity(const ast::FunctionDef &candidate) -> size_t;

        // Check if where-clause bounds are satisfied for a candidate.
        static auto functionCandidateWhereMatches(const ast::FunctionDef &candidate,
                                                  Map<Str, CheckingRef<TypeInfo>> &bindings,
                                                  const Map<Str, Vec<ast::FunctionDef *>> &activeConstFunctions = {}) -> bool;
    };
} // namespace NG::typecheck
