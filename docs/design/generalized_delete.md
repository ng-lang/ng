# Generalized `= delete` Declarations

## Goal

Extend the current `= delete` support from type-specialization-only usage into a general negative declaration mechanism.

Current support can reject cases such as:

```ng
type<T> ref<ref<T>> = delete;
```

This design should define and implement where deleted declarations are legal, how they participate in overload and specialization resolution, and how diagnostics should point to the deleted declaration.

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
