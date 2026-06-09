#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>
#include <fwd.hpp>

namespace NG::typecheck
{
    /**
     * @brief Manages trait implementations, auto-trait derivation, and impl lookup.
     *
     * Extracted from TypeChecker to enable independent testing of trait logic
     * and reduce the TypeChecker god class size.
     */
    class TraitRegistry
    {
    public:
        // ── Trait impl records ───────────────────────────────────────────
        struct TraitImplRecord
        {
            Str traitName;
            Str targetPattern;
            Str moduleId;
            Set<Str> genericParamNames;
            Vec<Str> whereBounds;
            Map<Str, Str> methods;
            ast::ImplDef *definition = nullptr;
            TokenPosition pos;
        };

        // ── State ────────────────────────────────────────────────────────
        Map<Str, Vec<Str>> trait_impls_by_type;  // typeName -> [traitName, ...]
        Vec<TraitImplRecord> localTraitImpls;
        Map<Str, Vec<ast::UseImplDecl *>> selectedTraitImpls;
        Set<Str> matchedSelectedTraitImpls;
        Set<Str> autoTraitNames;
        Set<Str> derivedTraitImplKeys;

        // ── Trait registration ───────────────────────────────────────────

        /// Register a trait implementation for a type.
        void registerImpl(const Str &typeName, const Str &traitName)
        {
            auto &impls = trait_impls_by_type[typeName];
            if (std::find(impls.begin(), impls.end(), traitName) == impls.end())
            {
                impls.push_back(traitName);
            }
        }

        /// Register a local trait implementation record.
        void registerLocalImpl(TraitImplRecord record)
        {
            localTraitImpls.push_back(std::move(record));
        }

        /// Register a derived trait impl key (e.g., "Box::Copy").
        void registerDerivedImpl(const Str &typeName, const Str &traitName)
        {
            derivedTraitImplKeys.insert(typeName + "::" + traitName);
        }

        // ── Trait queries ────────────────────────────────────────────────

        /// Check if a type has a specific trait implementation registered.
        [[nodiscard]] auto hasImpl(const Str &typeName, const Str &traitName) const -> bool
        {
            auto it = trait_impls_by_type.find(typeName);
            if (it == trait_impls_by_type.end()) return false;
            return std::find(it->second.begin(), it->second.end(), traitName) != it->second.end();
        }

        /// Check if a derived trait impl exists.
        [[nodiscard]] auto hasDerivedImpl(const Str &typeName, const Str &traitName) const -> bool
        {
            return derivedTraitImplKeys.contains(typeName + "::" + traitName);
        }

        /// Check if a trait name is an auto trait.
        [[nodiscard]] auto isAutoTrait(const Str &traitName) const -> bool
        {
            return autoTraitNames.contains(traitName);
        }

        /// Get all trait implementations for a type.
        [[nodiscard]] auto getImplsForType(const Str &typeName) const -> const Vec<Str> &
        {
            static const Vec<Str> empty{};
            auto it = trait_impls_by_type.find(typeName);
            return it != trait_impls_by_type.end() ? it->second : empty;
        }

        // ── Super-trait resolution ───────────────────────────────────────

        /// Check if candidateName implies requiredName via super-trait chain.
        [[nodiscard]] auto traitImplies(const Str &candidateName, const Str &requiredName,
                                         const Map<Str, CheckingRef<TypeInfo>> &locals) const -> bool
        {
            if (candidateName == requiredName) return true;
            auto it = locals.find(candidateName);
            auto trait = it == locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(it->second);
            if (!trait) return false;
            Set<Str> seen;
            return traitImpliesRecursive(*trait, requiredName, seen, locals);
        }

        // ── Reset ────────────────────────────────────────────────────────

        void clear()
        {
            trait_impls_by_type.clear();
            localTraitImpls.clear();
            selectedTraitImpls.clear();
            matchedSelectedTraitImpls.clear();
            autoTraitNames.clear();
            derivedTraitImplKeys.clear();
        }

    private:
        [[nodiscard]] auto traitImpliesRecursive(const TraitType &candidate, const Str &requiredName,
                                                  Set<Str> &seen,
                                                  const Map<Str, CheckingRef<TypeInfo>> &locals) const -> bool
        {
            if (!seen.insert(candidate.name).second) return false;
            for (auto &superTrait : candidate.superTraits)
            {
                if (!superTrait) continue;
                if (superTrait->name == requiredName) return true;
                auto it = locals.find(superTrait->name);
                if (it != locals.end())
                {
                    if (auto st = std::dynamic_pointer_cast<TraitType>(it->second))
                    {
                        if (traitImpliesRecursive(*st, requiredName, seen, locals)) return true;
                    }
                }
            }
            return false;
        }
    };
} // namespace NG::typecheck
