#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>
#include <fwd.hpp>

namespace NG::typecheck
{
    // Forward declarations
    auto unwrap(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>;

    /// Check if a type satisfies a trait via super-trait chain.
    auto traitImplies(const Str &candidateName, const Str &requiredName,
                      const Map<Str, CheckingRef<TypeInfo>> &locals) -> bool;

    /// Check if a type satisfies a trait.
    /// This is the core trait satisfaction check extracted from TypeChecker.
    auto typeSatisfiesTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                            const Map<Str, Vec<Str>> &trait_impls_by_type,
                            const Set<Str> &activeAutoTraits,
                            const Set<Str> &activeDerivedTraitImplKeys,
                            const Map<Str, CheckingRef<TypeInfo>> &locals) -> bool;

    /// Check if a type satisfies an auto trait (structural check).
    auto typeSatisfiesAutoTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                                const Map<Str, Vec<Str>> &trait_impls_by_type,
                                const Map<Str, CheckingRef<TypeInfo>> &locals,
                                Set<Str> &seen) -> bool;

    /// Check if a type can derive a trait.
    auto typeCanDeriveTrait(const CheckingRef<TypeInfo> &type, const Str &traitName,
                            const Map<Str, Vec<Str>> &trait_impls_by_type,
                            const Map<Str, CheckingRef<TypeInfo>> &locals,
                            Set<Str> &seen) -> bool;

    /// Check if a trait is object-safe.
    auto isObjectSafeTrait(const TraitType &trait) -> bool;
} // namespace NG::typecheck
