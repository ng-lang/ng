#pragma once

#include <typecheck/typeinfo.hpp>
#include <ast.hpp>
#include <fwd.hpp>

namespace NG::typecheck
{
    using ast::ASTRef;

    // ── Type tag predicates ─────────────────────────────────────────────

    constexpr inline bool isIntegralType(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return c >= code(typeinfo_tag::SIGNED) && c < code(typeinfo_tag::FLOATING_POINT);
    }

    constexpr inline bool isSigned(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return (c & 0xF0) == code(typeinfo_tag::SIGNED);
    }

    constexpr inline bool isPrimitive(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return c >= code(typeinfo_tag::PRIMITIVES) && c < code(typeinfo_tag::COLLECTION_TYPE);
    }

    constexpr inline bool isFloatingPoint(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return (c & 0xF0) == code(typeinfo_tag::FLOATING_POINT);
    }

    // ── Type unwrapping ────────────────────────────────────────────────

    /// Unwrap TypeAliasType and ParamWithDefaultValueType to get the underlying type.
    auto unwrap(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>;

    /// Unwrap TypeAliasType to get the underlying concrete type (const ref version).
    inline const TypeInfo &unwrapAlias(const TypeInfo &t)
    {
        if (t.tag() == typeinfo_tag::TYPE_ALIAS)
            return unwrapAlias(*static_cast<const TypeAliasType &>(t).underlyingType);
        return t;
    }

    /// Dereference a reference type to get the referenced type.
    inline auto deref_reference_type(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>
    {
        auto unwrapped = unwrap(type);
        if (unwrapped && unwrapped->tag() == typeinfo_tag::REFERENCE)
            return static_cast<ReferenceType &>(*unwrapped).referencedType;
        return type;
    }

    // ── Type matching ──────────────────────────────────────────────────

    /// Match types with alias transparency (including unit-to-customized coercion).
    auto typeMatch(const TypeInfo &a, const TypeInfo &b) -> bool;

    // ── Sequence type utilities ─────────────────────────────────────────

    /// Get the element type of a builtin sequence type (array, vector, span, range, varargs).
    auto builtin_sequence_element_type(const CheckingRef<TypeInfo> &type) -> CheckingRef<TypeInfo>;

    /// Check if a type is a builtin sequence type.
    inline auto is_builtin_sequence_type(const CheckingRef<TypeInfo> &type) -> bool
    {
        return builtin_sequence_element_type(type) != nullptr;
    }

    // ── Const value utilities ───────────────────────────────────────────

    using ConstValue = std::variant<bool, int64_t, Str>;

    /// Check if a const value equals a given size.
    auto const_value_equals_size(const CheckingRef<TypeInfo> &type, size_t value) -> bool;

    // ── Self type predicates ────────────────────────────────────────────

    /// Check if a type is the Self generic parameter.
    auto is_self_type(const CheckingRef<TypeInfo> &type) -> bool;

    /// Check if a type is ref<Self>.
    auto is_ref_self_type(const CheckingRef<TypeInfo> &type) -> bool;

    /// Check if a type contains Self in a non-receiver position.
    auto contains_non_receiver_self(const CheckingRef<TypeInfo> &type) -> bool;

    // ── Expression predicates ───────────────────────────────────────────

    /// Check if an expression is a valid lvalue for reference creation.
    auto isReferenceableExpression(const ast::Expression *expr) -> bool;

    /// Check if an expression is a valid lvalue for move.
    auto isMovableExpression(const ast::Expression *expr) -> bool;

    // ── Place key computation ───────────────────────────────────────────

    /// Compute a static place key for an expression (e.g., "self.field[0]").
    auto staticPlaceKey(const ast::Expression *expr) -> std::optional<Str>;

    /// Compute a relative receiver place (strip receiver name prefix).
    auto relativeReceiverPlace(const ast::Expression *expr, const Str &receiverName) -> std::optional<Str>;

    /// Combine receiver and relative places.
    auto absoluteReceiverPlace(const Str &receiverPlace, const Str &relativePlace) -> Str;

    /// Extract the place key from a ref/& expression.
    auto borrowedPlaceFromRefExpression(const ast::Expression *expr) -> std::optional<Str>;

    // ── Move/borrow tracking utilities ──────────────────────────────────

    /// Get the root of a moved place (e.g., "self" from "self.field").
    auto movedPlaceRoot(const Str &place) -> Str;

    /// Check if an entry is a borrow entry ($borrow:...).
    auto isBorrowEntry(const Str &entry) -> bool;

    /// Get the alias name from a borrow entry.
    auto borrowedAliasName(const Str &entry) -> Str;

    /// Get the target place from a borrow entry.
    auto borrowedTargetPlace(const Str &entry) -> Str;

    /// Create a borrow entry string.
    auto borrowEntry(Str alias, Str place) -> Str;

    /// Check if a moved place is a descendant of another place.
    auto isMovedDescendantOf(const Str &moved, const Str &place) -> bool;

    /// Get the parent place component (e.g., "self.field" from "self.field[0]").
    auto previousPlaceComponent(const Str &place) -> std::optional<Str>;

    /// Find the nearest moved ancestor or self.
    auto movedAncestorOrSelf(const Set<Str> &moved, const Str &place) -> std::optional<Str>;

    /// Check if any moved place is a descendant of the given place.
    auto hasMovedDescendant(const Set<Str> &moved, const Str &place) -> bool;

    /// Check if two places conflict (one is descendant of the other).
    auto placesConflict(const Str &lhs, const Str &rhs) -> bool;

    /// Find a borrowed place that conflicts with the given place.
    auto borrowedConflict(const Set<Str> &state, const Str &place) -> std::optional<Str>;

    /// Find the target of a borrowed alias.
    auto borrowedAliasTarget(const Set<Str> &state, const Str &alias) -> std::optional<Str>;

    /// Clear a moved place and its descendants from the moved set.
    void clearMovedPlace(Set<Str> &moved, const Str &place);

    // ── Scope utilities ─────────────────────────────────────────────────

    /// Get the set of names in a scope.
    auto scopeNames(const Map<Str, CheckingRef<TypeInfo>> &scope) -> Set<Str>;

    /// Filter moved bindings to only include those in the allowed set.
    auto filterMovedBindings(const Set<Str> &moved, const Set<Str> &allowed) -> Set<Str>;
} // namespace NG::typecheck
