# Memory Management

NG's runtime model: value semantics, cell-backed references, and an
explicit GC-free heap. No garbage collector exists.

## Runtime values

Every NG value is a `Value`: integers and floats by value; strings,
arrays, tuples, structs, and enums as `shared_ptr`-backed aggregates;
references as (cell, steps) views; opaque tokens for native handles;
ranges as (start, end) pairs.

## Copy-first value semantics

`Copy` values deep-copy on bind, call, and return; aggregates never alias
through ordinary copies. Affine nominal types move instead, with
`clone`/`move` expressions making the choice explicit (see
[References, Moves & Ownership](/guide/references-moves)).

## References are views, not ownership

`ref`/`ref mut` capture the root cell plus place steps; writes through
them stay visible after rebinds. They never escape their scope (no
returns, no aggregate storage except a recursive enum's own payload), so
no lifetime syntax is needed. Loans release at last use (NLL).

## Drop

`impl Drop` runs exactly once per initialized affine value on scope exit,
including early returns and block fall-through; moved-out bindings skip
their drop while moved-to bindings drop normally. Partial moves are
field-aware so a destructor never double-owns.

```ng
struct Resource { id: i64 }

impl Drop for Resource {
    fun drop(self: Self ref) -> unit {
        print("dropped");
    }
}
```

## The explicit heap (no GC)

The `memory` module exposes native handles (`Handle = native`) with
explicit `release`, plus generic owning wrappers whose `Drop` frees the
cell when the wrapper goes out of scope.

### `Box<T>`

`Box<T>` is affine and generic. `new<T>(value)` allocates a cell holding a
deep-copied snapshot of `value`; `read(ref box)` yields a deep copy out;
`write(ref box, updated)` snapshots a new payload in.

```ng
import prelude;
import memory;

fun make() -> i64 {
    let mut cell = box(7);
    write(ref cell, 9);
    return read(ref cell);   // prints 9; cell's Box drops here
}

fun main() -> unit {
    print(make());
}
```

`box(value)` is an alias for `new(value)`. Payloads ride the 64-bit word
slot, so integers, bools, unit, strings, arrays, structs, enums, tuples,
and floats (`f32` bit-preservingly) all round-trip. `Box<T>` also supports
`clone()`, which deep-copies into a fresh cell: the original stays live and
writes through the clone never alias it. Both cells release exactly once.

### `Cell<T>`

`Cell<T>` is a second affine, generic handle with interior mutability.
`newCell<T>(value)` creates it; `cellGet`/`cellSet` borrow it through a
*shared* `ref`, so any number of co-borrowers may read and write the same
cell without a `ref mut` (ref-mut exclusivity still applies the moment a
mutable borrow is actually taken).

```ng
import prelude;
import memory;

fun bump(cell: Cell<i64> ref, by: i64) -> unit {
    cellSet(cell, cellGet(cell) + by);
}

fun main() -> unit {
    let counter = newCell(0);
    let left = ref counter;
    let right = ref counter;
    bump(ref counter, 40);
    cellSet(right, cellGet(right) + 2);
    print(cellGet(left));    // 42 — both views see the write
}
```

### Counting live cells

`outstanding()` reports live allocations. Cells created inside a function
are released on that function's exit paths, so the count returns to its
pre-call value (verified by `example/box.ng`):

```ng
let before = outstanding();
let t1 = new("tmp");
let t2 = new([9, 9]);
let t3 = new(Point { x: 1, y: 2 });   // all three drop at scope end
```

At process exit the count is `0` for handle-releasing code, and a leaked
handle would keep it non-zero during execution — `outstanding()` is the
hook for asserting that discipline in tests.

Recursive structures use `ref`-payload enums (`Node<T>`) rather than
handles; see [Data Structures](/guide/data-structures).

## Interop

Native hosts receive deep-copied `Value`s and return `Value`s; opaque
handles cross the boundary as tokens (`type X = native;`). See
[C++ Compatibility](/ref/cxx-compatibility).
