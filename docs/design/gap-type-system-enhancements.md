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

## Sub-Proposal B: `impl Trait` — NOT APPLICABLE (Rejected by Design)

> **Status: REJECTED.** NG's trait model already provides equivalent functionality without `impl Trait`.

### Why This Doesn't Fit NG

Rust's `impl Trait` syntax creates an anonymous/existential type: "some type T that implements Trait." This is necessary in Rust because traits are **not** types — they can only be used as bounds.

In NG, traits **are** abstract types that can be used directly:

1. **Traits as types** — `ref<Show>` or `Show` can be used as a parameter/return type directly
2. **Trait objects** — `val view: ref<Show> = counter;` stores any `Show` implementor
3. **Generic bounds** — `<T: Show>` constrains a type parameter

### Equivalent NG Patterns

Instead of Rust's `impl Trait`, NG uses existing constructs:

```ng
// Return position: use trait reference or generic
fun makeIterator() -> ref<Iterator<Item = i32>> { ... }

// Argument position: use trait reference or generic bound
fun process(items: ref<Iterable>) { ... }
// or:
fun process<T: Iterable>(items: ref<T>) { ... }
```

### Conclusion

No implementation needed. The existing trait system (`trait` as abstract type, `ref<Trait>` for dynamic dispatch, `<T: Trait>` for static dispatch) covers all use cases that `impl Trait` would address.

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

- `val y = move x; print(x[0]);` is a **compile-time warning** (not runtime error — Phase 1 keeps runtime check)
- Two simultaneous `ref<T>` to the same value is a compile error
- `ref<T>` and `ref<const T>` cannot coexist
- A reference doesn't outlive its referent (lexical scope) — compile-time warning
- Partial moves are compile-time warnings on subsequent reads
- Runtime use-after-move check is always present as a safety net
- All existing tests pass unchanged

### Scope Limit: Lexical Borrows Only

This implementation limits itself to **lexical borrows** (reference scope = enclosing block). It does NOT attempt:
- Non-lexical lifetimes (NLL) — would require flow-sensitive analysis
- Two-phase borrows — future enhancement
- Lifetime elision — future enhancement

The existing runtime move checking remains as a safety net for any cases the compile-time checker cannot prove.

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

## GAT Feasibility Note

**Verdict: Not achievable without a lifetime system.** GATs require:
- Lifetime parameter syntax in trait declarations
- Lifetime inference and variance checking
- Lifetime-aware type unification

None of these exist in NG, and adding a lifetime system is a 12+ month project on its own.

**Recommendation:** Defer indefinitely. Revisit when/if NG gains lifetime annotations.

---

## Summary: Implementation Order

| Order | Feature | Effort | Dependencies | Risk |
|---|---|---|---|---|
| 1 | `never` type | 2 weeks | None | 🟢 Low |
| 2 | `impl Trait` | **Rejected** — covered by existing trait system | — | — |
| 3 | Borrow checker (lexical only) | 6-8 weeks | Partial move tracking (exists) | 🟡 Medium |
| 4 | GAT | **Deferred indefinitely** | Needs lifetime system | 🔴 Very High |