# Auto Traits And Derive Traits

## Order

Recommended Issue order: 7.

## Goal

Add compiler-assisted trait implementation mechanisms for common marker and structural traits.

Example direction:

```ng
type Point: derive(Copy + Clone) {
  x: i32;
  y: i32;
}

auto trait Send;
```

## Dependencies

Prerequisites:

- [Module Artifact And Typechecker Integration](archive/module_artifact_typechecker.md), for cross-module trait impl visibility and coherence.
- [Generalized `= delete` Declarations](archive/generalized_delete.md), for negative or blocked auto-trait behavior.
- Existing trait dispatch, trait bounds, and impl coherence.

Related:

- [Standard Library Modularization](archive/stdlib_modularization.md), if core marker traits live in `std`.

## Scope

In scope:

- Syntax and semantics for `derive(...)`.
- Syntax and semantics for `auto trait`.
- Built-in derive support for a small initial set, initially `Copy` and `Clone`.
- Structural eligibility checks.
- Interaction with explicit impls and coherence.
- Diagnostics explaining why derive/auto trait application failed.

Out of scope:

- Procedural macros.
- User-defined derive expansion hooks.
- Associated types.
- Runtime generation for user-defined derive hooks.

## Semantics

`derive(...)` is explicit implementation evidence for compiler-supported traits.
It is not the same mechanism as an auto trait. A structural type with copyable
fields does not satisfy `T: Copy` unless it declares `derive(Copy)` or has an
explicit `impl Copy for T`.

`auto trait Name;` declares a methodless marker trait whose implementation is
computed structurally. A type satisfies an auto trait when every reachable field
type also satisfies it, unless a future negative impl/delete rule blocks that
propagation.

Built-in scalar values, strings, unit, references, fixed arrays, spans, and
tuples participate structurally. `vector<T>` is intentionally not `Copy` through
derive eligibility because it owns dynamic storage.

`derive(Copy)` conflicts with `Drop`, because copying a value with deterministic
destruction would make ownership ambiguous. Explicit impls and derived impls for
the same `(trait, type)` pair are also deterministic coherence conflicts.

## Phased Implementation Plan

### Phase 1: Parser And Typechecker Evidence

- Implemented: `auto trait Name;` syntax.
- Implemented: parser support for `auto trait` declarations with optional supertraits and method bodies, so the typechecker can reject invalid auto traits with diagnostics.
- Implemented: `type Name: derive(Copy + Clone) { ... }` syntax for structural type definitions.
- Implemented: typechecker validation that auto traits have no generic parameters and no methods.
- Implemented: structural auto trait satisfaction for primitive, reference, tuple, array, vector, span, and structural field graphs.
- Implemented: derive eligibility checks for `Copy` and `Clone`.
- Implemented: derived impl evidence for trait bounds and explicit-impl coherence.
- Implemented: `Drop` conflict checks for derived `Copy`.
- Implemented: STUPID and ORGASM example coverage through `example/55.auto_derive_traits.ng`.
- Implemented: `derive(Clone)` exposes a callable `clone()` method.

### Phase 2: Synthetic Runtime Bodies

- Implemented: a derived `Clone::clone(self: ref<Self>) -> Self` body is synthesized for structural types.
- Implemented: STUPID and ORGASM lower the synthetic method through the same runtime dispatch path.
- Remaining: ensure synthetic clone behavior respects future partial-move initialization state semantics.
- Implemented: tests call a derived `clone()` method directly.

### Phase 3: Debug/Show-Like Derived Traits

- Define the standard trait shape for `Debug` or `Show`.
- Generate deterministic field-order formatting.
- Decide whether formatting traits are standard-library traits, compiler-known traits, or both.
- Add examples covering nested structural types and tuples.

### Phase 4: Negative Auto Trait Controls

- Integrate generalized `= delete` or negative impl rules.
- Allow a type to block auto trait propagation intentionally.
- Add diagnostics that identify the field path that prevents propagation.

### Phase 5: Module Artifact Export

- Persist auto trait definitions and derived impl evidence into module artifacts.
- Ensure imported modules expose the same trait satisfaction results as source modules.
- Reject cross-module coherence conflicts deterministically.

## Acceptance Criteria

- `auto trait Send;` parses and a structural type with only sendable fields satisfies `T: Send`.
- `type Point: derive(Copy + Clone) { x: i32; y: i32; }` parses and satisfies `T: Copy` and `T: Clone`.
- `derive(Clone)` synthesizes a callable `clone(self: ref<Self>) -> Self` method in both STUPID and ORGASM.
- A structural type without `derive(Copy)` or explicit `impl Copy` does not satisfy `T: Copy`.
- `derive(Copy)` rejects fields that cannot be copied, such as `vector<T>`.
- Explicit impls conflict deterministically with derived impls.
- `Drop` conflicts deterministically with derived `Copy`.
- Invalid auto traits with generic parameters or methods fail during type checking.
- Negative or blocked auto-trait behavior is specified before implementation.
