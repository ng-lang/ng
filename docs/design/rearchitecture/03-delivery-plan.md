# NG vNext Delivery Plan

> **Status:** proposed.  
> **AI-assisted document:** drafted with AI assistance.
>
> Every work package below is intentionally small enough to be reviewed, tested, and reverted independently. Do not start a later phase because it is attractive; start it only after all listed entry criteria are true.

## Delivery rules

1. One PR has one observable architectural objective.
2. Add characterization/acceptance tests before or with the implementation; no "tests later" work packages.
3. A new abstraction is introduced beside the old path, exercised in differential tests, then made authoritative, then the old path is deleted.
4. New code must not add mutable process-global state.
5. A phase completion PR updates this document, the [index](README.md), and user-facing docs.
6. New language syntax requires a resolved decision from [the decision log](04-language-decisions.md).
7. Existing source behavior is not a compatibility promise. Differential tests identify intentional semantic changes and prevent accidental ones.

## Phase R0 — Governance, decisions, and characterization

### Objective

Make current behavior measurable and decide the language direction before changing core representation.

### Entry criteria

- This plan and the gap register are accepted as the active planning baseline.
- Owners are available to decide D-001 through D-005 before their blocking phase.

### Work packages

| ID | Change | Verification |
|---|---|---|
| R0.1 | Create a test feature matrix mapping parser/typechecker/HIR-or-STUPID/VM/module artifact support. | Matrix is generated or checked in; each unsupported cell has a diagnostic test. |
| R0.2 | Add parser AST tests for precedence and associativity. | `2 * 3 + 4`, subtraction/division chains, comparison chains, assignment, pipeline, generic/comparison ambiguity. |
| R0.3 | Add numeric lexical/parse error corpus. | All supported suffixes, max/min values, overflow, `1e+2`, malformed exponent, invalid suffix, exact spans. |
| R0.4 | Add in-process compiler and VM reuse fixtures. | Sequential modules cannot retain globals, stack, imports, trait metadata, or native state. |
| R0.5 | Add malformed bytecode corpus/fuzz harness. | Every opcode rejects truncated/bad operands without out-of-bounds access; sanitizer job passes. |
| R0.6 | Add backend differential harness. | Selected examples run through legacy STUPID and ORGASM; compare values/stdout/error class under a defined normalization. |
| R0.7 | Add module initialization/session isolation fixtures. | Source/native/bytecode imports, init once, failed init, aliasing, cycles, independent sessions. |
| R0.8 | Establish sanitizer and static-analysis CI modes. | ASan+UBSan test job; optional TSan target after globals are removed; clang-tidy baseline recorded. |

### Exit criteria

- All stop-the-line gaps from the register have a failing characterization test or an explicit test proving existing behavior.
- D-001, D-002, and D-004 are decided or have implementation-blocking issues recorded.
- CI can run unit tests plus sanitizer tests without relying on test order/global cache leakage.

---

## Phase R1 — Stop-the-line correctness and bytecode safety

### Objective

Fix known wrong results and make existing bytecode execution safe enough to serve as a migration oracle. This phase does **not** add major language features.

### Work packages

| ID | Minimal implementation | Verification / deletion gate |
|---|---|---|
| R1.1 | Replace parser binary-expression recursion with a precedence-climbing/Pratt parser while retaining existing AST node shapes temporarily. | New AST tests and execution test prove standard precedence/associativity. Delete old `binaryExpression()` recursion. |
| R1.2 | Define one supported numeric literal matrix; reject unsupported formats at parse/type stage with spans. | No raw `std::stoi/stoll/stod` error reaches CLI. Boundary/property tests pass. |
| R1.3 | Add an opcode descriptor table for the current bytecode enum. | Decoder and verifier use it for operand widths. No second manual width table may be introduced. |
| R1.4 | Implement pre-execution bytecode verifier. | Reject bad opcode, truncated operand, pool/index/export/import/function index, invalid jump target, impossible local/global index. Fuzz corpus passes under sanitizers. |
| R1.5 | Route VM operand reads through checked decoder APIs. | Remove direct unchecked pool access. Every instruction handler has verifier and runtime defense tests. |
| R1.6 | Reset VM run state and compiler lowering state deterministically. | Reuse tests pass; all compiler maps are in per-compile context or explicitly reset. |
| R1.7 | Quarantine `BytecodeModule::merge()`: either implement schema-driven remapping including imports/float constants/variable instructions, or mark it internal-unavailable and remove callers. | Merge tests cover every opcode descriptor. No partial scanner remains. |
| R1.8 | Correct docs generated from current opcode descriptor. | `docs/guide/orgasm-backend.md` opcode table matches enum/schema in CI. |

### Exit criteria

- No known arithmetic precedence error remains.
- VM never executes an artifact before verification succeeds.
- Compiler/VM reuse and malformed-artifact tests pass under sanitizers.
- Module merge is either proven complete by schema tests or absent from supported execution paths.

---

## Phase R2 — Source model, lexer, parser, and immutable syntax AST

### Objective

Provide a stable syntax/tooling boundary. This phase may intentionally break current grammar according to D-001/D-002.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R2.1 | Add `SourceManager`, `SourceFileId`, byte ranges, line map, diagnostic labels. | Parser/lexer diagnostics include file, range, and line/column; multi-line tests pass. |
| R2.2 | Replace `TokenPosition` with `SpanId`; add explicit EOF token and token kind tables. | No parser path throws an EOF exception solely for normal end-of-input. |
| R2.3 | Rewrite lexer as a deterministic cursor with ASCII-safe classification and exact literal token payloads. | Existing chosen syntax corpus plus random-byte fuzz tests; no signed-char UB sanitizer finding. |
| R2.4 | Introduce one-owned syntax AST and remove raw/shared AST compile switch. | AST destruction is automatic; no `destroyast` call remains in production pipeline. |
| R2.5 | Implement Pratt expression parser and distinct type parser/token cursor. | Generic closing `>>`, comparisons, postfix calls/index/member, assignment, and error recovery tests pass. |
| R2.6 | Replace heuristic type declaration parsing with explicit vNext grammar. | Each declaration form maps to one syntax node; ambiguous legacy forms are rejected or intentionally specified. |
| R2.7 | Add recovery API for REPL/LSP (`Complete`, `Incomplete`, `RecoveredWithErrors`). | REPL no longer manually guesses only braces/semicolons. |
| R2.8 | Add formatter-oriented syntax snapshots and parser fuzzing. | Parse-print-parse structural invariants for supported syntax. |

### Exit criteria

- Syntax AST has no typechecker/compiler fields.
- Parser does not mutate its input token stream.
- All syntax nodes have spans.
- The new parser is the only parser used by CLI and REPL.

---

## Phase R3 — CompilationSession and module graph

### Objective

Make module resolution, source/artifact caching, and semantic loading deterministic and session-scoped.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R3.1 | Introduce `CompilationSession`, `ModuleResolver`, and canonical `ModuleId`. | Two sessions resolve the same name independently with different roots. |
| R3.2 | Build an explicit module dependency graph before body checking. | Direct/transitive cycles get one deterministic diagnostic with import spans. |
| R3.3 | Split `ModuleArtifact` from loader cache entries; artifacts contain no runtime state. | Static assertion/API review prevents `StorageCell`/runtime function in artifact structures. |
| R3.4 | Define strict import resolution and explicit registered-opaque test mode. | Misspelled source import fails typecheck; fixture opaque module succeeds only when registered. |
| R3.5 | Implement artifact cache key/version/invalidation policy. | Editing source or target config invalidates only necessary artifacts; deterministic cache test. |
| R3.6 | Make prelude an explicit resolver-provided dependency rather than a hidden global/typechecker singleton. | Session with/without prelude has deterministic behavior; failure can retry in later session. |

### Exit criteria

- `get_module_registry()` and global file cache are not required by new frontend paths.
- Parsing/resolving a module never executes it.
- All import visibility and canonical identity tests run without process-global cleanup helpers.

---

## Phase R4 — Symbols, types, resolved HIR, and typed HIR

### Objective

Replace string-based semantic identity and mutable AST annotations with a queryable typed representation.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R4.1 | Create Def/Type/Const arenas and stable IDs in `CompilationSession`. | Same-named definitions in distinct modules remain distinct in lookup, diagnostics, and artifact interface. |
| R4.2 | Implement declaration collection and namespace/name resolution into Resolved HIR. | Imports, aliases, overload sets, trait/type namespaces, and shadowing have snapshot tests. |
| R4.3 | Implement TypeInterner and type annotation resolution. | Alias/newtype/nominal/opaque/ref/function/generic identity tests do not use `repr()` parsing. |
| R4.4 | Lower syntax bodies to HIR and typecheck expressions/statements bidirectionally. | HIR snapshot includes `TypeId`, value category, resolved callee, spans. |
| R4.5 | Move overload/generic inference to solver result objects. | Compiler no longer calls string inference helpers for migrated expressions. |
| R4.6 | Move trait/impl selection to explicit evidence records. | Inherent/trait/default/qualified dispatch cases produce selected `ImplId`/method `DefId`. |
| R4.7 | Emit structured public module interface descriptors. | Source and serialized test artifact interface round-trip equality passes without `type_from_repr()`. |
| R4.8 | Migrate CLI to parse → resolve → typecheck → TypedModule API. | Typecheck occurs exactly once; syntax tree remains unmodified before/after. |

### Exit criteria

- New compiler path takes `TypedModule`/`InstanceGraph`, not syntax AST.
- No new code writes semantic information into syntax nodes.
- No new semantic code makes behavior decisions by parsing type display strings.
- The old typechecker is only a differential oracle for migrated language subsets.

---

## Phase R5 — Const evaluation, scoped-borrow analysis, and FlowIR

### Objective

Make compile-time computation, scoped-borrow constraints, and drop semantics typed, deterministic, and backend-independent without lifetime syntax.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R5.1 | Implement `ConstValue`, const interning, and typed literal arithmetic. | Exact integer limits/overflow policy, strings/tuples, diagnostics, and resource budgets tested. |
| R5.2 | Implement restricted `ConstEvaluator` for typed HIR. | Const calls work without creating STUPID/runtime module/StorageCell. Forbidden IO/native/mutation tests fail statically. |
| R5.3 | Introduce `ConstNativeDescriptor` for pure deterministic host predicates. | Native const predicate has declared capability/signature; nondeterministic/native runtime calls rejected. |
| R5.4 | Implement generic instance graph and per-instance const-if elimination. | Chosen branches lower; inactive-branch policy follows D-003 and is tested. |
| R5.5 | Introduce `Place`, move paths, loans, and lexical ownership dataflow. | Whole/partial moves, restore by assignment, borrow conflict, joins, loops, and call effects tested. |
| R5.6 | Lower typed HIR to FlowIR with explicit blocks/cleanup edges. | FlowIR verifier proves local state and exactly-once drop in test corpus. |
| R5.7 | Define function effect summaries and enforce const/unsafe/foreign restrictions. | Capability diagnostics reference callee/effect/span chains. |

### Exit criteria

- No compile-time path calls legacy `eval_const_function`/STUPID.
- Ownership facts are not encoded in variable-name strings.
- FlowIR is the sole input for new backend lowering.

---

## Phase R6 — RuntimeSession, values, modules, and lifecycle services

### Objective

Replace the mixed `StorageCell` model with one runtime contract usable by the HIR evaluator and VM.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R6.1 | Introduce `RuntimeSession`, descriptor registry, and `ModuleInstanceStore`. | Two sessions have isolated heap/globals/native state. |
| R6.2 | Define `Value`, `Slot`, `HeapObject`, and `TypeDescriptor` APIs for primitives/tuples/structs/enums/refs. | Member/index/tag behavior has common evaluator/VM tests. |
| R6.3 | Implement descriptor-directed trace/copy/move/drop. | Aggregate graph, partial move, finalizer, and native-handle ownership tests pass. |
| R6.4 | Implement immutable artifact → instance initialization protocol. | Init once/failure/cycle tests pass for source/native/bytecode artifact fixtures. |
| R6.5 | Extract lifecycle/member/sequence/call services from legacy STUPID and VM. | Both engines call the same service test double for migrated operations. |
| R6.6 | Migrate GC roots to session-owned heap and explicit execution roots. | No global provider registration needed in new path; GC stress tests pass. |
| R6.7 | Define native handle descriptor and typed host value views. | No `shared_ptr<void>` native state cast in migrated native APIs. |

### Exit criteria

- New runtime values do not carry two competing aggregate representations.
- `ModuleArtifact` contains no mutable runtime state.
- New HIR evaluator can run a module in an isolated RuntimeSession.

---

## Phase R7 — Bytecode v3 compiler, VM, and artifact format

### Objective

Build a verified bytecode backend from FlowIR using the new runtime and module interface.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R7.1 | Define bytecode v3 opcode schema, operand types, stack effects, and binary encoding. | Encoder/decoder/disassembler/verifier generated from one schema. |
| R7.2 | Define fixed-endian artifact v3 with explicit section limits and metadata versions. | Cross-reader golden files; corrupt-file corpus; no host scalar serialization assumptions. |
| R7.3 | Lower scalar/control-flow/call FlowIR subset to bytecode. | Differential HIR-evaluator/VM tests for functions, branches, loops, returns. |
| R7.4 | Lower descriptors, aggregates, places, moves, and cleanup operations. | Drop/move/ref/struct/enum tests use shared runtime descriptors. |
| R7.5 | Implement bytecode module import through ModuleInstance API. | Imported globals/functions/types initialize and dispatch correctly exactly once. |
| R7.6 | Add source map, stack trace, and verifier error reporting. | Runtime error reports original source span and call stack. |
| R7.7 | Decide/remove legacy bytecode merge. Prefer runtime linking unless static linking has a demonstrated requirement. | No unverified hand-remap implementation remains. |
| R7.8 | Add bytecode package compatibility tests and performance baseline. | Artifact format/version/target mismatches reject predictably; benchmark is recorded, not guessed. |

### Exit criteria

- VM only consumes verified bytecode v3.
- Bytecode compiler only consumes FlowIR.
- Source/native/bytecode imports use the same module-instance protocol.
- Legacy ORGASM format is either retired or isolated as read-only migration tooling.

---

## Phase R8 — Reference interpreter convergence and legacy removal

### Objective

Make one reference semantics engine and remove the direct-AST STUPID path.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R8.1 | Implement FlowIR or typed-HIR evaluator on RuntimeSession. | Differential corpus compares evaluator and bytecode VM across migrated features. |
| R8.2 | Move REPL to incremental syntax/session/typecheck/HIR execution. | REPL rejects type errors consistently with file execution and preserves only session-approved globals. |
| R8.3 | Migrate const/evaluator test fixtures off legacy STUPID. | No frontend test requires AST execution. |
| R8.4 | Delete legacy AST visitor interpreter, duplicated lifecycle helpers, and legacy native adapters. | Search/build rule proves no production path includes `src/intp/stupid.cpp`; old tests ported or removed with rationale. |

### Exit criteria

- There is no user-visible execution mode that bypasses type checking.
- Exactly one runtime semantic service implementation is used by reference evaluator and VM.

---

## Phase R9 — Native ABI, C FFI, opaque types, and bindgen

### Objective

Replace ad-hoc `StorageCell` host callbacks with declared runtime-native and C ABI boundaries.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R9.1 | Define syntax/HIR for `extern`, `extern "C"`, `opaque`, `repr(C)`, and ownership annotations according to D-004. | Parser/typechecker rejects illegal combinations with spans. |
| R9.2 | Implement `CallableDescriptor`, native function descriptor, typed `ValueView`, and generated C++ adapter helpers. | Same native descriptor works from HIR evaluator and VM; wrong argument/ownership tests fail before host call. |
| R9.3 | Implement opaque pointer-handle descriptors with drop/clone/borrow/null/thread capabilities. | C fixture verifies create/use/destroy, double-drop prevention, and ownership transfer. |
| R9.4 | Implement ABI-safe C import subset using target ABI descriptor and libffi/generated stubs. | C fixture covers integers, floats, cstr, slices, opaque handles, `repr(C)` record where supported. |
| R9.5 | Define callback registration/trampoline and returned-buffer policy. | Foreign callback lifetime/use-after-unregister tests; panic/error boundary tests. |
| R9.6 | Implement optional C export wrappers and `ng-bindgen` MVP. | Generated C header compiles and invokes exported ABI-safe function from C test. |
| R9.7 | Remove legacy `NGCallable(self, env, args)` default registration API. | Legacy adapter is explicit `unsafe legacy` migration-only or deleted. |

### Exit criteria

- Safe FFI does not expose raw VM slots or arbitrary runtime state.
- Opaque types cover pointer/resource wrappers with explicit ownership.
- Unsupported C ABI shapes fail at declaration time, not during VM execution.

---

## Phase R10 — Experimental concurrency MVP

### Objective

Add a deliberately minimal, explicitly non-stable `spawn` / `await` experiment only after ownership, session isolation, and native capabilities are enforceable. This phase does not establish NG's permanent concurrency contract.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R10.1 | Define preliminary `Send` capability checks for task arguments/results and reject scoped refs/thread-affine handles. | Compile tests reject invalid task transfer. |
| R10.2 | Implement isolated child `RuntimeSession` task execution independent of process globals. | Parent and child cannot observe shared mutable module globals. |
| R10.3 | Add experimental `spawn direct_call(args)` / `await task` lowering to HIR/FlowIR/bytecode. | Moved/copyable argument/result and typed `Result` failure tests pass. |
| R10.4 | Label syntax, diagnostics, artifact metadata, standard library, and docs as experimental/non-stable. | Test fixture proves experimental metadata is emitted and exposed. |
| R10.5 | Integrate native-handle sendability/thread-affinity rejection. | Cross-session/thread foreign resource negative tests pass. |

### Exit criteria

- No task relies on accidental shared VM/module/GC globals.
- Scoped refs cannot enter a task.
- The MVP is visibly marked experimental and may change without compatibility guarantees.
- TSan/stress suite passes for the supported isolated-session configuration.

### Explicitly not delivered

- stable `Send`/`Sync` semantics;
- detach, cancellation, actors, channels, shared heap, or shared mutable state;
- async state machines or scoped refs across suspension;
- stable task ABI or final concurrency memory model.

---

## Phase R11 — Tooling, documentation, performance, and stabilization

### Objective

Build developer tooling and optimize only on stable boundaries.

### Work packages

| ID | Minimal implementation | Verification |
|---|---|---|
| R11.1 | Formatter over syntax tree/trivia model. | Idempotence corpus and parse-equivalence tests. |
| R11.2 | Incremental query API for LSP. | Edit/recheck benchmark; diagnostics/symbol navigation tests. |
| R11.3 | Debugger/profiler over bytecode v3 source maps and runtime sessions. | Breakpoint/stack/local inspection fixtures. |
| R11.4 | Generate language/ABI/opcode/module-interface docs from descriptors. | CI detects stale generated docs. |
| R11.5 | Benchmark suite for parser/typecheck/const/VM/runtime/FFI. | Baseline trend stored; optimization PRs include benchmark deltas. |
| R11.6 | Evaluate optional SSA optimizer, AOT, WASM, moving GC, richer hidden borrow inference, and async state machines. | RFC plus target-specific correctness/performance evidence before implementation; user-visible lifetime syntax remains forbidden. |

## Cross-phase test strategy

| Layer | Required test style |
|---|---|
| Lexer/parser | golden tokens, AST snapshots, property/fuzz tests, span assertions |
| Resolver/type system | typed-HIR snapshots, negative diagnostics, module identity/coherence fixtures |
| Const evaluator | deterministic value/error corpus, fuel/recursion/resource tests |
| Ownership/FlowIR | CFG/dataflow snapshots, drop-count instrumentation, alias/move negative tests |
| Runtime | descriptor contract tests, heap stress, module instance isolation, native handle lifecycle |
| Bytecode | schema round-trip, verifier fuzzing, golden artifacts, HIR evaluator differential tests |
| FFI | compiled C/C++ fixture libraries, ABI layout assertions, ownership/callback/error tests |
| Concurrency | deterministic scheduler tests, stress tests, TSan, capability negative tests |

## Definition of done for a migration

A migrated feature is done only when:

1. Syntax, HIR, runtime, and backend support are declared in the feature matrix.
2. The feature has positive and negative diagnostics tests.
3. HIR evaluator and bytecode VM agree on the supported subset.
4. Module/artifact behavior is tested where the feature is public.
5. Native/const/concurrency capability behavior is tested if applicable.
6. The legacy implementation for that feature is deleted, or the index explicitly records why it remains as a temporary oracle.
