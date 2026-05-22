# Partial Move Semantics

This document tracks Issue #38. The goal is to make `move obj.field` and `move tuple[0]` statically meaningful, with StorageCell moved sentinels kept as a runtime safety net rather than the long-term source of language semantics.

## Dependencies

Partial moves depend on the value/reference/move model, `Drop`, and the StorageCell moved sentinel. They should remain earlier than generalized `= delete`, enhanced tuples, auto-derived traits, and fixed-size arrays because those features need stable initialization and destruction rules.

Array element partial move is still out of scope until constant generic parameters provide fixed-size arrays. Dynamic arrays should use explicit extraction APIs such as `take`, `replace`, or `swap_remove`; `move array[index]` is rejected today.

## Place Model

A tracked place is a lexical root plus structural components:

- `value`
- `obj.field`
- `obj.inner.field`
- `tuple[0]`
- `tuple[0][1]`
- `self.resource`

Object fields and tuple constant indexes are tracked recursively. Field access through a `ref<T>` receiver uses the same place key, so `move box.inner.left` can track a nested field even when `box` or `inner` is a reference.

Moving a whole binding marks the binding moved. Moving a field or tuple slot marks only that subplace moved. Reading the moved subplace is rejected, reading disjoint sibling places is allowed, and reading the containing whole place is rejected until all moved subplaces are restored.

Assignments restore initialization state:

- `obj.field := value` clears the moved state for `obj.field`.
- `tuple[0] := value` clears the moved state for `tuple[0]`.
- `obj := value` clears all moved child state below `obj`.
- `obj.inner := value` clears all moved child state below `obj.inner`.

## Control Flow

Partial-move state is propagated through compound scopes, branches, loops, and switches.

Branch and switch exits merge moved state conservatively: if a place may be moved on any path, it is treated as moved after the control-flow construct. If every path restores the same moved place, the restored state is visible after the branch.

Loop bodies propagate moved state to the loop exit. This is intentionally conservative because the current checker does not prove loop iteration counts or path reachability.

Lexical scope filtering removes places and direct borrow aliases whose roots leave scope.

## Direct Borrow Tracking

The checker tracks direct lexical aliases created by `ref place` and `&place`.

While a direct alias is live:

- moving the borrowed place is rejected
- assigning the borrowed place is rejected
- moving through the alias, such as `move *borrowed`, is rejected
- moving or assigning disjoint sibling places is allowed
- leaving the alias scope releases the borrow

This is not a full lifetime or arbitrary alias graph. It is a direct-place borrow check for the current `ref` surface and prevents the main unsound cases introduced by partial move.

## Method Effects

Receiver methods carry inferred structural effects relative to the receiver parameter, conventionally `self`.

The type checker records ordered receiver effects:

- reads: `self.field`, `self.tuple[0]`
- writes: `self.field := value`
- moves: `move self.field`
- unknown effects: taking a ref to receiver state or calling through receiver state when effects cannot be resolved

Calling a method on a partially moved receiver is allowed only when the ordered effects do not read or move a moved place. Writes restore the written place, so a method that only assigns `self.left` can reinitialize `box.left` after `move box.left`. Whole-receiver or unknown-effect methods remain conservative and require the receiver to be fully initialized.

Method moves update the caller's place state. This is required for `Drop::drop(self: ref<Self>)` implementations that consume resource fields with `move self.ptr`.

## Drop Rules

Drop is field-aware and deterministic across STUPID and ORGASM:

- initialized fields are dropped normally
- moved-out fields are skipped
- remaining initialized child fields are dropped after a custom/native `Drop` handler
- custom `Drop::drop(self: ref<Self>)` may move resource fields
- a field moved inside `Drop::drop` is not double-dropped by recursive child cleanup

The runtime still checks moved sentinels before dropping a cell. This is a safety net and debugging assertion for STUPID/ORGASM. A future native backend should rely on static place checking and should not need a dynamic moved check for well-typed code.

## Acceptance Coverage

Current tests cover:

- `move obj.field` allows reading sibling fields.
- reading the moved field fails.
- reading the whole object after a partial move fails.
- method calls on partially moved receivers use inferred read/write/move effects.
- method writes can restore moved receiver fields.
- nested field moves and whole-place overwrites clear child state.
- `move tuple[0]` and nested tuple constant-index moves.
- branch, switch, loop, and scope moved-state propagation.
- direct `ref` aliases reject conflicting move/assignment and release at lexical scope exit.
- `move array[0]` is rejected.
- STUPID and ORGASM both run partial-move and Drop regression examples.
