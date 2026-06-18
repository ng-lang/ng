# Type System in Depth

NG's type system combines nominal typing, structural typing, and powerful inference. This chapter explores advanced type system features.

## Type Inference

NG infers types in most contexts without explicit annotations:

```ng
val x = 42;                 // x: i32
val y = 3.14;               // y: f32
val z = [1, 2, 3];          // z: [i32]
```

### Bidirectional Inference

Type information flows both from the expression to the context, and from the context to the expression:

```ng
val arr: [i32] = [];        // [] is typed as empty [i32]
val p: f64 = 3.14;          // 3.14 is typed as f64 (not f32)

fun foo() -> string {
    return "hello";          // return type inferred from annotation
}

fun bar(x: i32) -> i32 => x; // parameter constrained by annotation
```

### Default Numeric Types

Without annotations:
- Integer literals default to `i32`
- Float literals default to `f32`

```ng
val a = 42;                  // a: i32
val b = 3000000000;          // b: i32 (will wrap — use i64 or u64 explicitly)
val c = 42u64;               // c: u64
val d = 3.14;                // d: f32
val e = 3.14f64;             // e: f64
```

## Nominal Types

A **nominal type** is a distinct type identified by its name, not its structure. NG supports two forms:

### Wrapped Types (Newtypes)

```ng
type UserId wraps i32;
type ProductId wraps i32;

val uid: UserId = UserId(1);
val pid: ProductId = ProductId(2);

// uid = pid;  // TYPE ERROR: UserId != ProductId
```

Newtypes are **opaque** — they must be explicitly wrapped and unwrapped. Use `as` to cast:

```ng
val raw: i32 = uid as i32;
```

### Type Aliases

```ng
type Age = i32;     // transparent: Age and i32 are the same type
type Point = (x: i32, y: i32);
```

Type aliases are **transparent** — `Age` and `i32` are interchangeable in all contexts.

## Structural Types

Structural types compare by their shape rather than name:

```ng
type PointA { x: i32; y: i32; }
type PointB { x: i32; y: i32; }

// PointA and PointB are compatible because they have the same structure
```

## Union Types

Union types allow a value to be one of several types:

```ng
type Value = i32 | string | bool;

val v1: Value = 42;
val v2: Value = "hello";

// Runtime type check
const if (v1 is i32) {
    print("v1 is an integer");
}
```

### Untagged Union vs Tagged Union

| Feature | Union Type (`A \| B`) | Tagged Union (`A(x) \| B(y)`) |
|---|---|---|
| Tag | No (runtime check via `is`) | Yes (stored tag) |
| Patterns | `is` operator | `switch` with `case` |
| Exhaustiveness | Not checked | Checked by compiler |
| Payload | The value itself | Named/numbered fields per variant |

## Type Checking with `is`

The `is` operator checks an expression's type at runtime:

```ng
val x: Value = 42;
val isInt = x is i32;       // true
val isStr = x is string;    // false
```

This works with union types and tagged unions.

## Type Queries with `typeof`

The `typeof` operator queries type properties at compile time:

```ng
fun checkType<T>(value: T) {
    const if (is_ref<T>) {
        print("T is a reference type");
    }
}
```

## Type Casting with `as`

Use `as` to convert between related types:

```ng
val x: i32 = 42;
val y: i64 = x as i64;        // numeric widening
val z = UserId(x);            // newtype wrapping
val raw = userId as i32;       // newtype unwrapping
```

## Literal Type Inference

```ng
val a = true;       // bool
val b = false;      // bool
val c = unit;       // unit
val d = "str";      // string
val e = 'str';      // string (single quotes also OK)
```

## Exhaustiveness Checking

The type checker ensures `switch` statements cover all variants of a tagged union:

```ng
type Result = Ok(i32) | Err(string);

switch (result) {
    case Ok(v) { print(v); }
    case Err(m) { print(m); }
    // No "otherwise" needed — all variants covered
}
```

If you omit a variant and don't provide `otherwise`, the compiler reports an error.

## What's Next?

Continue to [Compile-Time Programming](compile-time-programming.md) to explore NG's powerful compile-time evaluation features.

> **Try it:** `example/44.type_specialization.ng` — Type specialization patterns
> **Try it:** `example/48.higher_kinded_generics.ng` — Higher-kinded types