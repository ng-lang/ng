# Type System Enhancements

> **Status:** Restructured from 4-feature monolith into 4 independent sub-proposals.
> Each sub-proposal is independently estimable and deliverable.

---

## Sub-Proposal A: `never` Type (2 weeks)

### Goal

Add a bottom type `never` that represents computations that never produce a value.

### Motivation

Currently, functions like `exit(1)`, `panic()`, or infinite loops cannot be used as expressions. The type checker must reject valid patterns:

```ng
fun unwrapOrPanic<T>(opt: Option<T>) -> T {
    match (opt) {
        case Some(v) => v;
        case None => exit(1);  // ERROR: exit returns unit, not T
    }
}
```

### Design

#### Syntax

```ng
fun exit(code: i32) -> never;   // Function never returns
```

`never` is a built-in type, like `unit`.

#### Type Checking Rules

1. `never` unifies with **any** type (it's a bottom type)
2. A function declared `-> never` never returns normally
3. The type of `return` in a `never`-returning function is `never`
4. `never` cannot be assigned to a variable of type `never` (no values of type `never` exist)
5. The result type of a `loop { break; }` or `match` with all branches returning `never` is `never`

#### Implementation

The `never` type is added to the type system as a new `PrimitiveType` variant:

```cpp
// In include/typecheck/typeinfo.hpp
struct NeverType : TypeInfo {
    // Singleton: no fields
};
```

#### Grammar Changes

```ng
// Token: KEYWORD_NEVER (already exists as part of the language? Check.)
// If not: add to lexer, map to builtin type.
fun abort() -> never = native;
```

### Sub-Proposal A Acceptance Criteria

- `fun exit() -> never` compiles and the type checker accepts it
- `match (opt) { case Some(v) => v; case None => exit(); }` type-checks (never unifies with T)
- A value of type `never` cannot be created (compile error)
- `never` can be used in return position of native functions
- All existing tests pass

---

## Sub-Proposal B: `impl Trait` (3 weeks)

### Goal

Allow functions to return opaque types that implement a trait, without naming the concrete type.

### Motivation

```ng
// Without impl Trait: must name the concrete iterator type
fun range(start: i32, end: i32) -> RangeIter<i32> { ... }

// With impl Trait: return any type implementing Iterator
fun range(start: i32, end: i32) -> impl Iterator<Item = i32> { ... }
```

### Design

#### Syntax

Return position:
```ng
fun makeIterator() -> impl Iterator<Item = i32> {
    return RangeIter<i32> { start: 0, end: 10 };
}
```

Argument position:
```ng
fun process(items: impl Iterable) {
    for item in items { ... }
}
```

#### Type Checking

- `impl Trait` in return position creates an **existential type**: "there exists some type T such that T: Trait"
- The concrete type is inferred from the function body
- The caller cannot name or depend on the concrete type
- `impl Trait` in argument position is syntactic sugar for a generic parameter: `fun process<T: Iterable>(items: T)`

#### Implementation

```cpp
struct ImplTraitType : TypeInfo {
    CheckingRef<TraitBound> bound;  // The trait constraint
};
```

The type checker:
1. Creates an `ImplTraitType` as the return type
2. Checks that the returned expression satisfies the trait bound
3. When used by the caller, only the trait methods are visible

### Sub-Proposal B Acceptance Criteria

- `fun foo() -> impl Show { ... }` compiles and the returned value can call `.show()`
- A different return type that still impl `Show` is accepted
- A return type that does NOT impl `Show` is a compile error
- `impl Trait` in argument position accepts any matching concrete type
- Multiple calls to the same function with different concrete types both work

---

## Sub-Proposal C: Compile-Time Borrow Checker (3-4 months)

### Goal

Replace the current **runtime** use-after-move detection with **compile-time** ownership and borrowing rules.

### Motivation

Currently, use-after-move is checked at runtime:

```ng
val x = [1, 2, 3];
val y = move x;
print(x[0]);  // Runtime error: use after move
```

A compile-time borrow checker would catch this before execution, providing stronger safety guarantees and eliminating the runtime error path.

### Design

#### Borrowing Rules (Phase 1: Lexical Borrows)

The type checker enforces:
1. **At any time, a value has either**:
   - One `ref<T>` (mutable reference), OR
   - Any number of `ref<const T>` (immutable references)
2. **A reference must not outlive its referent** (lexical scope check)
3. **A value cannot be used after `move`** (already partially implemented)
4. **A moved field cannot be accessed until reassigned** (already implemented as partial move tracking)

#### Type Checker Implementation

The existing partial move tracking in `typecheck.cpp` is extended:

```cpp
// New tracking structures
struct BorrowEntry {
    Str place;            // e.g., "person.name"
    RefKind kind;         // Mutable | Immutable
    SourcePosition start; // Where the borrow started
    SourcePosition end;   // Where the borrow ends (scope exit)
};

struct MoveTracker {
    HashMap<Str, bool> moved;       // place → isMoved
    Vec<BorrowEntry> activeBorrows; // currently active borrows
};
```

#### Migration Path

1. **Phase 1a**: Add compile-time warnings for move/borrow violations (during type checking)
2. **Phase 1b**: Convert warnings to errors for simple cases (lexical borrows only)
3. **Phase 1c**: Keep runtime check as fallback for complex cases not yet handled

#### Code Changes

| File | Change |
|---|---|
| `src/typecheck/typecheck.cpp` | Add borrow tracking alongside existing move tracking |
| `src/typecheck/typecheck.cpp` | `visit(RefExpression*)` → register borrow scope |
| `src/typecheck/typecheck.cpp` | `visit(AssignmentExpression*)` → check borrow conflicts |
| `src/typecheck/typecheck.cpp` | `visit(IdExpression*)` → check use-after-move |
| `src/runtime/` | Keep runtime checks in debug mode; skip in release |

### Sub-Proposal C Acceptance Criteria

- `val y = move x; print(x[0]);` is a **compile error** (not runtime)
- Two simultaneous `ref<T>` to the same value is a compile error
- `ref<T>` and `ref<const T>` cannot coexist
- A reference doesn't outlive its referent (lexical scope)
- Partial moves are compile-time errors on subsequent reads
- Runtime use-after-move check is still present in debug mode
- All existing tests pass (code that triggers runtime errors may need updating)

---

## Sub-Proposal D: Generic Associated Types (GAT) — 4-6 months

### Goal

Allow traits to have associated types that are themselves generic over lifetimes or type parameters.

### Motivation

```ng
trait Collection {
    type Item;
    type Iter<'a>: Iterator where Self: 'a;

    fun iter<'a>(self: ref<'a, Self>) -> Self::Iter<'a>;
}

impl Collection for [i32] {
    type Item = i32;
    type Iter<'a> = SliceIter<'a, i32>;

    fun iter<'a>(self: ref<'a, Self>) -> SliceIter<'a, i32> { ... }
}
```

### Design

#### Syntax

```ng
trait StreamingIterator {
    type Item<'a>: SomeTrait;
    fun next<'a>(self: ref<'a, Self>) -> Option<Self::Item<'a>>;
}
```

#### Type Checker Implementation

GATs require:
1. **GAT declaration syntax** in trait definitions
2. **GAT implementation** in impl blocks
3. **GAT resolution** when `Self::Item<'a>` is used
4. **Where clause support** for GAT bounds

GATs are fundamentally higher-kinded: `Item` is a type constructor `Lifetime → Type`. The existing HKT infrastructure (`F<_>`) can be extended to support lifetime parameters.

#### Dependencies

- Requires existing HKT infrastructure (already implemented)
- Requires lifetime parameter support in the type system (new)

### Sub-Proposal D Acceptance Criteria

- A trait with a GAT compiles
- An impl provides a concrete GAT mapping
- `Self::Item<'a>` resolves to the correct type
- GATs work with where clauses: `where <T as Collection>::Iter<'a>: Iterator`
- Complex examples (e.g., streaming iterator) compile and type-check correctly

---

## Summary: Implementation Order

| Order | Feature | Effort | Dependencies | Risk |
|---|---|---|---|---|
| 1 | `never` type | 2 weeks | None | 🟢 Low |
| 2 | `impl Trait` | 3 weeks | Trait system (exists) | 🟡 Medium |
| 3 | Borrow checker | 3-4 months | Partial move tracking (exists) | 🔴 High |
| 4 | GAT | 4-6 months | HKT infrastructure (exists) | 🔴 Very High |