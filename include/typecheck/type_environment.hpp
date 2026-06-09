#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>
#include <fwd.hpp>

namespace NG::typecheck
{
    /**
     * @brief Manages type-checking scope: variable bindings, move state, imports.
     *
     * Extracted from TypeChecker to enable independent testing of scope logic
     * and reduce the TypeChecker god class size.
     */
    class TypeEnvironment
    {
    public:
        // ── Variable bindings ────────────────────────────────────────────
        Map<Str, CheckingRef<TypeInfo>> locals;

        // ── Move tracking ────────────────────────────────────────────────
        Set<Str> movedBindings;
        bool allowMovedLvalueRead = false;

        // ── Import tracking ──────────────────────────────────────────────
        Set<Str> importedSymbolNames;
        Set<Str> importedImplNames;
        Set<Str> exportedImportNames;
        Map<Str, Str> importAliases;
        Vec<Str> importedModuleIds;

        // ── Constants ────────────────────────────────────────────────────
        static constexpr const char *WILDCARD_IMPORT_KEY = "$$wildcard_import$$";

        TypeEnvironment() = default;
        explicit TypeEnvironment(Map<Str, CheckingRef<TypeInfo>> locals,
                                 Set<Str> movedBindings = {},
                                 bool allowMovedLvalueRead = false)
            : locals(std::move(locals)), movedBindings(std::move(movedBindings)),
              allowMovedLvalueRead(allowMovedLvalueRead) {}

        // ── Scope operations ─────────────────────────────────────────────

        /// Look up a binding by name. Returns nullptr if not found.
        [[nodiscard]] auto lookup(const Str &name) const -> CheckingRef<TypeInfo>
        {
            auto it = locals.find(name);
            return it != locals.end() ? it->second : nullptr;
        }

        /// Add or update a binding.
        void bind(const Str &name, CheckingRef<TypeInfo> type)
        {
            locals[name] = std::move(type);
        }

        /// Check if a binding exists.
        [[nodiscard]] auto has(const Str &name) const -> bool
        {
            return locals.contains(name);
        }

        // ── Move operations ──────────────────────────────────────────────

        /// Mark a binding as moved.
        void markMoved(const Str &name)
        {
            movedBindings.insert(name);
        }

        /// Check if a binding has been moved.
        [[nodiscard]] auto isMoved(const Str &name) const -> bool
        {
            return movedBindings.contains(name);
        }

        /// Check if a binding is usable (exists and not moved, or moved reads allowed).
        [[nodiscard]] auto isUsable(const Str &name) const -> bool
        {
            if (!has(name)) return false;
            if (isMoved(name) && !allowMovedLvalueRead) return false;
            return true;
        }

        // ── Import operations ────────────────────────────────────────────

        /// Check if wildcard imports are active.
        [[nodiscard]] auto hasWildcardImport() const -> bool
        {
            return locals.contains(WILDCARD_IMPORT_KEY);
        }

        /// Enable wildcard imports.
        void enableWildcardImport()
        {
            locals[WILDCARD_IMPORT_KEY] = nullptr;
        }

        /// Register an imported symbol name.
        void registerImportedSymbol(const Str &name)
        {
            importedSymbolNames.insert(name);
        }

        /// Register an import alias.
        void registerImportAlias(const Str &alias, const Str &target)
        {
            importAliases[alias] = target;
        }

        /// Check if a symbol was imported.
        [[nodiscard]] auto isImportedSymbol(const Str &name) const -> bool
        {
            return importedSymbolNames.contains(name);
        }

        // ── Copy for child checkers ──────────────────────────────────────

        /// Create a copy of this environment for a child type checker scope.
        [[nodiscard]] auto childScope(Map<Str, CheckingRef<TypeInfo>> extraLocals = {}) const -> TypeEnvironment
        {
            TypeEnvironment child;
            child.locals = locals;
            child.movedBindings = movedBindings;
            for (const auto &[k, v] : extraLocals)
            {
                child.locals[k] = v;
                child.movedBindings.erase(k);
            }
            child.allowMovedLvalueRead = allowMovedLvalueRead;
            child.importedSymbolNames = importedSymbolNames;
            child.importedImplNames = importedImplNames;
            child.exportedImportNames = exportedImportNames;
            child.importAliases = importAliases;
            child.importedModuleIds = importedModuleIds;
            return child;
        }
    };
} // namespace NG::typecheck
