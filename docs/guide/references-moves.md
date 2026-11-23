# References, Moves & Ownership

NG's ownership model: copy-first value semantics, affine nominal types
with `move`/`clone`, `impl Drop` lifecycle, heap cells in `std.memory`,
and borrow-checked scoped references. There is no GC and no user-visible
lifetime syntax.

## Copy-first value semantics

`Copy` types (integers, floats, bools, strings, tuples, arrays, and
`Copy`-derived types) copy deeply on bind, call, and return — values never
alias:

```ng
let a = "hello";
let b = a;          // deep copy; a is untouched
```

Assigning through one binding of an array leaves the other untouched:

```ng
let mut original = [1, 2, 3];
let mut copied = original;
copied[0] := 9;     // original[0] is still 1
```

## Affine nominal types

Structs/enums are affine by default: binding, calling, or returning one
**moves** it, and use-after-move is a compile error:

```ng
let first = makeCounter();
let second = first;        // move
return first.value;        // type error: use of moved value `first`
```

The error is `use of moved value `first``. `clone` makes an explicit copy;
`move` makes an implicit move explicit:

```ng
let second = clone first;
let third = move second;
```

### Partial moves

Moving a struct field invalidates the binding as a whole until the field
is reassigned; assigning the field revives it and every field is readable
again:

```ng
let mut partial = Point { x: 5, y: 6 };
let moved = move partial.x;
partial.x := 7;             // revive
return partial.y;           // ok
```

Reading any field of the binding before the revive is rejected:
`let yy = partial.y;` right after the move reports *use of partially
moved value `partial`*. Partial moves are field-aware for structs; moving
a tuple element moves the whole tuple, so `move pair[0]` followed by
`pair[1]` reports *use of moved value `pair`*.

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

References cannot be returned from a function unless the return type
declares an origin set. `fun getRef() -> ref<i64> { return ref v; }` is
rejected with *references cannot be returned from a function; declare the
origin set on the return type (`T ref(a | b)`)*.

References *can* be stored in aggregates, including inside arrays:

```ng
let mut v = 1;
let views: array<ref<i64>> = [ref v];
assert(*views[0] == 1);
```

An `array<ref<Trait>>` dispatches dynamically through each element:

```ng
let views: array<ref<Printer>> = [counter, number];
views[0].show();
```

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
let write = ref mut value;   // error: cannot mutably borrow `value` while it is shared-borrowed
```

Inline call-site borrows live only for their statement:

```ng
bump(ref mut value);         // loan ends after this statement
let read = ref value;        // ok
```

Assigning through a `ref` is rejected — *cannot assign through an
immutable reference*; use `ref mut`.

## Heap cells in `std.memory`

The `memory` module provides generic owning cells over native handles.
There is no GC: each cell is released drop-exactly-once when its owner
leaves scope.

```ng
import memory;

struct Point {
    x: i64,
    y: i64,
}

let boxed = new<Point>(Point { x: 1, y: 2 });
let value = read(ref boxed);       // deep copy out of the cell
write(ref boxed, Point { x: 3, y: 4 });
```

`Box<T>` is exported together with `new<T>`, `box<T>`, `read<T>` and
`write<T>`. `outstanding()` reports the live cell count, so drop
discipline is observable at runtime (see `example/box.ng`).

`Cell<T>` is a handle with interior mutability: `newCell<T>` allocates it
and `cellGet`/`cellSet` take a *shared* `ref`, so many co-borrowers read
and write the same cell without a `ref mut` (see `example/cell.ng`).

Recursive data structures can use either `ref`-payload enums or boxed
payloads (see [Data Structures](/guide/data-structures)).

Next: [Traits](/guide/traits).
