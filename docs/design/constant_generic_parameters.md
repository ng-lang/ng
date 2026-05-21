# Constant Generic Parameters

## Goal

Add scalar compile-time values as generic parameters, separate from type parameters.

Example direction:

```ng
type Array<T, const N: u32> = native;

fun repeat<const N: u32>(value: i32) -> Array<i32, N> = native;
```

## Scope

In scope:

- Syntax for const generic parameters.
- Supported scalar types: initially `bool`, signed/unsigned integers, and possibly `string` only if needed.
- Type checker representation for const generic arguments.
- Monomorphization keys and name mangling including const values.
- Validation that const generic arguments are compile-time constants.
- Use in type aliases, native opaque types, functions, and constraints.

Out of scope:

- Full `const fun`.
- Arbitrary compile-time expressions beyond already-supported scalar const values.
- Runtime generic metadata in ORGASM.

## Acceptance Criteria

- Parser accepts `fun<const N: u32>()`.
- Parser accepts mixed parameters like `type Buffer<T, const N: u32> = native;`.
- Type checker rejects non-constant values as const generic arguments.
- `Buffer<i32, 4>` and `Buffer<i32, 8>` are distinct instantiated types.
- ORGASM symbols are fully monomorphized and include const generic values in mangling.
