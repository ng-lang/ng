# Memory Model

The runtime value representation and the GC-free allocation story.

## Runtime values

`NG::Value` is a variant:

- `int64_t` and `double` — integers (all widths are checked against their
  static width at runtime) and floats
- `string` — interned-style `shared_ptr` strings (deep-copied on bind)
- arrays, tuples, structs, enums — `shared_ptr`-backed aggregates;
  ordinary copies are deep (`deepCopy`), so values never alias through
  copy semantics
- references — a (root cell, place steps) view; writes go through the
  cell, so mutations stay visible after rebinds
- trait views — a reference plus a dispatch-table reference
- opaque — `uint64_t` handle tokens for `native fun` boundaries
  (`type X = native;`)
- ranges — (start, end) pairs

## Ownership at compile time

- `Copy` types (scalars, strings, tuples, derived `Copy` types) deep-copy
  on bind/call/return.
- Affine nominal types move; `clone` copies explicitly; `move` makes the
  transfer explicit. Partial moves are **struct-field** aware — moving one
  field out of a struct leaves the remaining fields usable:

  ```ng
  struct N { v: Box<i64> }
  let n = N { v: new(1) };
  let m = move n.v;     // OK
  ```

  Tuple-index partial moves are **rejected**: moving `t.0` and then `t.1`
  reports `use of moved value`, because the tuple moves as one value
  rather than field-wise.
- `impl Drop` runs exactly once per initialized value on every scope
  exit; drop edges are emitted by the lowering. A value moved out is not
  dropped at the source binding; the moved-to binding is.
- `move` also works as a bare statement, which consumes the value and
  drops it immediately:

  ```ng
  let s = new(1);
  move s;               // s is consumed and its handle released here
  ```
- `ref`/`ref mut` are scoped views — no returns — and loans release at
  last use. They may be stored in aggregates (for example
  `array<ref<i64>>` compiles) and in recursive-enum self payloads
  (`next: ref<Node<T>>`); such references never outlive values reachable
  in their scope.

## Allocation

There is no garbage collector. Allocation happens in two places:

- **Aggregate cells** — arrays/tuples/structs/enums are
  `std::shared_ptr`-backed; cells are freed when the last value/reference
  goes away (deterministic, refcounted — not a tracing GC).
- **Native handles** — the `memory` stdlib module
  (`allocate`/`load`/`store`/`release`/`outstanding`) manages an
  embedding-owned slot table. Two generic affine wrappers own those
  handles and release them through `Drop`: `Box<T>`
  (`new`/`box`/`read`/`write`, plus `clone`) and `Cell<T>` with interior
  mutability (`newCell`/`cellGet`/`cellSet` through a shared `ref`).
  `outstanding()` reports live cells and is how drop-exactly-once is
  asserted.

See the [memory management guide](/guide/memory-management) for the
language-level view.
