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
- INC-12 (prelude Option<T>/Result<T,E> + opt/result combinators + parseInt
  Result migration + law PBT + contract example): committed (554 cases /
  3634 assertions, ctest 555/555).
- INC-13 (`?` try-operator desugar): committed (560 cases / 3662 assertions,
  ctest 561/561).
- INC-15 (origin contracts, D-002 A1.1): implemented, verified, docs-synced;
  committed (570 cases / 3672 assertions, ctest 571/571).
- INC-16 (refs into aggregates + cross-call borrows, D-002 A1.2/3):
  implemented, verified, docs-synced; committed (598 cases / 3694 assertions,
  ctest 599/599).
- INC-17 (borrow-capturing closures, M2b): implemented, verified,
  docs-synced; committed `eeea445`/`4aab98c`/`5ab9ef6` (`[INC-17]` 11 cases,
  focused suite 193/193 / 801 assertions, full 609/609 / 3705 assertions,
  ctest 610/610).
- INC-18 (exit-time bump arena, 过渡卫生层): implemented; arena probe
  (`test/probes/arena_all.ng`) + `test/arena_test.cpp` `[INC-18]` 4 cases
  green incl. leaks(1) 0/0; **full 613/613 (3726 assertions), ASan 613/613,
  ctest confirm before commit**.

## Recent completed changes

- INC-18 (exit-time bump arena): all program value allocations now bump a
  per-process mmap anonymous arena (`NgrtArenaBlock` chain, 1 MiB blocks, 16B
  alignment, atexit OS return). `ngrt_alloc`/`ngrt_new_string` bump;
  `ngrt_free` dispatches by pointer origin (arena no-op / malloc scratch free);
  `ngrt_release` reduced to logical live-table clearing. All generated `$malloc`
  removed: helpers `str.ssa`/`arr.ssa` (6 sites) + 13 lowering emission points →
  `$ngrt_alloc`; libngrt value-return paths (ngrt_str_case/replace/join/reverse/
  split headers/ngrt_allocate cell) → ngrt_alloc/arena new_string. Kept-malloc
  scratch: regex `ngrt_c_string`, readFile/systemOutput buffers. Fix
  `_DARWIN_C_SOURCE` restores MAP_ANONYMOUS under `_POSIX_C_SOURCE` (+ MAP_ANON
  fallback). **CTS caught the helpers→`$ngrt_alloc` link side-effect**: the
  `native_qbe_test` link lines (helpers archive only) started failing 20 cases;
  both now append the `NG_NGRT_PATH` archive (`[Qbe]` 47 cases green). macOS
  leaks(1): concat probe (flipped contract in `test/leak_probe_test.cpp`,
  19999→0) and arena_all probe both 0 leaks/0 bytes (footprint peak 194.5M,
  in-run growth documented: blocks are returned only at exit per the DEC-C
  scaffold limitation). Host stats surface `ngrt_arena_blocks()`/
  `ngrt_arena_bytes()` pinned by `test/arena_test.cpp` (alloc/free dispatch
  incl. malloc sentinel, new_string layout, block-crossing throughput
  200k×64B, release idempotence/reuse). **Full 613/613 (3726 assertions),
  ASan 613/613.**

- INC-17 (borrow-capturing closures, M2b): multi-bearer `Loan.bearers`
  (release at the last surviving bearer's last use), paired with the
  `checkFunction` save/restore fix that un-masked `example/hkt.ng`,
  `example/option_result.ng`, and the nested-call double-inference bug now
  fixed in `argumentTypeAt`. Unused capturing closures release their loan at
  declaration. `isAffine` Copy-aware via `derive(Copy)`. Corpora + E2E:
  `test/borrow_capture_test.cpp` (11 `[INC-17]` cases, incl. ref-mut write-
  through runtime) + `example/borrow_capture.ng` (6 true prints, exit 0).
  Committed `eeea445`/`4aab98c`/`5ab9ef6`; focused 193/193 (801 assertions),
  full 609/609 (3705 assertions), ctest 610/610.

- INC-16: move/assign-vs-borrow interplay, place-through-ref, and aggregate
  permission rules (D-002 A1.2/3). S1: checker gates `isBorrowed` +
  `placeName` in `trackConsumption` (move prefix / Member / ResolvedName) and
  the `Assign` handling fire `E-borrow-conflict` while a loan lives; NLL
  positives (last-use release at statement grain) stay green. S2: `T ref mut`
  params are mutable sub-target roots; Index/Member on a reference receiver
  read/write directly (`items[i]`, `b.value`); reads deref first via
  `LoadRef` (copy-first preserved); Field ordinals resolve through the
  referenced descriptor; ref-through-ref and slicing are deferred A1.3;
  immutable-ref writes and ref-binding reassignment are rejected. S3:
  Reference fields rejected at both struct-instance build sites; enum payload
  refs (recursive `List<T>`) remain the sanctioned exception. Corpora +
  E2E: `test/nll_problem_cases_test.cpp`, `test/place_through_ref_test.cpp`,
  `test/ref_permissions_test.cpp`, `example/place_through_ref.ng`. Full suite
  598/598 (3694 assertions), ctest 599/599; guard flowir.cpp 2203 → 2256.

- INC-13: HIR-side desugar of the `?` try operator (D-007) into switch +
  early-return. New `TryExpression` syntax node (ast.hpp 966 → 977 guard
  note); parser disambiguates `?` by lookahead — `f(xs)?...` stays the
  map-spread filter marker, any other `?` becomes the try operator.
  `Resolver::desugarTry` restructures `let x = expr?;` (and bare `expr?;`)
  into `let $tryN = expr; switch ($tryN) { case Ok($okN) { let x = $okN;
  <rest> [tail] } case Err($errN) { return Result.Err($errN); } }`, producing
  only pre-existing let/switch/return nodes so the checker's three drop-edge
  tables and FlowIR lowering apply unchanged (zero backend changes).
  Function-body tails move into the Ok arm as explicit returns; if/loop-branch
  semantics unchanged. Differential PBT (`test/try_operator_test.cpp`, INC-1
  harness): 2/3-step chains + drop-edge variant, `?` ≡ hand-written expansion,
  60 trials × 3 probes, green across 4 seeds. Negative contracts (E-resolve
  messages): non-Result return (i64/unit), `return <expr>?`, mid-expression,
  closure body, destructuring let. Contract example `example/try_operator.ng`
  (parse → validate → compute chain, 4 true assertions). Full suite 560/560
  (3662 assertions), ctest 561/561.

- INC-12: prelude generic `Option<T>`/`Result<T,E>` enums plus combinator
  family `optMap/optAndThen/optOrElse` and `resultMap/resultAndThen/
  resultOrElse` (closure-typed transform arguments; pure library on the
  INC-14 closure machinery, no new compiler code). `parseInt` migrated to
  `Result<i64, string>` (Err payload = the offending text); legacy
  `OptionalInt` removed and all call sites updated (`example/int_string.ng`,
  `test/string_conversion_test.cpp`, trap note). Laws tested by PBT
  (`test/option_result_test.cpp`, INC-1 harness, 60 trials × 8 probes):
  Option identity, Option/Result composition on both Some/None and Ok/Err,
  Option orElse default + keep-on-Some; contract example
  `example/option_result.ng` prints 8 true assertions. **Names deviated from
  the plan (map/andThen/orElse)** because seq.map already owns `map` and
  cross-module same-name generic overloads silently select one candidate
  (`map(xs, ...)` with a local Option-map present dies with `expected Opt,
  got array<i64>`); documented in the plan. Newly surfaced pre-existing gaps
  recorded: (a) a non-generic function with a concrete generic-enum param
  (`fun f(o: Opt<i64>)`) is uncallable (`no matching function
  specialization`); (b) nullary/assorted enum constructors in argument/switch
  positions lack expected-type inference (bind through an annotated `let`
  first); (c) no `derive(Eq)` for generic enums, so law equality reduces to
  nested-switch → i64 compare; (d) block-closure bodies use a tail expression,
  not `return`.

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
- INC-14 **S3 known gap**: consumption enforcement did not survive a
  higher-order use of the closure (`apply(h, x)` re-enters `checkFunction` via
  `instantiateFunction`, wiping `moveState_`). **FIXED in INC-17** — the
  multi-bearer change made `checkFunction` save/restore
  `moveState_`/`borrowState_`/transient state across re-entrancy, which exposed
  and required the `example/hkt.ng` (`ref back`) and `example/option_result.ng`
  (second `Option.None` use) fixes, plus the nested-call double-inference fix
  in `argumentTypeAt`. Copy-semantics teaching of the move checker shipped via
  `isAffine` Copy-awareness (`derive(Copy)`).
- INC-14 **S4** (fully generic closures), then S5 (Fn traits), S6
  (differential PBT seeding — map vs map-spread). Note: the plan's original S4
  "move captures + env drop" was subsumed by S3's static threading; the next
  slice is fully generic closure literals.
- INC-14 **S6** (differential PBT seeding — `map(xs, |x| f(x))` vs map/fold
  spreads on generated arrays) — **done**. All INC-14 slices (S1–S6) landed.
- INC-12 (combinators + law PBT) — **done**.
- INC-13 (`?` try-operator desugar) — **done** (see Recent completed changes
  and worklog 2026-09-08). INC-13 boundary documented: `?` limited to let/
  statement roots; mid-expression, `return <expr>?`, closure bodies, and
  destructuring lets are explicitly rejected for later slices.
- INC-15 (A1 origin contracts, `T ref(a)` → param ordinals, return-path
  checks) — **done** (see Recent completed changes and worklog 2026-09-08).
  INC-15 boundary: only returned references with declared origins are admitted;
  ref fields / cross-call borrows (`array<T> ref` direct indexing,
  ref-in-aggregate) are INC-16.
- INC-16 (A1.2/3: references into aggregates + cross-call borrows) — **done**
  (see Recent completed changes and worklog 2026-09-10). INC-16 boundary:
  ref-in-aggregate permission rules landed (S3); the reference-through-
  reference / slicing boundary (A1.3) and the closure-capture variant of the
  corpus were carried to INC-17 (M2b).
- INC-17 (M2b borrow-capturing closures) — **done** (`eeea445`/`4aab98c`/
  `5ab9ef6`; see Recent completed changes and worklog 2026-09-10).
- INC-18 (exit-time bump arena) — **implemented**: verification running
  (focused + full suite + ctest + leaks(1) 0/0 confirmed), docs-synced,
  commit (feat/docs/chore) pending confirmation, after which **next** per the
  increment plan: Phase 6 堆域线 INC-19 (generic `Box<T>` + `new`, DEC-C
  RC/CoW storage — arena 拆除时点随此落地).

## References

- `.agents/worklog/2026-08-27.md` — INC-B / INC-A / INC-9 / INC-10 entries
- `.agents/worklog/2026-08-30.md` — INC-11 entry
- `.agents/worklog/2026-08-31.md` — INC-14 S1 entry + S2a+S2b entry + S2c entry
- `.agents/worklog/2026-09-01.md` — INC-14 S3 entry
- `.agents/worklog/2026-09-07.md` — INC-14 S4-fwd (closure-param forwarding), S4-generic, S5, S6 entries
- `.agents/worklog/2026-09-08.md` — INC-12 + INC-13 entries
- `docs/design/rearchitecture/09-increment-plan.md` — execution table + INC-14 slices
- `AGENTS.md` — pipeline/build/test/commit conventions