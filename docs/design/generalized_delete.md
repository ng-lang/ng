# Generalized `= delete` Declarations

## Order

Recommended Issue order: 2.

## Goal

Extend the current `= delete` support from type-specialization-only usage into a general negative declaration mechanism.

Current support can reject cases such as:

```ng
type<T> ref<ref<T>> = delete;
```

This design should define and implement where deleted declarations are legal, how they participate in overload and specialization resolution, and how diagnostics should point to the deleted declaration.

## Dependencies

Prerequisites:

- Existing type alias specialization and overload/specialization resolution.

Unblocks:

- [Enhanced Tuple Types](enhanced_tuples.md), for deleted fallback projections such as invalid `tuple_element`.
- [Auto Traits And Derive Traits](auto_derive_traits.md), for negative or blocked auto-trait behavior.
- Future constraint modeling that needs "matched but forbidden" diagnostics.

## Scope

In scope:

- Deleted type aliases and type specializations.
- Deleted function overloads or generic function specializations.
- Deleted const specializations if useful for constraint modeling.
- Resolution rule: deleted candidates may match, but selecting one is a compile-time error.
- Clear diagnostics showing the deleted declaration that matched.

Out of scope:

- Runtime behavior.
- Trait auto-derive interaction.
- `const fun` execution.

## Acceptance Criteria

- `type<T> X<Bad<T>> = delete;` rejects matching type use.
- `fun<T> foo(value: Bad<T>) = delete;` rejects matching calls.
- A more specific deleted declaration beats a more general valid declaration.
- Non-matching deleted declarations do not affect valid overloads.
- Parser/typechecker tests cover exact match, generic pattern match, and fallback behavior.

## Implementation Status

Implemented:

- Deleted primary and specialized type aliases.
- Deleted generic function overloads selected by argument type patterns.
- Deleted non-generic functions as forbidden callable signatures.
- Deleted const predicate specializations.
- Candidate selection treats deleted declarations as normal candidates first; selecting one is a compile-time error.
- More-specific deleted generic function patterns beat less-specific valid fallbacks.
- Parser and typechecker tests cover exact matches, generic pattern matches, fallback behavior, and const predicates.

Current limitations:

- Function overload selection is intentionally narrow and only models same-name generic candidates needed by deleted declarations.
- Runtime backends skip deleted functions; deleted declarations are expected to be rejected by type checking before execution.
