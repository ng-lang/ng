# Constant Generic Parameters

## Order

Recommended Issue order: 3.

## Goal

Add scalar compile-time values as generic parameters, separate from type parameters.

Example direction:

```ng
type Array<T, const N: u32> = native;

fun repeat<const N: u32>(value: i32) -> Array<i32, N> = native;
```

Current surface direction:

```ng
val fixed: array<i32, 4> = [1, 2, 3, 4];
val dynamic: vector<i32> = [1, 2, 3];

fun view(xs: span<i32>) -> u32 = native;
```

## Dependencies

Prerequisites:

- Existing enhanced generics, specialization, and mangling.
- [Generalized `= delete` Declarations](generalized_delete.md) is recommended first so invalid const-generic specializations can use one consistent negative declaration mechanism.

Unblocks:

- [`const fun` And Compile-Time Computation](const_fun.md)
- [Enhanced Tuple Types](enhanced_tuples.md), for `tuple_element<T, I>` and tuple slice bounds.
- Stable `.ngo` symbol persistence in [Bytecode Module Loading](bytecode_module_loading.md).

## Scope

In scope:

- Syntax for const generic parameters.
- Supported scalar types: initially `bool`, signed/unsigned integers, and possibly `string` only if needed.
- Type checker representation for const generic arguments.
- Monomorphization keys and name mangling including const values.
- Validation that const generic arguments are compile-time constants.
- Use in type aliases, native opaque types, functions, and constraints.
- Fixed-size `array<T, N>` type annotation backed by const generic `N`.
- Dynamic owning `vector<T>` type annotation.
- Non-owning contiguous `span<T>` type annotation for native/runtime APIs.

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

## Array, Vector, And Span Model

The collection type split is:

- `array<T, N>` is a fixed-size owning value with compile-time length `N`.
- `vector<T>` is the dynamic owning contiguous container and replaces the previous dynamic `array<T>` meaning.
- `span<T>` is a non-owning contiguous view. It can be used in type signatures before the runtime exposes general extraction/conversion APIs.

Compatibility migration:

- `[T]` type annotation is treated as `vector<T>`.
- Legacy suffix syntax `T array` is treated as `vector<T>`.
- `array<T>` is rejected; callers must write `array<T, N>` or `vector<T>`.
- Array literals remain runtime array cells today, but type-check as `vector<T>` unless an expected fixed `array<T, N>` type is present.

Implemented behavior in this stage:

- Parser accepts `const N: u32` generic parameters and scalar literal generic arguments.
- Type checker represents scalar const generic arguments as `ConstValueType`.
- Generic type/function instantiation validates type-vs-const parameter positions.
- Fixed `array<T, N>` checks array literal length when an expected fixed array type exists.
- Mangling/canonical names include const values as explicit `const<type:value>` segments.

Deferred runtime/API work:

- Runtime storage currently continues to use existing array cells for both fixed array literals and vector literals.
- Dedicated vector/span runtime layouts and conversions are separate follow-up work.
- `span<T>` is type-checkable but construction/extraction APIs still need standard library/runtime design.
