# Post-Cutover Plan — vNext Completion Work

> **Status:** planning document. The legacy cutover is complete; the work items
> below are planned and designed, but **not implemented**.
>
> **AI-assisted document:** drafted with AI assistance based on the accepted
> decisions in [`04-language-decisions.md`](04-language-decisions.md), the
> migration matrix in
> [`05-legacy-example-migration-matrix.md`](05-legacy-example-migration-matrix.md),
> and the roadmap in [`03-delivery-plan.md`](03-delivery-plan.md).

## 1. What the cutover delivered

The legacy orgasm/interpreter pipeline has been fully removed from the
repository. The state at cutover:

- **Only pipeline:** syntax → HIR (`hir::Resolver`) → type checker
  (side tables + `TypeInterner`) → FlowIR → bytecode v3 → VM. `ngi` links only
  `ng`; the legacy `ng` library, headers, sources, stdlib
  (`lib/std/`, `lib/std.ng`), legacy tests, and the legacy example corpus
  (`example/01..60`, `example/design-draft`, `example/legacy`, imgui/shebang/
  external/interpreter/IDE examples) are deleted.
- **vNext examples:** 33 files under `example/` (plus
  `example/modules/`), each runnable end to end through `ngi`.
- **vNext stdlib:** redesigned `lib/std/` — `prelude`, `io`, `string`
  (`length`/`charAt`/`substring`/`toUpper`/`toLower`), `seq`
  (`len`/`sum`/`arrayContains`/`reverse`), `list` (recursive-enum `List<T>`
  with `length`/`get`/`contains`), and `memory` (native handles,
  `allocate`/`load`/`store`/`release`/`outstanding`, and a concrete `Box` with
  `impl Drop` release). No GC exists; heap domains are deferred (D-015 rule 7).
- **Tests:** 48 suites under `test/`; the full suite
  (`./build/ng_test`) is the only green gate (1425 assertions / 386 test cases
  passing at cutover).
- **Docs:** `AGENTS.md` rewritten for the vNext-only structure.
- **Host-boundary validation:** after the cutover the imgui corpus was
  re-implemented on the promoted pipeline — a redesigned Dear ImGui binding
  (`lib/std/imgui.ng`, SDL3 GPU backend, `ngi_imgui` frontend), a self-hosting
  `runNgi` native, an unlimited-fuel `--fuel 0` mode, and a minimal NG IDE
  (`example/ng_ide.ng`) — proving `native fun`, opaque handles, re-entrant
  compilation, and interactive loops end to end (matrix row 14).

## 2. How to read this plan

Work is grouped into three categories:

- **Category A — decided, first slice done, remaining edges.** These features
  have an accepted decision and a shipped vNext slice; the listed work
  completes them. They are the default execution order.
- **Category B — explicitly deferred by the design docs.** These are accepted
  decisions whose implementation is gated behind other phases (D-008
  `isize`/`usize`, D-015 rule 7 heap domains, R9 C ABI).
- **Category C — unstarted roadmap rows.** R9 (partial), R10, R11. No
  implementation begins before the blocking phases below.

Each item states its blocking work, target phase/decision, and the remaining
legacy evidence it covers.

## 3. Category A — decided features, remaining edge work

### A1. Full non-lexical loan analysis (D-002/D-015)

**Current slice:** scoped `ref`/`ref mut`/`*` places with cell-backed bindings,
deep-copy bind/call/return, simple shared/mut exclusivity checks, block-scoped
release, escaped-reference rejection (`example/ref_swap.ng`,
`ref_places.ng`).

**Remaining work:**

1. Non-lexical borrows: loans released at last use, not block exit;
   reborrow after release; mut-after-shared-loan diagnostics with spans.
2. D-002 origin contracts (`T ref(a)`, `T ref(a | b)`) and returning
   references (`T ref` results) — currently rejected as escaped.
3. Refs stored in aggregates/globals/task payloads remain rejected until the
   full loan analysis and R10 transfer rules land.
4. Use-after-move through an outstanding ref, and borrows across calls
   (argument loan scopes, return-places).

**Blocks:** R5 ownership, R10 concurrency transfer (invariants 10–11).
**Legacy evidence:** examples `21`–`24`, `39`, `41`, `50`, `51`.
**Decisions:** D-002 rule 3, D-015 rules 5, "Remaining D-015 work".

### A2. Module interface artifacts and instances (R3/R6/R7)

**Current slice:** transitive `import` loading with per-module visible-name
sets, selective imports, export gating, transitive re-export, and
deterministic cycle/missing-module diagnostics (`example/modules/`).

**Remaining work:**

1. `ModuleArtifact` / `ModuleInstance` split: immutable compiled modules
   (exported function descriptors, vtables, consts) vs. per-session instances
   (globals, initialization state, native state) — invariant 7.
2. Exported function descriptor tables: name → signature/instance id, so
   imports bind against a typed artifact rather than re-checking source.
3. Session-scoped resolver/artifact caches and instance lifecycle
   (initialize/teardown) in `RuntimeSession`.
4. Stdlib preload as a session-owned artifact set, not per-run source parse.

**Blocks:** R6 (`RuntimeSession`), R7 artifact descriptor work.
**Legacy evidence:** examples `08`, `13`, `18`, `56`, `59`; matrix row 5.

### A3. Monomorphized instance artifacts and overload caching (R4/R7)

**Current slice:** generic functions/enums instantiate per concrete argument
list with re-checked bodies; explicit and inferred generic call arguments;
`GenericDefId`/`InstanceId` identity (`example/generic_functions.ng`).

**Remaining work:**

1. Instance descriptor reuse: identical `InstanceId`s share one checked body
   and one artifact function; no duplicate re-check per call site.
2. Overload-set artifacts: encode resolved overloads per call site in the
   artifact; keep specificity resolution (`specificityGuarded`) out of the
   runtime path.
3. Partial specialization semantics for `const` predicates where the design
   demands it (currently exact/pattern/primary priority at module level).

**Blocks:** R4 instance graph, R7 artifact encoding.
**Legacy evidence:** examples `15`, `43`, `44`, `60`; matrix row 7.

### A4. Structural switch patterns and rest/destructuring patterns

**Current slice:** `switch` with `case Variant(binding)`/`otherwise`,
exhaustiveness, multi-field variant tuple payloads, positional destructuring,
recursive payloads through `ref<Node<T>>` (`example/enum_match.ng`,
`recursive_enums.ng`).

**Remaining work:**

1. Structural patterns in `case`: tuple patterns (`(a, b)`), literal patterns
   (`1`, `"x"`), and or-patterns (`A | B`) — legacy `20`'s surface.
2. Rest patterns in destructuring (`(first, ...rest)`) and tuple spread/rest
   bindings — legacy `50`, `54` remainder.
3. Pattern bindings as move-into-bindings with the A1 partial-move rules
   (field-aware, per-branch merge).

**Blocks:** A1 for move-aware pattern bindings; R4 pattern lowering.
**Legacy evidence:** examples `14`, `20`, `50`, `54`; matrix row 4.

### A5. Generic impls, Self-typed and default trait-view methods (D-011)

**Current slice:** trait declarations, impls with coherence, supertraits,
default methods, qualified calls, static dispatch, `T: Trait` bounds, auto
traits with `derive(Copy + Clone)`, `ref<Trait>` views with per-concrete
vtables and dynamic calls (`example/traits.ng`, `derive.ng`,
`trait_objects.ng`).

**Remaining work:**

1. Generic impls: `impl<T> Trait for Type<T> { ... }` with per-instance method
   instantiation and coherence across instances.
2. Self-typed view methods: methods whose `self` type appears in the
   signature (e.g. `clone() -> Self`), callable through `ref<Trait>`.
3. Default methods through views: dispatch to the trait default when a
   concrete impl does not override.
4. Trait bounds in const predicates/functions (`where T: Trait` inside
   `const fun`) — see A6.

**Blocks:** R4 instance graph (shared with A3).
**Legacy evidence:** examples `25`–`40`, `46`, `55`, `59`; matrix rows 12–13.

### A6. Const evaluation follow-ups: native hosts and generic const fun

**Current slice:** `const if` folding, module-level `const` predicates with
pattern specialization and `= delete`, typed-HIR `ConstInterpreter` for
`const fun` (loops, recursion, tail recursion, runtime callability), `where`
clauses, and per-instance `const if` (`example/const_predicates.ng`,
`const_fun.ng`, `where_clauses.ng`, `const_if_instances.ng`).

**Remaining work:**

1. `= native` hosts inside `const fun` / const contexts: const-evaluable
   native functions with capability declarations (invariant 8), so const
   evaluation can call declared pure hosts.
2. Generic `const fun` compile-time calls: calling a `const fun` over
   const-generic parameters during type checking of another instance.
3. Trait bounds in const predicates (interacts with A5).

**Blocks:** R9 declared native descriptors for pure hosts.
**Legacy evidence:** examples `17`, `42`, `46`, `47`, `53`; matrix row 8.

### A7. Stdlib completion slice (legacy 52/56/59 residue)

**Current slice:** `seq` `len`/`sum`/`arrayContains`/`reverse`,
`currentExecutablePath`, string utilities, recursive-enum `List<T>`,
concrete `Box` (`example/std_seq.ng`, `std_list.ng`, `heap_box.ng`,
`fixed_arrays.ng`).

**Remaining work** (decided surface, no new decision needed):

1. `span<T>` views over array storage, with bounds-checked slicing
   (`fixed_arrays.ng` maps legacy vector→array; span is the remaining 52 item).
2. Growing collections: `pushBack`/`append`/spread for `List<T>` and
   arrays — depends on the B1 heap domain for allocation beyond the current
   fixed-size native handles.
3. List builders: `List<T>` collection literals lowering to push loops.
4. Move `reverse` from a driver-native into `lib/std` once it can be
   expressed in NG (or keep as a declared native in the ABI layer).

**Delivered (2026-08):** `regexMatch` (string intrinsic with invalid-pattern
diagnostics in `lib/std/string.ng`), range/slice value spreads into array
literals (`[...(1..5)]`, `[...nums[1..3]]`, mixed `[0, ...xs, 9]`, at most
one spread per literal; `test/array_spread_test.cpp`), and
`example/std_string.ng` completing the legacy 56 surface end to end.

**Blocks:** B1 for growth allocation; R7 for span ABI.
**Legacy evidence:** examples `52`, `59`; matrix rows 2/10 remainder.

## 4. Category B — explicitly deferred by the design docs

| Item | Decision | Gate | Notes |
|---|---|---|---|
| B1. Heap domain: generic `Box<T>`, `Gc<T>`, `Arc<T>`, arena callbacks, `new` | D-015 rule 7 (D-002 rules 7–9, 14) | R6 `RuntimeSession` | No GC. The current concrete `Box` with `impl Drop` remains the only heap form until R6. |
| B2. `isize`/`usize` | D-008 | R9 `TargetAbiDescriptor` | Target-width ABI types; layout comes from the ABI descriptor, never host serialization. |
| B3. Declared C ABI: `extern "C"`, `repr(C)`, ABI descriptors, opaque lifecycle policies, bindgen | R9 | R4, R6, R7 | `native fun` intrinsics and opaque declarations already ship; declared host signatures, ownership contracts, and bindgen do not. |

These are accepted language decisions whose implementation is intentionally
gated; do not start them before their blocking phase.

## 5. Category C — unstarted roadmap rows

- **R9 remainder** (in progress): declared ABI descriptors, safe C boundary,
  opaque wrapper lifecycle policies, bindgen (see B3).
- **R10** (not started): concurrency-native runtime and language layer.
  Structured concurrency with ownership-aware transfer; task payloads are
  move-only; refs do not cross task boundaries; process globals are not an
  implicit sharing mechanism (invariant 11). Blocks on R5 (A1), R6 (B1), R7,
  R9.
- **R11** (not started): formatter, LSP, debug metadata, benchmarks, and
  stabilization documentation.

## 6. Suggested execution order

1. **A1** non-lexical loans — unblocks move-aware patterns (A4) and is the
   last D-015 slice.
2. **A3** instance artifact reuse + **A5** generic impls / view methods —
   both need the R4 `InstanceId` graph.
3. **A4** structural switch and rest patterns (after A1).
4. **A6** const follow-ups (pure native hosts depend on an early R9
   descriptor slice).
5. **A2** module interface artifacts together with the **R6** `RuntimeSession`
   slice, which then unblocks **B1**.
6. **B1** heap domain, then **A7** growing collections / list builders.
7. **B2/B3** with **R9**; **R10** after R5/R6/R7/R9; **R11** last.

Each item is a vertical slice: grammar/HIR/checker/lowering/VM + tests +
example + docs, committed green.

## 7. Acceptance criteria per item

Reuse the matrix gate (05-legacy-example-migration-matrix.md):

1. A vNext-only feature test covers parsing, resolution, type validation,
   lowering, bytecode, and VM execution.
2. Negative tests cover exact diagnostics and source spans where relevant.
3. The feature has one authoritative vNext runtime/ABI representation; no
   legacy shim.
4. The matrix row and this document are updated in the same commit that lands
   the slice, with `./build/ng_test` fully green.

## 8. Non-goals and standing constraints

- No GC; heap domains arrive as explicit R6 runtime work, not a tracing
  collector grafted onto `Value`.
- No user-visible lifetimes, lifetime names, or origin syntax in normal
  user-facing output (D-002 hard constraint).
- No legacy syntax compatibility (implicit top-level execution, `val`, loop
  shorthand, heuristic `type` forms).
- No `dyn Trait` or owning erased trait containers; `ref<Trait>` remains the
  only dynamic-trait value form (matrix non-goals).
- The current concrete `Box` is not extended into a generic container before
  B1; do not grow its native surface ad hoc.

## 9. Documentation touchpoints

When any item lands, update in the same commit:

- its status in [`04-language-decisions.md`](04-language-decisions.md);
- its row in [`05-legacy-example-migration-matrix.md`](05-legacy-example-migration-matrix.md);
- the progress board in [`README.md`](README.md);
- this document (move the item to done, adjust ordering).
