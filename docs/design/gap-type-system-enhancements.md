# Type System Enhancements: GAT, Impl Trait, Never Type, And Borrow Checker

## Order

Recommended implementation order: **11**.

## Goal

Extend NG's type system with advanced features needed for expressive, safe, and composable generic code.

## Motivation

While NG has a capable type system (generics, traits, HKT, const generics), several important features are missing:

| Feature | Purpose | Examples of Need |
|---|---|---|
| GAT (Generic Associated Types) | Associated types that can themselves be generic | `Collection<T>::Iter<'a>`, streaming iterators |
| `impl Trait` | Anonymous types that implement a trait | Return opaque iterator types, avoid naming complex generics |
| Never type `!` / `never` | Type of expressions that never return | `loop { break; }`, `panic`, `exit`, exhaustive `continue` |
| `Self` in non-receiver positions | Return `Self` from trait methods for builder patterns | Builder pattern, fluent APIs |
| Compile-time borrow checking | Replace runtime move-checking with compile-time guarantees | Eliminate use-after-move runtime errors entirely |

## Proposed Features

### 1. Generic Associated Types (GAT)

```ng
trait Collection {
    type Item;
    type Iter<'a>: Iterator where Self: 'a;

    fun iter<'a>(self: ref<'a, Self>) -> Self::Iter<'a>;
    fun len(self: ref<Self>) -> u32;
}

impl Collection for [i32] {
    type Item = i32;
    type Iter<'a> = SliceIter<'a, i32>;

    fun iter<'a>(self: ref<'a, Self>) -> SliceIter<'a, i32> {
        return SliceIter { data: self, index: 0 };
    }
}
```

### 2. `impl Trait` (Return Position)

```ng
// Instead of naming the concrete iterator type:
fun range(start: i32, end: i32) -> impl Iterator<Item = i32> {
    return RangeIter { start: start, end: end, current: start };
}

// In generic code:
fun process(items: impl Iterable) {
    for item in items { ... }
}
```

### 3. Never Type `never`

```ng
// A function that never returns
fun exit(code: i32) -> never {
    nativeExit(code);
}

// Allows exhaustive match on generic types
fun unwrapOrPanic<T>(opt: Option<T>) -> T {
    match (opt) {
        case Some(v) => v;
        case None => exit(1);     // never unifies with any T
    }
}
```

### 4. Compile-Time Borrow Checker

Replace the current **runtime** use-after-move detection with **compile-time** borrowing rules inspired by Rust:

```ng
// Current: runtime check
val x = [1, 2, 3];
val y = move x;
// print(x[0]);        // Runtime error: use after move

// Future: compile-time check
val x = [1, 2, 3];
val y = move x;
print(x[0]);            // Compile error: use of moved value 'x'
```

The borrow checker would enforce:
- At most one mutable reference or any number of immutable references
- References must not outlive their referent
- A value cannot be used after it has been moved
- Partial moves are tracked field-by-field (already exists in type checker)
- A moved field cannot be read until reassigned

## Dependencies

- GAT: requires existing HKT infrastructure (already supports `F<_>`), GAT extends this to associated types.
- `impl Trait`: requires existential type support — related to trait objects.
- `never`: requires the type checker to recognize uninhabited types.
- Borrow checker: requires the existing partial-move tracking to be extended to full borrow analysis.
- Unblocks: safe iteration patterns, builder APIs, zero-cost abstractions.

## Scope

**In scope:**
- GAT syntax and type checking
- `impl Trait` in return position (and later in argument position)
- `never` type definition and unification rules
- Compile-time borrow checker design and implementation
- Migrate from runtime to compile-time move checking

**Out of scope:**
- Lifetime elision (requires full lifetime inference — future work)
- `impl Trait` in trait definitions (assoc type position — future work)
- Polonius-style two-phase borrows (future optimization)
- Thread-safety borrow checking (concurrent borrows — see [Concurrency](gap-concurrency.md))

## Acceptance Criteria

- GAT: a trait with a lifetime-parameterized associated type compiles and can be implemented
- `impl Trait`: a function returning `impl Iterator` works with `for` loops
- `never`: `exit(1)` can be used in any branch regardless of expected type
- Borrow checker: use-after-move is a compile error, not a runtime error
- Borrow checker: mutable reference exclusivity is enforced at compile time
- Borrow checker: a reference cannot outlive its referent (must be lexical scope at minimum)
- All existing tests pass (code that would fail the borrow checker is updated)
- The runtime use-after-move error path can still be kept as a safety net (debug mode)

## Potential Challenges

- GATs are notoriously difficult to type-check correctly (witness Rust's multi-year GAT stabilization effort).
- Lifetime inference is complex and interacts with HKT and const generics.
- The borrow checker will reject some currently-working code (code that passes at runtime may fail at compile time).
- Partial moves already have compile-time tracking — extending this to full borrow checking is a significant rework of `typecheck.cpp`.
- The `never` type interacts with the existing `unit` type — careful design needed to avoid confusing the two.