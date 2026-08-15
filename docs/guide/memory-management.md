# Memory Management

NG's runtime model: value semantics, cell-backed references, and an
explicit GC-free heap. No garbage collector exists or is planned for the
first release.

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
including early returns and block fall-through; partial moves are
field-aware so a destructor never double-owns.

## The explicit heap (no GC)

The `memory` module exposes native handles with explicit `release` and a
concrete `Box` whose `Drop` frees its cell when the box goes out of
scope:

```ng
import memory;

fun make() -> i64 {
    let mut cell = box(7);
    write(ref cell, 9);
    return read(ref cell);   // cell's Box drops here, freeing the handle
}
```

`outstanding()` reports live allocations. Generic `Box<T>`, `Gc`, `Arc`,
arenas, and `new` arrive with the runtime-session work; until then,
recursive structures use `ref`-payload enums (`List<T>`) and native
handles.

## Interop

Native hosts receive deep-copied `Value`s and return `Value`s; opaque
handles cross the boundary as tokens (`type X = native;`). See
[C++ Compatibility](/ref/cxx-compatibility).
