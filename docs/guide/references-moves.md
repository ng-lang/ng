# References, Moves & Ownership

NG's ownership model: copy-first value semantics, affine nominal types
with `move`/`clone`, `impl Drop` lifecycle, and borrow-checked scoped
references. There is no GC and no user-visible lifetime syntax.

## Copy-first value semantics

`Copy` types (integers, floats, bools, strings, tuples, and `Copy`-derived
types) copy deeply on bind, call, and return — values never alias:

```ng
let a = "hello";
let b = a;          // deep copy; a is untouched
```

## Affine nominal types

Structs/enums are affine by default: binding, calling, or returning one
**moves** it, and use-after-move is a compile error:

```ng
let first = makeCounter();
let second = first;        // move
print(first.value);        // type error: use of moved value
```

`clone` makes an explicit copy; `move` makes an implicit move explicit:

```ng
let second = clone first;
let third = move second;
```

### Partial moves

Move a struct field or tuple element and the rest stays usable
(field-aware tracking across branches, loops, and switches):

```ng
let pair = (3, "kept");
let movedHead = move pair[0];
assert(pair[1] == "kept");
```

## `impl Drop`

Affine types can declare a destructor that runs exactly once when an
initialized value leaves scope (returns and fall-through included):

```ng
struct Handle {
    id: i64,
}

impl Drop for Handle {
    fun drop(self: Self ref) -> unit {
        print("dropping handle");
    }
}
```

Drop edges are block-scoped, field-aware (moving a field out skips
double-ownership), and emitted by the lowering — never user-visible.

## References

`ref` is a scoped, non-returnable view; `ref mut` is exclusive:

```ng
let mut value = 1;
let read = ref value;
assert(*read == 1);
let write = ref mut value;
*write := 2;
```

References cannot be returned from functions or stored in aggregates
(the one sanctioned exception is a recursive enum's own `ref<...>` payload).

### Borrow checking with non-lexical loans

Shared and mutable borrows conflict, but loans end at **last use**, not
block exit:

```ng
let read = ref value;
let seen = *read;            // last use of read
let write = ref mut value;   // ok: the loan is already released
```

Overlapping borrows are rejected:

```ng
let read = ref value;
let write = ref mut value;   // error: value is shared-borrowed
```

Inline call-site borrows live only for their statement:

```ng
bump(ref mut value);         // loan ends after this statement
let read = ref value;        // ok
```

## The heap (no GC)

The stdlib `memory` module provides native handles with explicit
release and a concrete `Box` whose `Drop` frees the cell
(`example/heap_box.ng`). Generic `Box<T>`/`Gc`/`Arc` arrive with the
runtime-session work; recursive data structures use `ref`-payload enums
instead (see [Data Structures](/guide/data-structures)).

Next: [Traits](/guide/traits).
