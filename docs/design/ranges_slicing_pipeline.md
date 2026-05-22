# Ranges, Slicing, Fold, And Pipeline Syntax

## Order

Recommended Issue order: 10.

## Goal

Define collection-oriented syntax that was originally discussed alongside tuples but is not a prerequisite for enhanced generics or tuple type introspection.

This design is intentionally last in the current dependency order because it adds surface syntax and collection semantics rather than foundational type/module infrastructure.

## Dependencies

Prerequisites:

- Current tuple and array value semantics.
- [Enhanced Tuple Types](enhanced_tuples.md), if tuple slicing must preserve exact tuple element types.
- [Constant Generic Parameters](constant_generic_parameters.md), for compile-time tuple slice bounds.

Optional:

- [Standard Library Modularization](stdlib_modularization.md), if range/fold helpers live in std modules.

## Scope

In scope:

- Range operator syntax: `a..b`, `a..=b`, `..b`, `a..`.
- From-end index syntax: `^n`.
- Array and tuple indexing with ranges.
- Tuple slicing type preservation for compile-time bounds.
- Collection fold/map/filter syntax, if it survives redesign.
- Pipeline syntax, if it composes cleanly with functions and traits.

Out of scope:

- Tuple value basics.
- Generic pack mechanics.
- Module system changes.

## Current Baseline

Already exists:

- Lexer tokens for `..`, `..=`, `...`, and `^`.
- Prelude functions `range(start, end)` and `slice(array, start, end)`.
- Descending `range` behavior in prelude.
- Clamped array `slice` behavior in prelude.

Not implemented:

- Range expressions as AST/typechecked/runtime values.
- From-end indexing.
- `xs[a..b]` syntax.
- Tuple slicing preserving tuple type shape.
- Fold/map/filter postfix syntax.
- Pipeline syntax.

## Design Questions

- Are ranges first-class values or only index syntax?
- Are open ranges allowed outside indexing?
- Does `a..b` mean end-exclusive and `a..=b` mean end-inclusive?
- Should descending ranges be represented by `range(start, end)` semantics or an explicit step?
- Should fold/map/filter syntax lower to stdlib functions or trait methods?
- Should pipeline syntax be general function composition or collection-only sugar?

## Acceptance Criteria

- `range(0, 3)` compatibility remains intact.
- `xs[1..3]` works for arrays.
- `tuple[1..3]` works when bounds are compile-time constants and returns a tuple with preserved element types.
- `xs[^1]` indexes from the end when the collection has runtime length.
- Invalid tuple slice bounds fail during type checking.
- Fold/pipeline syntax has a separate parser/typechecker test suite before runtime lowering.
