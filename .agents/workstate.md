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
  ctest 614/614**; committed `a29604f`/`248cf30`/`a372728`.
- INC-19 **S1** (generic `Box<T>` + `new`, DEC-C B1 core): implemented;
  `test/box_test.cpp` `[Box]` 6 cases + `example/box.ng` sweep + `[Box]` full
  **620/620 (3749 assertions), ctest 621/621** (per-case registration, +7 box
  cases). Old single-typed memory natives removed → arena probe + one stdlib
  fixture migrated to explicit-generic native calls. Committed
  `c36c53a`/`c03565f`/`d311f84`.

- INC-20a (affine-enum switch consumption, keystone for Box-in-enum migration):
  `checkSwitch` calls `consumeSwitchScrutinee` before branches when scrutinee
  type is affine → `moveState_.isWholeMoved` suppresses scope-end double-drop.
  Verified by probe: switch over affine enum with Box payload no longer double-
  releases. ng_test ~[QBE] 573/573 (3558 assertions), ctest 621/621 (unchanged
  baseline). Committed `1b09a64`.
- INC-20b (`Box.clone()` real deep copy — option (a) of the INC-20 blocker):
  **committed** `3c69e98`/`2b73209`/`f237e12`. Clone
  borrows the receiver (original stays live), deep-copies payload into a fresh
  cell via `emitCloneDeep`; top-level Box copies re-envelope on heap
  (`emitBoxEnvelopeCopy`, $ngrt_alloc(8) + handle copy) — fixes the
  "invalid heap handle" crash from returning a stack-allocated QBE box
  envelope; `list_spread.ng` unblocked by routing enum clone ladders through
  `$ng_clone{id}_helper` (`cloneHelperStream_` gate, avoids inline ladder
  predecessors-not-matched-in-phi). Typechecker tracks `boxCloneCalls`
  (`isBoxReceiver`); FlowIR emits BoxClone/LoadRef. Full **623/623 (3758
  assertions), ASan box/clone probes clean**.
- INC-20c **S0** (Box-in-aggregate fixes + Box-list feasibility): two memory
  bugs fixed and committed as `11bcc48` (fix). (1) Enum/struct literal payload
  args are consumption-tracked (`inferExpectedEnum`/`inferExpectedStruct`) —
  fixes the double `ngrt_release` crash when a box moved into `Pair.Cons(1, t)`
  is read again via a switch pattern. (2) Box-typed locals always bind fresh
  heap cells (`collectLocals` seeds `cellLocals_`) — fixes stale aggregate
  payload tokens when a whole-moved Box local fell to the stack-slot fast path
  (in-process `boxrec.ng` regression). `[enum-resolve]`/`[enum-spec]` tracing
  proved the toolchain's "expected List, got List" on `new<List<T>>(tailLocal)`
  was a probe typing error (`listLength<i64>(…)`, not `<List<i64>>`); var
  reassignment is `:=`, not `=`. **Full Box-list semantics validated**: a
  production-shaped `list.ng` probe (`Cons(head, tail: Box<List<T>>)`, fully
  implicit inference — length/get/contains/listFrom/append/reverse/pushFront)
  exits 0. Suite 623/623 (3758 assertions), ASan clean.
- INC-20c **S1** (std.list on Box nodes): **migrated** `lib/std/list.ng` to
  `Cons(head: T, tail: Box<List<T>>)`, list contracts end-to-end green.
  Two typecheck joins (`isListTail`, list-literal target + spread source),
  FlowIR spread element typing accepts Box tails (flowir at the 2256 guard),
  and `lowering_expr.cpp` MakeRef-with-Box-result now builds real boxes —
  registered cell (`$ngrt_allocate`) + 16B envelope wrapping the existing
  node word (no deep copy) — fixing the literal-built "invalid heap handle"
  crash (unregistered `$ngrt_alloc` value cells are rejected by
  `ngrt_load/release`). `new`-built box probes clone/read/switch correctly.
  **Accepted (documented)**: builders (`listFrom`/`pushFront`/`append`/
  `reverseList`) are **O(n²) allocations** — `new`/`read` deep-copy the
  whole remaining subtree (incl. tail boxes) at each recursion level
  (measured: new(k-node)=k+1, read(k-node)=k; listFrom 1..4 → 3/8/15/24
  cells). Box VALUE copies (move/field-store/param-by-value) are shallow
  and share the pointee; deep copy happens only in `clone()` and at native
  boundaries (`new`/`read`). Full **623/623 (3758 assertions)**; example
  sweep + `stdlib_redesign` list cases green. Guard flowir 2256/2256 (at
  ceiling).
- INC-20c **S2** (list spreads through std helpers): **migrated** list array-
  spreads off dedicated IR: removed `EnumListLength`/`EnumListGet`
  (flowir.hpp), the `$ngrt_enum_list_*` helpers (enum.ssa deleted, CMake
  updated), and the two lowering cases. The checker now resolves a side-table
  `arrayListSpreadCalls` (spread node → `spreadLength`/`spreadGet` instances —
  private value-taking NG wrappers in list.ng that delegate to `length`/`get`,
  so flowir passes the list VALUE with no MakeRef machinery) via
  `resolveListSpreadCall` (name + param0-unify + `instantiateFunction`); flowir
  emits `Evaluate Call` for both the bound and per-element reads. Guard flowir
  2256 → **2262**. Non-std chain-enum spreads (structurally Cons/Nil+Box) now
  fail cleanly ("requires the std.list List type"). +1 IR-absence test
  (`--emit=ssa` excludes `ngrt_enum_list`). Full **624/624 (3761 assertions)**;
  probe exits 7, custom-chain probe errors cleanly.
- INC-21 **S1** (span\<T\> borrowed scoped view): implemented + verified — surface
  per the change plan below (whole `span xs` / window `span xs[lo..hi]`,
  bounds-checked element reads, shared-loan borrows, non-returnable/non-stowable/
  read-only, `span<T>`/`T span` types); runtime value = 16-byte `{data, len}` heap
  blob via `$ngrt_span_make`/`$ngrt_span_get` (span.ssa, bounds → `hlt`). Guards
  bumped (flowir 2262 → **2302**; ast.hpp 980 → **991**). Verified: `[Span]`
  51 assertions, `[Guard]`, `~[Examples]` 601/601, `[Examples]` 34/34, example
  `span.ng` exit 15 (whole/window/generic). Borrow integration: span loans retie
  to the binding like refs (shared-borrow/mutable conflict); OOB window
  construction + OOB reads → "killed by a signal".

## Recent completed changes

- INC-20c S2 (list spreads route through std helpers): removed
  `EnumListLength`/`EnumListGet` FlowIR opcodes, `$ngrt_enum_list_len/_get`
  helpers (enum.ssa deleted + CMake), and their lowering cases. Checker
  side-table `arrayListSpreadCalls` maps a spread node to `spreadLength`/
  `spreadGet` instances (private value-taking wrappers over std `length`/`get`,
  resolved by name + param0-unify + `instantiateFunction`); flowir lowers both
  spread emissions as `Evaluate Call` — the value ABI avoids the MakeRef
  plumbing the ref-taking helpers would need. Guard 2256→2262; non-std chain-
  enum spreads now fail cleanly. Verified: `[Stdlib]`/`[Guard]`, full suite
  624/624 (3761 assertions), probe exit 0. Committed (feat/docs/chore trio).

- INC-20c S0 (Box-in-aggregate fixes, foundation for std.list on Box nodes):
  two memory bugs. ① Enum/struct literal payload arguments were not
  consumption-tracked, so `let p: Pair = Pair.Cons(1, t)` left `t`'s scope-end
  drop edge alive and the switch-arm return path released the same cell twice
  (`invalid heap handle`) when a pattern-bound `b` shared it. `trackConsumption`
  now runs on every payload arg in `inferExpectedEnum` (tuple + single-payload
  branches) and `inferExpectedStruct`. ② Whole-moved Box locals then flipped to
  stack slots; a box VALUE is an address to an 8-byte cell holding the handle,
  and a stack slot is reused once the binding dies, dangling aggregate payload
  tokens (in-process `boxrec.ng` wrong reads). `collectLocals()` forces every
  Box-typed local into `cellLocals_`. Verified: `boxpattern.ng`/`boxpattern3.ng`
  exit 0, `boxm2.ng` E-move-used, `boxrec.ng` exit 0 in-process, ASan probes
  clean, full suite **623/623 (3758 assertions)**. Also proved (via
  `[enum-resolve]`/`[enum-spec]` interner tracing) that `new<List<i64>>(list)`
  works — earlier "expected List, got List" was a probe mis-typing
  (`listLength<i64>` vs `listLength<List<i64>>`), and `:=` is the block/switch/
  loop-body reassignment operator. **Box-list feasibility probe exits 0**:
  production-shaped std.list on `Cons(head, tail: Box<List<T>>)` with fully
  implicit inference (length/get/contains/listFrom/append/reverse/pushFront).
  See worklog 2026-09-15. Committed `11bcc48` (fix).

- INC-20b (Box.clone deep copy, INC-20 unblocker option (a)): full root-cause
  chain landed. **Crash fix**: `new()`'s box literal lowered to a *stack*
  envelope (QBE `alloc8 8` is alloca); the old deep-clone-on-return heap-
  lifted it, but the clone box-shortcut `return source` kept the stack pointer,
  so the cell's stored `box` and the drop's `loadl(box+0)` read freed/reused
  stack (e.g. `0x10052c050` = cell1's own address) → `ngrt_release(invalid
  handle)`. `emitClone` top-level Box copies now `emitBoxEnvelopeCopy`: fresh
  8-byte heap envelope (`$ngrt_alloc(8)`) + handle copy (shares the cell →
  drop-exactly-once; envelopes arena-owned, never released). **phi crash fix**:
  `list_spread.ng` `qbe: predecessors not matched in phi %t279` — inline enum
  tag ladders split structured blocks (every QBE label is a block boundary);
  the ladder's unterminated last block fell through to the next structured
  label. Enum deep clones now always call `$ng_clone{id}_helper` from user
  bodies (`cloneHelperStream_` null → delegate + register type); the ladder
  inlines only inside the helper (no structured phis). Composite payloads
  (struct/tuple/enum with Box fields) deep-copy into fresh cells via
  `emitCloneDeep` (`$ng_clone{id}_helper` recursion) — "cell never aliases
  payload". Box is affine (`let b = a` = E-move-used), so shared-handle
  envelopes never double-release. Verified: `example/box.ng` (+clone section,
  exit 0), `/tmp/p13.ng`/`/tmp/p14.ng` recursive/leaf clone exit 0,
  `list_spread.ng` exit 0. 3 new box_test `[Box][INC20]` cases (deep-copy +
  write isolation, `outstanding()` balance through a helper, recursive
  `Box<E>` enum clone walk). **Full 623/623 (3758 assertions), ASan probe
  batch clean**; guard flowir 2256 (at ceiling).

- INC-19 S1 (generic `Box<T>` + `new`): `lib/std/memory.ng` rewritten to a
  generic affine `Box<T>` over the session-owned handle table —
  `export impl<T> Drop for Box<T>` releases the cell drop-exactly-once;
  `new`/`box`/`read`/`write` call the generic natives with **explicit**
  `allocate<T>`/`load<T>`/`store<T>` (cross-callee type-parameter inference
  is unsupported — probed, documented). Checker landed: Struct unify arm in
  `unifyGuarded`, generic-Drop registration via `matchImplPattern` +
  `instantiateFunction`, `dropEdge`/`hasDrop` replacing the `dropTypes_`
  guards, and instance signatures recorded in `functionTypeIds_` (FlowIR
  drop-call typing — the S1 blocker). Word payloads i64/string/array/struct/
  enum/tuple plus f64/f32 bit-preserving round-trip (whole-word store/reload);
  no aliasing (deep copy in/out). Old single-typed memory natives removed:
  `test/probes/arena_all.ng` and a stdlib fixture migrated to explicit
  generic-native calls. Corpora + E2E: `test/box_test.cpp` (6 `[Box]` cases,
  incl. drop-exactly-once `outstanding()` accounting, affine-move negative
  contract, and f64/f32/word round-trips) + `example/box.ng` (sweep, exit 0
  with per-line markers). Full **620/620 (3749 assertions), ctest 621/621**.

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
- INC-18 (exit-time bump arena) — **done** (`a29604f`/`248cf30`/`a372728`;
  see Recent completed changes and worklog 2026-09-10).
- INC-19 (generic `Box<T>` + `new`, DEC-C B1 core) — **S1 landed**
  (`c36c53a`/`c03565f`/`d311f84`); ASan + leaks(1) rerun pending but baseline
  green (620/620, 3749 assertions, ctest 621/621). INC-20 **unblocked** by
  INC-20b (`Box.clone`, option (a)). Next: INC-20c (std.list on Box nodes,
  remove FlowIR List opcodes).
- INC-19 S1 **known limits** (documented): no Gc/Arc/allocator params (R6);
  generic call inference cannot pass a type parameter as an implicit arg
  across a callee boundary — `new`/`read`/`write` spell `allocate<T>`/
  `load<T>`/`store<T>` explicitly; floats are bit-preservation only until the
  DEC-A checked-numeric redesign.
- INC-20a **keystone landed** (`1b09a64`): `consumeSwitchScrutinee` in
  `checkSwitch` marks affine enum scrutinees moved before branches — required
  by any future Box-in-enum design. **INC-20b landed** (`3c69e98`/`2b73209`/
  `f237e12`): `Box.clone` deep copy. **INC-20c S0 landed** (`11bcc48`): Box
  aggregate-move/drop fixes + full Box-list semantics validated by probe.
  **INC-20c S1 landed**: `lib/std/list.ng` on Box nodes (typecheck/fix +
  lowering box-wrap), list contracts green. **S2 landed**: removed
  `EnumListLength`/`EnumListGet` + `$ngrt_enum_list_*` helpers by routing
  spread through std list wrappers (`spreadLength`/`spreadGet` instances via a
  checker side-table); guard 2262; full 624/624. Doc-sync + commit trio.

## INC-20c S1 change plan (EXECUTED 2026-09-16; see execution log below)

**De-risking discovery (verified against SSA + lowering sources):** the feared
"no flowir-level way to build a box envelope" is a non-issue. Three facts make
S1 a *small* change:

1. `emitRefObject` (src/native/lowering_aggregate.cpp:260) for a heap-cell local
   snapshots `loadl(localSlots_[rootLocal])` into ref-object field0. In
   `lowerListLiteral` the acc local is a heap cell whose value *is* the
   value-slot pointer, so each MakeRef emission is `{valueSlotPtr, 0}` with
   field0 == the box handle convention (a cell address). Walker:
   `loadl(payload+24)` → field0 → `loadl(field0)` → node = **2 loads,
   byte-identical for the ref layout and the box layout**.
2. `enum.ssa` `$ngrt_enum_list_len/_get` only touch payload+24 / field0 /
   deref — the walk needs **no change**.
3. `new<T>(rest)` produces an 8-byte envelope `{handle=valueCell}` — same
   field0 convention. User-built and literal-built boxes unify on it.

The literal machinery is descriptor-driven, so `lowerListLiteral`'s instruction
stream (flowir.cpp:733) types the refAcc from `descriptor.elements[consVariant]
.elements[1]` — once `Cons` declares `tail: Box<List<T>>` the MakeRef result
carries the Box type automatically. **No new instruction, no enum.ssa edit.**

**Challenge outcome (2026-09-16):** the draft's flagged "double-release hazard in
string-element clone ladders" was a **false positive**. The shallow tail in
`$ng_clone120_helper`'s emitted SSA came from the *current ref build* — a Ref
tail falls through `emitCloneDeep` to `return source` (lowering_aggregate.cpp:504).
`emitCloneDeep` already owns a dedicated **Box branch** (lines 371-386): fresh
cell (`$ngrt_allocate`) + fresh envelope (`$ngrt_alloc(8)`) + deep recursion via
`$ng_clone{id}_helper`. With Box tails the field takes that branch → deep copy,
no alias, no double release. **No clone-ladder edit needed**; instead a
regression probe pins the behavior (below).

**Edit set (ordered):**

- **Fix 0 — pre-flight probes (before code edits):**
  - Probe A: inline enum `Cons(head: i64, tail: Box<ListP<T>>)` + `[1,2,3]`
    literal + switch/read traversal + `[...xs]` spread → exit 0. Validates the
    literal→Box runtime path (A/B/C mechanics) independent of std.
  - Probe B: `listFrom(["a","b"])`-shaped Box list + deep clone + drop →
    `outstanding()` stays balanced (pins the Box branch of `emitCloneDeep` once
    tails are Box; the shallow-ref SSA warning is gone).
- **Fix A — typecheck_infer.cpp:486** (list-collection-literal target): accept
  a Box tail alongside the ref tail. Extend the Cons-field1 condition with
  `|| (isBox(field1) && interner_.descriptor(field1).typeArguments[0] == expected)`.
- **Fix B — typecheck_infer.cpp:977** (spread source detection): same Box
  acceptance, `isBox(field1) && typeArguments[0] == source` fills `listSource`/`listElement`.
- **Fix C — flowir.cpp:944-948** (spread element typing, EnumListGet branch):
  currently only matches `payload.elements[1].kind == Reference`; with Box tails
  it matches nothing → `element` stays untyped → later
  `function_.valueTypes.at(element.value)` throws. Extend to accept
  Struct-Box-of-source; net lines ≈ 0 (simplify by reading the found variant
  directly).
- **Fix D — lib/std/list.ng** (the migration): `Cons(head: T, tail: Box<List<T>>)`.
  `length/get/contains(list: ref<List<T>>)` pattern-match `*list`, recurse by
  `let boxed: List<T> = read<List<T>>(ref rest); ... ref boxed` — probe
  `listboxprobe3.ng` shape, fully implicit inference (validated exits 0).
  Builders (`listFromInner/pushFront/append/reverseList`) return into
  `new(rest)` boxes. ~91-line module, keeps the exported surface identical
  (`length/get/contains/listFrom/pushFront/append/reverseList`).
- **Fix E — corpus migration:**
  - `test/stdlib_redesign_test.cpp:84-87`: `List.Cons(1, ref empty)` →
    `List.Cons(1, new<List<i64>>(empty))` (each `new` affine-consumes the tail
    value; chain stays move-valid — probe-verified pattern).
  - `example/std_list.ng:8-11`: same rewiring.
  - `list_builders.ng` / `list_literals.ng` / `list_spread.ng` + test strings at
    lines 99–102, 161–163, 180–183: no source change expected (function/literal/
    spread based); the spread cases only activate Fixes B/C.
  - `lib/std/list.ng`'s `List.Cons` callers elsewhere: audit
    `rg "List.Cons|ref empty" example lib test` for stragglers.

**Verification (all in one S1 execution):**
1. Apply A/B/C alone → full suite `./build/ng_test` still 623/623 (conditions
   are joins, no behavior change while tails are ref).
2. Apply D+E → stdlib_redesign list cases green again (direct construction via
   `new`, literals, spreads).
3. `./build/ngci --emit=ssa example/list_literals.ng` inspect: acc MakeRef field0
   == value-slot pointer; `$ngrt_enum_list_len/_get` still walk it.
4. ASan: `box`/list probe batch clean; `stdlib_redesign` + examples sweep green.
5. Guard: flowir 2256-line ceiling — A/B/C must be net ≤ +2 lines or offset.

**Risks / accepted:**
- Literal-built boxes hold a *shallow* node word in the value-slot; correct
  because the acc is affine-consumed (single owner). Drop path releases the
  value-slot cell; the pointed-to node envelope leaks per literal node
  (same documented Box-in-enum gap, probe leaked 123 cells, exits 0).
- `new`-built vs literal-built boxes differ only in structure size (8 vs 16B);
  both satisfy field0 handling / read. No double release (probe-validated).

**Commit:** one `feat(vnext)` for A–E + doc-sync + chore worklog per the
convention. S2 (opcode/helper removal) stays separate (needs the FlowIR
line-budget plan).

### Execution log (2026-09-16, all applied & green)

Fixes A/B/C landed as planned (`isListTail` in `typecheck_infer.cpp` at both
list-literal-target and spread-source sites; FlowIR spread element loop
accepts Ref or Box tails — flowir net +6 lines, exactly **2256/2256**).

**Deviation — the planned "no lowering change" was wrong**: probe A (inline
Box-tailed enum + `[1,2,3]` literal) **compiled** (A/B/C typing correct) but
**crashed at runtime** `invalid heap handle`, exit 1. Root cause: the literal's
value-slot cells are allocated via `$ngrt_alloc` (bump arena, *not* registered
in the live table), while every box operation (`read`/`ngrt_load`, the clone
box branch, `release`) validates the handle via `ngrt_handle_valid` →
`ngrt_live_add` is only in `$ngrt_allocate`. Box handles must be **registered
cells**. Fixed in `lowering_expr.cpp` MakeRef: when the instruction result type
is a Box, emit `value=loadl(loadl(accSlot)); cell=$ngrt_allocate(value);
env=$ngrt_alloc(16); storel cell; storel 0,+8; result=env` — wraps the existing
node *word* (no deep copy), so literal tails become real boxes and the
`enum.ssa` walkers/unmodified label each build is O(n). User `ref boxLocal`
stays on the ref-object path (result type is `ref<Box>`, not `Box`).

**Deviation — builders are O(n²), accepted per user decision**: `new`/`read`
deep-copy the entire remaining subtree at every level, so `listFrom` etc.
allocate quadratically (`new(k-node)=k+1`, `read(k-node)=k`; listFrom 1..4 →
3/8/15/24). Verified Box VALUE copies (move / field-store / NG param-by-value)
are **shallow** (share pointee: Cons-store +1, param +0, return +0); deep copy
is confined to `clone()` and native boundaries (`new`/`allocate` arg, `read`/
`load` result). Wait hole in future R6 mutating builders. Probe B's strict
`outstanding()==before` assert relaxed to leak-awareness (embedded Box fields
are not recursively dropped — pre-existing documented gap).

## INC-20c S2 change plan (EXECUTED 2026-09-16)

**Goal:** remove `EnumListLength`/`EnumListGet` + `$ngrt_enum_list_*` helpers
and route list array-spreads through the std list helpers, keeping list
contracts green and the IR-line guard.

**De-risking findings (all verified):**
1. Imported std functions live in `module_->functions` (every module flattens
   into one unit; only name *visibility* is per-module), so the checker can
   scan for caller-less synthetic targets.
2. `unify` recurses enum variants and records type params in the substitution —
   `unify(List<TP>, List<element>)` binds T→element exactly like a user call;
   and since a genuinely different enum (custom chain) never shares the
   `List` nominal id, the lookup naturally restricts spread routing to the
   std `List` type.
3. The proven lowering template already exists: map spreads emit
   `Evaluate Call` with a `callTarget` (flowir.cpp) and `lowerCall` resolves
   NG/instance targets via `symbolFor`; my synthetic calls ride the same path.
4. `Evaluate Call` to std `length`/`get` needs a `ref` argument — building it
   in flowir would require MakeRef-over-a-synthetic-local (+8 lines). Instead
   two **value-taking private wrappers** in list.ng (`spreadLength`/
   `spreadGet`, each `return get/length(ref list)`) let flowir pass the list
   VALUE directly (zero MakeRef plumbing); the wrapper instance's body does
   the ref and re-instantiates `length`/`get`.

**Edits:**
- `lib/std/list.ng`: +`spreadLength<T>(list: List<T>) -> i64` and
  `spreadGet<T>(list: List<T>, index: i64) -> T` (private).
- Checker: `Checker::resolveListSpreadCall(name, source, span)` (name + param0
  unify + `instantiateFunction`; throws "array spread over `X` requires the
  std.list List type" when no std List helper matches); member
  `listSpreadCalls_` (spread node → {length instance, get instance}) seeded in
  the array-literal value-spread branch (`typecheck_infer.cpp`); new
  `TypeCheckResult.arrayListSpreadCalls`; moved in `typecheck_register.cpp`.
- `flowir.cpp`: `EnumListLength` emission → `Evaluate Call` to the length
  instance (operand = source value); `EnumListGet` emission → `Evaluate Call`
  to the get instance (operands = {source, indexRead}); the ArrayLength/array
  & range paths unchanged. Guard 2256 → **2262** (`test/guard_test.cpp` note).
- `include/flowir.hpp`: remove both opcodes + comments.
- `src/native/lowering_expr.cpp`: remove both lowering cases.
- `src/native/helpers/enum.ssa`: deleted (was only the two list helpers);
  `CMakeLists.txt` drops it from `NG_HELPER_SSA`.
- `test/stdlib_redesign_test.cpp`: +1 `--emit=ssa` absence assertion —
  a list-spread program's SSA contains no `ngrt_enum_list`.

**Accepted scope note:** a structurally list-shaped custom enum (e.g.
`Chain { Next(head, tail: Box<Chain>) }`) previously spread via the structural
walkers; after S2 it is rejected at check time with the clean std.list-required
error. Not used by any corpus/example (committed examples spread only std
`List`); documented in the plan.

**Verification:** `[Stdlib]` 24 cases / 96 assertions, `[Guard]`, non-example
suite 591 cases / 3477 assertions, example sweep 33 cases / 284 assertions —
**624/624 (3761 assertions)** green. Probes: `probe_s2.ng` (spread + mixed +
empty + indexing) exits 0 / returns 7; custom `Chain` spread → clean TypeError.

## INC-21 S1 change plan (span\<T\> borrowed scoped view; LANDED 2026-09-17 — S2 in execution below)

User decisions (2026-09-17): INC-21 split **S1 = span\<T\>**, **S2 =
growth/pushBack + PBT**; `span<T>` is a **borrowed, scoped, non-returnable
view** over array storage.

**Surface:** `span xs` (whole array), `span xs[lo..hi]` (bounds-checked
window); element reads `view[i]` bounds-checked (OOB = negative contract,
runtime signal); type annotations `span<T>` / `T span`. Shared borrow of the
source root (reuses the `ref` loan machinery → later mutable borrows /
reassignment of the source conflict with E-borrow-conflict). Read-only (writes
blocked); non-returnable; not stowable in struct fields/arrays/tuples; span
bindings not reassignable. No span-of-span slices or span `len()` in S1.

**De-risking findings (all verified):**
1. Array ABI is `{len, cap, data[16..]}` — word 1 is already a capacity slot
   (`arr.ssa`); every helper currently sets cap = len. Growth (S2) is
   ABI-compatible; S1 untouched.
2. `ref` machinery is the exact template: borrow registration lives in the
   `ref`/`ref mut` prefix case (`typecheck_infer.cpp` ~1209) keyed on the root
   local's `borrowState_.counts`; release is last-use/bearer-driven in
   `typecheck_check.cpp`; runtime state is a compile-time loan only — a span
   value needs no runtime tracking (mutation while borrowed is rejected).
3. `span` is a free keyword (no `.ng` identifier; only a doc-comment mention).
4. Read-only comes for free via `requireMutableDeref`
   (`typecheck_check.cpp` ~909): add a Span case → always reject (span is an
   immutable view; no `span mut`).
5. Non-returnable mirrors the ref check at `typecheck_check.cpp` ~518.
6. Span value = arena heap blob address (`l`) holding a 16-byte `{data, len}`
   (MakeTraitView 24-byte blob precedent); locals/params carry the blob addr,
   so qtypeOf → Long, needsClone → false, no BindLocal cloning.
7. Window form reuses the range-subscript syntax `xs[lo..hi]` under `span`
   (no new grammar); the checker special-cases the operand like the INC-20c
   list-spread route — a Range-index operand is a window spec, not a copy.

**Edits:**
- `parser.cpp` (`typeTokenTable` ~150) + `KeywordSpan`; prefix case ~654 →
  `PrefixExpression("span", operand)` (note: `span` needs to bind the whole
  `xs[lo..hi]` operand — parse at a precedence that keeps the index).
- `ast.hpp`: `TypeSyntaxKind::Span` + `ScopedSpanTypeSyntax{target}`.
- `type_parser.cpp`: prefix `span<T>` (mirror `ref<T>`) + postfix `T span`.
- `hir.hpp`/`hir.cpp`: `TypeKind::ScopedSpan`, `lowerType` (~37), `renderTypeName`
  (~105, `"{target} span"`).
- `type_interner.cpp`: `internSpan` (mirror `internReference` ~72; descriptor
  `.element`); `resolve`/`resolveWithBindings` ScopedSpan cases (~425/~660);
  `specialize` (~360); `display` (~827, `"{element} span"`); struct-field reject
  (~245/~526).
- `typecheck_infer.cpp`: `span` prefix case (~1209) — operand must be an
  array-typed place or `Index(arrayPlace, Range)`; register a shared loan on the
  root (`borrowState_.add(..., /*mutable=*/false)`), result = `Span{element}`;
  Index receiver accepts Span (~1786/1823) for element reads, Range-slice on a
  span rejected; struct-field/array/tuple element rejects (~223/1055/1131);
  trailing `ref`-only checks (~1218/1223) untouched.
- `typecheck_register.cpp` ~53 / `typecheck_check.cpp` ~518/~909/~499 +
  `requireMutableDeref`: add Span to the same predicates as Reference.
- `typecheck_unify.cpp` ~163/~275/~689: Span rows (mirror Reference).
- `typecheck_const.cpp` ~269: Span case (no const form).
- `include/flowir.hpp`: + `InstructionKind::MakeSpan`, + `SpanRead`? (see
  Index-on-span below) with operand/carry fields.
- `flowir.cpp`: lower the `span` expression → MakeSpan; the Range-operand
  window lowers start/end expression values into MakeSpan operands; Index-on-a
  Span receiver lowers to a span element read.
- `src/native/helpers/span.ssa`: `$ngrt_span_make(l array, l start, l count)`
  (bounds-check → `hlt` on `start<0 || start+count>len`; returns 16B blob
  `{array+16+start*8, count}` and whole-view count = array length) +
  `$ngrt_span_get(l blob, l i)` (`i<0 || i>=len` → `hlt`; load word).
- `lowering_expr.cpp`: MakeSpan case + span element-read case; `function_lowerer.hpp`
  qtypeOf (~239 group) + `Span` → Long.
- `CMakeLists.txt`: + `span.ssa` in `NG_HELPER_SSA`.
- `test/span_test.cpp` (new): positive (whole/window reads, cross-call params,
  generic `span<T>`), negative-contract (window OOB at construction,
  `view[i]` OOB → exit 1 / "killed by a signal"), borrow conflicts (mutable
  borrow while span live; source reassign while span live), rejections
  (return, struct field, array/tuple element, write-through). + `example/span.ng`
  end-to-end sweep.
- Guards: flowir 2262 → +N; ast.hpp `kMaxHeaderLines` = 980 → +N.

## INC-21 S2 (array growth + in-place pushBack + growth PBT) — LANDED 2026-09-20

> Update vs plan: the List half was **not implementable in place** (Box payloads
> deep-copy on read — std.memory; `ref`-of-`ref mut` A1.3; affine moves forbid
> two live List bindings). User chose **List rebuild-per-push**
> (`*list := append(ref current, v)`, O(n)); the O(1)-amortized promise rides the
> array path. `resolvePushBackCow`/`pushTailAppend` removed; no list COW
> retarget. Guard bump flowir 2310

User decisions (2026-09-19): **unique-own `ref mut` mutation model** (grow + mutate
the buffer in place when the receiver root is uniquely owned; copy-on-write when
copies/aliases exist — value semantics always preserved, O(1)-amortized building);
**List too** (in-place mutating tail-cell pushBack mirrors the array rule); PBT as a
**unit-level model** (deterministic randomized trials: push-built array ≡ one-shot).

**Mechanics (all verified):**
1. `ref mut xs` at any call site already lowers to a MakeRef value = the *address of
   the array-pointer word* (for cell locals: the value-cell address; `lowerReferenceExpression`
   flowir.cpp:686; S1's cell-root pass forces referenced array locals into heap cells).
2. A native taking `ref mut <array<T>>` is expressible (`parseTypeUntil` accepts any
   param type for native funs; `write_field(b: Box ref mut)` is precedent), and the
   native receives that address as an `l` operand (Reference → qtypeOf Long, needsClone
   false → shallow pass-through).
3. Zero-step ref-root stores already work natively — `lowering_expr.cpp` AssignPlace:
   `instruction.placeSteps.empty()` → `$ngrt_ref_addr` + `storel`. So whole-referent
   write-back (`*list := e`) needs only checker + flowir target handling (parser already
   yields any expression as an assignment target).
4. Array ABI `{len, cap, data+16}`; cap word is currently always == len. In-place
   growth only ever *re-points the slot* on realloc; old buffers are never written, so
   old aliases (and COW copies) keep the exact pre-push value.
5. `TypeCheckResult` is keyed per call expression (`callTargets` precedent); the
   `Instruction.payload` (int64) rides native call lowering with no new IR fields.

**Surface / semantics (locked):**
- seq.ng: `export native fun pushBack<T>(values: array<T> ref mut, value: T) -> unit`.
  Helper split on a checker-decided flag (lowering_call): `$ngrt_arr_push_mut` (buffer
  unique → write into slack capacity / realloc doubling in place, amortized O(1)) vs
  `$ngrt_arr_push_cow` (always allocates a fresh grown buffer and re-points the slot;
  old copies observe nothing — value semantics preserved). Both bound-check-free,
  negative contract = same `hlt` family (only via impossible-to-reach misuse).
- list.ng: `pushBack<T>(list: List<T> ref mut, value: T) -> unit` (unique body: walk to
  the trailing Box cell and `write` it from Nil → `Cons(value, new(Nil))`; Nil receiver
  writes back `*list := Cons(value, new(Nil))`); aliased call sites are retargeted by
  the checker to `pushBackCow` (rebuild + `*list := rebuilt` write-back). In-place list
  append is O(n) traversal with O(1) mutation per push (no tail pointer in the type);
  true O(1)-amortized sequence building is the *array* path. Existing immutable
  `append(ref list, v) -> List<T>` stays unchanged.
- New micro-feature: **deref write-back `*refMut := e`** (zero-step ref-root AssignPlace)
  — required by the List Nil case and COW write-back; generalizes `b.value := v`.
- Uniqueness = **`sharedLocals`** flow-insensitive escape analysis over container-typed
  locals (array / std List): params start shared; shared when the container VALUE is
  duplicated — bound/assigned from another container, passed by value, returned, packed
  into a struct/tuple/enum literal, spread source, extracted member/index. `ref`/`span`
  borrows and the mutating receiver itself do NOT mark shared. Conservative (over-marking
  is sound; in-place is an optimization on top of always-correct COW).

**Edits:**
- `block_parser.cpp`: none (assignment target already parses). `typecheck_infer.cpp`
  Assign case: accept a Prefix"*" assignmentTarget → referent type + requireMutableDeref.
  `flowir lowerAssignment`/`lowerPlace`: deref root → `.placeRootRef` + zero steps
  (native store path exists). `typecheck_check.cpp`: `requireMutableDeref` deref case.
- Checker sharing analysis + decisions: `TypeCheckResult::inPlaceMutationCalls` (call
  expr → bool) for the array native; `sharedLocals` per function (reused for the list
  target substitution: `callTargets[call] = pushBackCow` when shared, else the public
  unique body). Recognize list `pushBack` by resolved name + param0-unify (list-typed
  ref mut), array `pushBack` similarly.
- `seq.ng` + `list.ng` declarations as above (+ `pushTail` helper).
- `arr.ssa`: `$ngrt_arr_push_mut(l addr, l val)` / `$ngrt_arr_push_cow(l addr, l val)`;
  growth factor doubling, capacity word updated; slot re-pointed on realloc.
- `lowering_call.cpp`: `if (name == "pushBack") symbol = instruction.payload ?
  "$ngrt_arr_push_mut" : "$ngrt_arr_push_cow";` — clang-format only the touched block.
- Guards: `test/guard_test.cpp` flowir / typecheck_infer / checker line bumps.
- Tests: `test/growth_test.cpp` `[Growth]` PBT (INC-1 harness, `pbt::currentSeed`, ~60+
  trials: random n ∈ [0, 200], random i64 values, random unique/COW split via an early
  `let b = xs` on ~half trials) asserting push-built ≡ one-shot `[...]` element-wise
  (len + timed reads, printed booleans); deref-assign round-trip tests; negative
  contracts (pushBack on immutable binding rejected, pushBack into a field-span ref not
  applicable, COW preserves the old copy / read-back isolation). + `example/growth.ng`.
- Docs: 09-increment-plan INC-21 S2 done; 07-post-cutover A7 (span + growth/pushBack
  landed, R6 list-builder note updated); workstate/worklog.

**Risks / accepted:** in-place is gated on a conservative *may-be-shared* flag — the
common `let mut acc=[]; while…{ pushBack(ref mut acc,…) }` builder is in-place; any
leak marks COW (sound, slower). List in-place is O(n) traversal per push (no tail
pointer) — the O(1)-amortized promise is delivered on the array path; documented.
A callee-returned array is conservatively shared in its caller → COW (sound).

## References

- `.agents/worklog/2026-08-27.md` — INC-B / INC-A / INC-9 / INC-10 entries
- `.agents/worklog/2026-08-30.md` — INC-11 entry
- `.agents/worklog/2026-08-31.md` — INC-14 S1 entry + S2a+S2b entry + S2c entry
- `.agents/worklog/2026-09-01.md` — INC-14 S3 entry
- `.agents/worklog/2026-09-07.md` — INC-14 S4-fwd (closure-param forwarding), S4-generic, S5, S6 entries
- `.agents/worklog/2026-09-08.md` — INC-12 + INC-13 entries
- `.agents/worklog/2026-09-10.md` — INC-17 + INC-18 entries
- `.agents/worklog/2026-09-11.md` — INC-19 S1 (generic Box<T> + new) + INC-20a (affine-enum switch consumption) entries
- `.agents/worklog/2026-09-14.md` — INC-20b (Box.clone deep copy) entry
- `.agents/worklog/2026-09-15.md` — INC-20c S0 (Box-in-aggregate fixes + Box-list feasibility) entry
- `.agents/worklog/2026-09-16.md` — INC-20c S1 (std.list on Box nodes) + INC-20c S2 (spreads via std helpers) entries
- `docs/design/rearchitecture/09-increment-plan.md` — execution table + INC-14 slices
- `AGENTS.md` — pipeline/build/test/commit conventions