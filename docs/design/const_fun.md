# `const fun` And Compile-Time Computation

## Goal

Introduce compile-time executable functions for use in `const` definitions, `where` predicates, const generic arguments, and `const if`.

Example direction:

```ng
const fun is_power_of_two(value: u32): bool {
  ...
}

fun foo<const N: u32>() where is_power_of_two(N) {
  ...
}
```

## Scope

In scope:

- `const fun` declaration syntax.
- A restricted compile-time evaluator.
- Deterministic, side-effect-free execution.
- Scalar return values initially: `bool`, integers, and `string` if needed.
- Calls from const definitions, `where` predicates, and `const if`.
- Recursion and loop limits with explicit diagnostics.

Out of scope:

- Runtime reflection.
- IO, native side effects, allocation-heavy compile-time execution.
- Type-level associated types.

## Acceptance Criteria

- `const fun` can compute bool/integer scalar values.
- `where const_fun<T>()` or `where const_fun(N)` evaluates during type checking.
- Non-terminating or over-limit compile-time execution reports a deterministic error.
- Non-const functions cannot be called from const contexts.
- Compile-time evaluator has tests for branching, loops, recursion limit, and invalid side effects.
