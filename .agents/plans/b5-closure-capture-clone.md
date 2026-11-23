# B5 — copy-capture of affine values requires Clone (cloned per call)

> **Status: LANDED 2026-11-23 (commit `4a4738c`).** Kept for reference; the
> semantics are now documented in `docs/ref/language-decisions.md` (D-006).

## Defect (reproduced)
```
import prelude; import memory;
fun main() -> i64 {
    let c = newCell(5);
    let f = [c] |x: i64| x + 1;
    return f(1);
}
```
→ `invalid heap handle` + `killed by signal` even on the FIRST call.

Root cause: captures are threaded by local id into every call
(`Checker::threadCallCaptures` → `callCaptures_`, `src/typecheck_call.cpp:36`).
For an affine captured value the callee receives/consumes the same cell handle
each call, so the first call invalidates it. Only `[move c]` marks the enclosing
local consumed (`src/typecheck_call.cpp:862`); a copy capture marks nothing, so
an uncalled `[move c]` closure also leaks.

## Decided semantics
`[c]` (copy capture) of an **affine** value requires the type to implement
`Clone`; the capture is **cloned at each call** so the original stays live and
each callee owns its own value. Affine types without `Clone` must use
`[move c]` or `[ref c]`/`[ref mut c]` — otherwise a compile-time diagnostic.

## Implementation notes gathered
- `hir::Expression::CaptureInfo{ local, isMove }` (include/hir.hpp:127).
- `Checker::checkClosure` builds `closureCapturesByLocal_` → `CaptureLocalInfo`
  `{localId, isMove, type}` (src/typecheck_check.cpp:326-331).
- Clone availability: `derivedCloneTypes_` is populated ONLY by `derive(Clone)`
  (src/typecheck_register.cpp:102-103) — a hand-written `impl Clone` is not in it.
  Use `impls_` (which contains explicit impls too) for the "is Clone" predicate.
- Per-call clone insertion: mark copy captures of affine types in
  `TypeCheckResult` and have FlowIR emit a clone per call of those captures.
- Existing clone lowering machinery: `derivedCloneCalls_` / `derivedCloneTypes_`
  and the Box clone path (`boxCloneCalls_`, `src/typecheck_call.cpp:675-691`).

## Sub-defect discovered (must fix for the above to be usable)
`clone` on `Cell<T>` is itself broken — it shallow-copies the handle instead of
allocating a new cell:
```
import prelude; import memory;
fun main() -> i64 {
  let c = newCell<i64>(5);
  let d = clone c;          // no new allocation
  cellSet(ref d, 9);        // invalid heap handle
  return cellGet(ref d);
}
```
→ `invalid heap handle`. `outstanding()` does not increase across `clone c`.
So `Cell<T>` is effectively NOT Clone today. Since the decided semantics make
copy-capture depend on `clone`, fix order:
1. Give `Cell<T>` a real Clone (deep copy → new cell), or
2. reject copy-capture of non-Clone affine values (which then rejects `Cell`,
   forcing `[move c]` / `[ref c]`).

Verified: `derive(Copy + Clone)` on a struct works (`clone p` → correct field).

## Status
Planned, not started. Blocker resolved by decision 2026-11-23:
copy-capture of affine values requires Clone, cloned per call.

## Follow-up defect (still open, NOT fixed by B5)
`clone` on `Cell<T>` is a shallow handle copy rather than a deep copy, so it
aborts independently of captures:
```
import prelude; import memory;
fun main() -> i64 {
  let c = newCell<i64>(5);
  let d = clone c;      // no new allocation
  cellSet(ref d, 9);    // invalid heap handle
  return cellGet(ref d);
}
```
→ `invalid heap handle`. B5 does not depend on it (copy-capturing a Cell is
now rejected because Cell is not Clone), but `clone c` used directly is still
wrong. Fix would give `Cell<T>` a real Clone (allocate a fresh cell).
