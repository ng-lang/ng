# Workstate

## Current focus

Executing the vNext increment plan (`docs/design/rearchitecture/09-increment-plan.md`) one
increment at a time: design → implement → verify (unit + conformance/PBT corpus, ASan
variant) → doc-sync → commit (feat + docs separately).

Maintainer swap decision (2026-08-30): **INC-14 minimal closures moved before
INC-12/13**, executed in S1–S6 slices because the combinators/law PBT need transform
arguments that NG cannot express without first-class functions.

## Status

- INC-0 … INC-8, ARCH-1 … ARCH-4, PBT consolidation: committed.
- INC-A (prelude auto-loading): skipped by user decision (visibility-blind overload registration).
- INC-B (core traits → `prelude.ng`): committed `c1e733b`/`4040ecd`.
- INC-9 (error codes + did-you-mean): committed `e38e51c`/`55b815d`.
- INC-10 + DEC-B (while/continue/break): committed; D-010 revised.
- INC-11 (derive(Show)): committed `fe8ee4d`/`75be8c5`; ASan imgui link fix committed
  `120eb50` (imgui archives kept uninstrumented).
- INC-14 **S1** (minimal closures — syntax, explicit param types, direct calls): committed
  `ffae039`/`bc4729a` (512 cases / 3511 assertions, ctest 513/513, ASan 512/512).
- INC-14 **S2a+S2b** (zero-param `||` heads, expected-type-driven inference, `|T| -> R`
  type literal, tail-return latent fix): committed `777651d`/`ed2e18c`/`2a0d879`/`002849e`
  (515 cases / 3525 assertions, ctest 516/516, ASan 515/515).
- INC-14 **S2c** (higher-order closures + std.seq `map<T,U>`): committed
  `d964f63`/`c7a7e39`/`25a65b4` (525 cases / 3553 assertions, ctest 526/526,
  ASan 525/525).
- INC-14 **S3** (copy/move captures + static capture threading): committed
  `a07198a`/`6921620`/`01390c7` (526 cases, ctest 526/526).
- INC-14 **S4-fwd** (closure-param forwarding + capture-value threading
  completion): committed (539 cases / 3592 assertions, ctest 539/539).
- INC-14 **S4-generic** (generic closure literals): committed (544 cases /
  3606 assertions, ctest 544/544).
- INC-14 **S5** (Fn/FnOnce auto-traits + escaping negative contract + seq
  filter/fold): committed (552 cases / 3628 assertions, ctest 552/552).
- INC-14 **S6** (differential PBT — closure combinators ≡ builtin spreads):
  committed (553 cases / 3628 assertions).

## Recent completed changes

- INC-14 S6 (final slice; INC-14 fully landed): differential PBT closing the
  closure contract. `map(xs, |v| f(v))` element-for-element equals the builtin
  map comprehension `[f(xs)...]`, and closure `fold(xs, seed, |a,v| g(a,v))`
  equals the builtin `g(seed, xs...)` spread — over generated arrays × a
  stable of pure functions (300 trials × 2 probes = 600 probes per batch,
  INC-1 harness, replay via PBT_SEED; green across 4 seeds). Two pre-existing
  NG inference gaps surfaced while building probes (documented, worked around):
  overload candidate scoring swallows inner `infer()` errors (comprehension in
  argument position → cryptic `no matching function specialization`; bind the
  comprehension to a local first), and unannotated `[]` cannot infer its
  element type (annotate `: array<i64>`).

- INC-14 S5: three-part slice. ① **seq.filter/fold library-ized**
  (`lib/std/seq.ng`): `filter<T>(values, pred: |T| -> bool)` keeps accepted
  elements, `fold<T, U>(values, initial: U, f: |U, T| -> U)` left-folds the
  accumulator — reuse of S2c/S3/S4 closure machinery, no new compiler code;
  forwarding wrappers and captured accumulators/offsets run end to end.
  ② **Fn/FnOnce minimal auto-trait surface** (`prelude.ng` + `hasImpl`):
  `export auto trait Fn {}` / `FnOnce {}` markers satisfied **only by closure
  types** (vs Send-style universal auto traits); `where T: Fn(Once)` generic
  bounds accept closures and reject non-closures (`does not satisfy its where
  clause`). ③ **Non-escaping negative contract**: `checkFunction` rejects a
  closure-typed declared return with `function X returns a closure, which
  cannot escape its scope (non-escaping closures, INC-14)` — previously a
  no-capture closure return compiled (bare code pointer) and a captured one
  referenced dead locals; nested `|x| |y| x` literals also rejected. Tests +8
  (52 closure cases); `example/closures.ng` + forwardFold/runBounded + 5
  invocations; guards held. Recorded gaps: signature `<T: X>` trait bounds are
  not call-site enforced (same for `Copy`/`Send`, only `where T: X` is
  checked); closure params immutable in bodies; closures in aggregates/views
  still rejected via legacy `unsupported type form`.

- INC-14 S4-generic: fully generic closure literals in generic function bodies
  (`let f: |T| -> T = |v: T| v; f(x)`, forwarded into higher-order calls,
  capturing type-parameter locals) now land end-to-end. Per user direction,
  **no stubbed/skipped lowering**: a generic closure is model-typed only — no
  code function exists until the enclosing generic is instantiated, then the
  closure re-infers with concrete types and a real lowered `__closureN` is
  synthesized. Two fixes: (a) removed the two `closure parameters require
  concrete types` gates (`inferExpected` + `infer`); (b) `cloneType` now clones
  closure `parameters`/`result` (was the latent "generics incomplete" bug —
  closure-typed `let`s never instantiated in generic bodies). Registration now
  marks **all generic templates** as placeholders (model-checked, not lowered),
  which eliminated dead TypeParameter model closures from the SSA and made the
  deferred closure-call path cover generic closure locals
  (`deferredClosures_`/`deferredClosureLocals_`). Generic closure bodies follow
  the generic-function model: op-free passes, ops on type params error at model
  time. Tests +5 (44 closure cases); `example/closures.ng` +3 invocations;
  guards held.

- INC-14 S4-fwd: closure-typed **parameters** can now be forwarded into another
  higher-order call (`mapClosure<T>(values, f) { map(values, f) }`). The
  `inferExpected` generic arg loop now records closureTargets for bound
  closure-typed locals (mirrors the `infer` Call-case), closing the gap where
  the inner `map` instance was built with `closureTargets={}` and its `f(x)`
  threw `closure value f was not created directly by a closure literal`. This
  also surfaced and fixed an **S3 runtime gap**: capturing closures passed to a
  higher-order call never filled `callCaptures_` (deferred-literal loop and
  `inferExpected` literal branch recorded targets but not capture values), so an
  instance with a capture param received `(values, f)` — arg-count mismatch that
  crashed (`map([1,2],[factor])`). Both now fill `callCaptures_` from the
  literal's `closureCaptures`, guarded by the `callTargets` first-inference
  idempotency check. Forwarding a current-instance closure **param** now threads
  the instance's own capture params (`instanceCaptureParamsByLocal_`) before
  falling back to `closureCapturesByLocal_`. 4-level chains and multi-param
  with-capture forwarding verified; tests +6 (38 closure cases).
- INC-14 S3: explicit capture lists `[x, move y] |p| body`. Resolver resolves
  capture names to enclosing-local ids (`CaptureInfo{local, isMove}`) and shadows
  them with fresh closure-body-scope locals (`closureCaptureParams`), so bodies
  read capture params. `inferClosure` types capture params from enclosing locals,
  checks the body, then marks move captures (whole value). `validateCaptures`
  replaced `ensureNoCapturedLocals` (uncaptured enclosing local → clean error).
  Code functions `__closureN` prepend capture params; FlowIR prepends capture
  values as leading operands (synthetic ResolvedName Evaluate) at direct closure
  calls; `instantiateFunction` prepends capture groups to higher-order instance
  signatures and re-binds the instance's closure call via
  `instanceCaptureParamsByLocal_`. Fixed the S3 blocker: overload resolution
  re-infers a call once per candidate (15× for `print(apply(f,5,7))`), so the
  higher-order capture record is now idempotent (first-inference only) —
  `apply(f,5,7)` = 112, `apply(cap2,2,0)` = 50, copy array = 2, move array = 7.
  Tests `test/closure_ho_test.cpp` +7 (closure suites 25→32);
  `example/closures.ng` +6 capture groups; guard flowir 2202→2203.
- INC-14 S2a+S2b: `parsePrefix` routes `OrOr` to `parseClosure` as an empty head
  (`|| expr`); new `ClosureTypeSyntax` (`|T| -> R`) → `hir::TypeKind::Closure` →
  `internClosure`; closure parameter annotations may be omitted and are inferred
  from the expected closure type (`inferExpected` Closure branch, factored
  `Checker::inferClosure`, records the closure expression type — previously an
  `unordered_map::at` crash). No-expected-type untyped params → clean error;
  result mismatch → clean error. FlowIR now wires the function-body tail value
  into the fallthrough Return (latent bugfix: bare `ret` previously returned a
  leftover QBE register; `|| 7` exposed it). Guard ceilings raised: flowir
  2177→2185, ast.hpp 938→955 (justified). Tests 12→15 closure cases (+2 S2
  runtime +3 negative/S2); `example/closures.ng` extended.
- INC-14 S1: `ClosureExpression` AST; resolver closure params/body; `TypeKind::Closure`;
  per-literal synthesized code function (`__closureN`, body cloned + param-local
  remapped, re-checked via `instances_`); calls reuse `callTargets`; FlowIR direct-call
  lowering with the callee local operand skipped; closure literal lowers to an inert
  zero word; `qtypeOf` Closure → Long. S1 contracts: explicit param types required,
  captures rejected (negative contract until S3). Also fixed
  `BlockParser::parseExpressionUntilAny` missing its `braceDepth` guard on the primary
  terminator break (truncated block-bodied closure let initializers). Guard ceilings
  raised in `test/guard_test.cpp`: flowir 2149→2177, ast.hpp 925→938 (justified).
  Tests: `test/closure_test.cpp` (12 cases) + `example/closures.ng`.
- INC-11: `export trait Show` in `lib/std/prelude.ng`; derive validator + `toString`
  native dispatch; FlowIR `buildShowString` field-level synthesis; guard 2075→2150.
- ASan imgui link fix: `-fno-sanitize=address,undefined` on `ngrt_imgui` + vendored
  `imgui` (plain `cc` links generated executables without the sanitizer runtime).

## Next steps

- INC-14 **S3 follow-up (done)**: move-capture consumption enforced on the
  original — `inferClosure` marks move-captured locals moved AFTER its
  `checkFunction(instance)` (which resets `moveState_`), so direct
  `let z = a` after `[move a]` is rejected. 3 tests added.
- INC-14 **S3 known gap**: consumption enforcement does not survive a
  higher-order use of the closure (`apply(h, x)` re-enters `checkFunction` via
  `instantiateFunction`, wiping `moveState_`). Fixing this correctly requires
  making `checkFunction` save/restore `moveState_`/`borrowState_` across
  re-entrancy — a broad change that exposes latent move-use bugs in
  `example/hkt.ng` (moves affine value through `unwrap` then reuses it) and
  `example/derive.ng` (`derive(Copy)` types still treated as affine / moved).
  Deferred; revisit together with Copy-semantics teaching the move checker.
- INC-14 **S4** (fully generic closures), then S5 (Fn traits), S6
  (differential PBT seeding — map vs map-spread). Note: the plan's original S4
  "move captures + env drop" was subsumed by S3's static threading; the next
  slice is fully generic closure literals.
- INC-14 **S6** (differential PBT seeding — `map(xs, |x| f(x))` vs map/fold
  spreads on generated arrays) — **done**. All INC-14 slices (S1–S6) landed.
- INC-12 (combinators + law PBT), then INC-13 `?` desugar per plan order.

## References

- `.agents/worklog/2026-08-27.md` — INC-B / INC-A / INC-9 / INC-10 entries
- `.agents/worklog/2026-08-30.md` — INC-11 entry
- `.agents/worklog/2026-08-31.md` — INC-14 S1 entry + S2a+S2b entry + S2c entry
- `.agents/worklog/2026-09-01.md` — INC-14 S3 entry
- `.agents/worklog/2026-09-07.md` — INC-14 S4-fwd (closure-param forwarding), S4-generic, S5 entries
- `docs/design/rearchitecture/09-increment-plan.md` — execution table + INC-14 slices
- `AGENTS.md` — pipeline/build/test/commit conventions