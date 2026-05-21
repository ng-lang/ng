# Auto Traits And Derive Traits

## Goal

Add compiler-assisted trait implementation mechanisms for common marker and structural traits.

Example direction:

```ng
type Point: derive(Clone + Debug) {
  property x: i32;
  property y: i32;
}

auto trait Send;
```

## Scope

In scope:

- Syntax and semantics for `derive(...)`.
- Syntax and semantics for `auto trait`.
- Built-in derive support for a small initial set, likely `Clone`, `Copy`, and `Debug`/`Show` if available.
- Structural eligibility checks.
- Interaction with explicit impls and coherence.
- Diagnostics explaining why derive/auto trait application failed.

Out of scope:

- Procedural macros.
- User-defined derive expansion hooks.
- Associated types.

## Acceptance Criteria

- Deriving `Clone` generates a valid `Clone` impl when all fields are cloneable.
- Deriving `Copy` only succeeds when all fields are copyable and no `Drop` conflict exists.
- Explicit impls conflict deterministically with derived impls.
- Auto traits propagate structurally through fields.
- Negative or blocked auto-trait behavior is specified before implementation.
