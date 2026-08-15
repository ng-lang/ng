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
  transfer explicit; partial moves are field-aware.
- `impl Drop` runs exactly once per initialized value on every scope
  exit; drop edges are emitted by the lowering.
- `ref`/`ref mut` are scoped views — no returns, no aggregate storage
  (except recursive-enum self payloads) — and loans release at last use.

## Allocation

There is no garbage collector. Allocation happens in two places:

- **Aggregate cells** — arrays/tuples/structs/enums are
  `std::shared_ptr`-backed; cells are freed when the last value/reference
  goes away (deterministic, refcounted — not a tracing GC).
- **Native handles** — the `memory` stdlib module
  (`allocate`/`load`/`store`/`release`/`outstanding`) manages an
  embedding-owned slot table; the concrete `Box` releases its handle
  through `Drop`. Generic `Box<T>`/`Gc`/`Arc` are deferred to the
  runtime-session work.

See the [memory management guide](/guide/memory-management) for the
language-level view.
