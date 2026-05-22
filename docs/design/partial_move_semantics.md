# Partial Move Semantics

This document tracks Issue #38. The goal is to make `move obj.field` and `move tuple[0]` statically meaningful without turning StorageCell move flags into the long-term language semantics.

## Dependencies

Partial moves depend on the existing value/reference/move model, StorageCell moved sentinels, and `Drop`. They should land before generalized `= delete`, enhanced tuples, and auto-derived traits because those features need stable initialization and destruction rules.

## Phase A Scope

Phase A implements structural place tracking for:

- local/global bindings: `move value`
- object fields: `move obj.field`
- fields through references: `move self.field`
- tuple constant indexes: `move tuple[0]`
- reassignment that restores a moved place: `obj.field := value`, `tuple[0] := value`

Dynamic array element moves are intentionally excluded. Arrays need either fixed-size constant-generic indexing or explicit APIs such as `take`, `replace`, and `swap_remove`.

## Static Rules

A place is represented by a root binding plus structural components. Examples:

- `obj`
- `obj.field`
- `self.ptr`
- `tuple[0]`

Moving a whole binding marks the binding moved. Moving a field or tuple slot marks only that structural place moved.

After a partial move:

- reading the moved place is rejected
- reading sibling fields or tuple slots is allowed
- reading the containing object as a whole is rejected until all moved subplaces are reassigned
- method calls on the partially moved object are rejected because the receiver is a whole-object use
- taking `ref obj` is rejected while `obj` is partially moved
- reassigning the exact moved place restores that place

Assignments clear the assigned place and any tracked subplaces below it. This lets `obj.field := value` restore `obj.field`, and `obj := value` restore the whole object.

## Drop Rules

Drop is field-aware:

- initialized fields are dropped normally
- moved-out fields are skipped
- a parent object with moved fields cannot be used as a complete value by user code
- custom `Drop::drop(self: ref<Self>)` may move resource fields, for example `nativeFree(move self.ptr)`

The StorageCell moved sentinel remains a STUPID/ORGASM safety net and debugger-visible runtime assertion. It is not intended to be required by a future native backend after static place checking has enough coverage.

## Conservative Alias Rule

Phase A does not implement a full borrow checker or arbitrary alias graph. Direct operations are still checked by place state. Existing references are protected by runtime moved sentinels, and a later static place-and-borrow pass can reject moves while a known live reference to the same root exists.

## Later Phases

- Add lexical borrow tracking for direct `ref` aliases.
- Add fixed-size array partial move after constant generic parameters.
- Add explicit dynamic array extraction APIs.
- Add richer method effect information if method calls on partially initialized receivers become necessary.

## Acceptance Tests

- `move obj.field` allows reading `obj.other`.
- reading `obj.field` after the move fails.
- reading `obj` after the move fails.
- calling `obj.method()` after the move fails.
- `obj.field := value` restores whole-object use.
- `move tuple[0]` allows reading `tuple[1]`.
- reading `tuple[0]` or `tuple` after the move fails.
- `tuple[0] := value` restores whole-tuple use.
- `move array[0]` is rejected until arrays have explicit extraction semantics.
