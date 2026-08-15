# NG vNext Gap Register

> **Status:** proposed baseline for R0.  
> **AI-assisted document:** drafted with AI assistance.
>
> This register is intentionally about architectural facts, not blame. A row is closed only when its acceptance criterion is met and its legacy implementation is deleted or isolated behind a documented compatibility/test adapter.

## Severity definitions

| Severity | Meaning |
|---|---|
| Stop-the-line | Can produce wrong language results, corrupt/read malformed bytecode, or make later work unsafe. Fix before new feature work. |
| High | Breaks semantic consistency, module isolation, soundness, or makes the required redesign harder. |
| Medium | Causes maintainability/performance/diagnostic debt but can be contained during migration. |
| Deferred | Valid future work, explicitly blocked on an earlier architecture phase. |

## A. Source, lexer, parser, and syntax tree

| ID | Severity | Current gap / evidence | Required outcome | Planned phase |
|---|---|---|---|---|
| SYN-01 | Stop-the-line | `ParserImpl::expression()` / `binaryExpression()` has no precedence or associativity table. `2 * 3 + 4` parses as `2 * (3 + 4)`. | Pratt/precedence parser; AST-shape tests for every precedence group. | R1, R2 |
| SYN-02 | High | `<...>` generic arguments versus comparison use lookahead heuristics; `acceptGT()` mutates `>>` tokens in place. | Context-aware token cursor and separate expression/type grammar; original token stream remains immutable. | R2 |
| SYN-03 | High | Numeric suffixes advertise unsupported `i128`, `f16`, `f128`, `f256`; `i128` is parsed with `stoll`. Exponent parsing is incomplete. | Exact literal representation, explicit supported numeric set, checked conversion and diagnostic spans. | R1, R2, R4 |
| SYN-04 | High | Parser nodes get positions after token consumption and only hold start line/column. Several diagnostics report `0:0`. | `SourceFileId + ByteRange` span on every token and syntax node; diagnostics carry labels. | R2 |
| SYN-05 | Medium | `type` syntax multiplexes alias, structural record, newtype, abstract declaration, native opaque type, and tagged union based on heuristics. | Explicit grammar selected in decision D-001; distinct syntax nodes. | R2; blocked by D-001 |
| SYN-06 | Medium | Parser loops and ad-hoc `accept` errors do not provide structured recovery. REPL manually waits for semicolon/brace counts. | Recoverable parser with EOF token, synchronization points, and incomplete-input result for REPL/LSP. | R2 |
| SYN-07 | Medium | AST ownership is switchable raw/shared pointer infrastructure but the real code path is shared ownership plus manual historical destructors. | One ownership model (`unique_ptr` syntax tree plus shared source storage); no `destroyast` dual mode. | R2 |
| SYN-08 | Medium | Lexer uses C character classification on `char`, may be locale/signed-char sensitive; literal escaping/Unicode policy is unspecified. | ASCII lexical policy now; separately designed Unicode identifier/string policy. | R2; Unicode decision later |

## B. Name resolution, types, generics, const evaluation, and ownership

| ID | Severity | Current gap / evidence | Required outcome | Planned phase |
|---|---|---|---|---|
| TYP-01 | High | `src/typecheck/typecheck.cpp` is a 7k+ line God object combining modules, symbols, types, traits, const evaluation, ownership, and artifacts. | Staged resolver/typechecker with owned session state and narrow APIs. | R3, R4, R5 |
| TYP-02 | Stop-the-line | Syntax AST is mutated with resolved callee names, mangled generic instances, and const-if results. Rechecking/reusing AST changes behavior. | Syntax AST immutable; resolved/typed facts live in HIR and side tables. | R4 |
| TYP-03 | High | Type/module/generic identities often use `Str repr()` and ad-hoc parsing. | Interned `ModuleId`, `DefId`, `TypeId`, `ConstValueId`, and canonical generic `InstanceId`. | R3, R4 |
| TYP-04 | High | `.ngo` type metadata is restored by `type_from_repr()`, which cannot faithfully reconstruct nominal, trait, effect, generic, or ownership facts. | Structured semantic interface metadata, versioned independently from display strings. | R4, R7 |
| TYP-05 | High | Typechecker uses static mutable maps for specializations, prelude state, artifacts, and active module checks. | `CompilationSession`; no mutable process-global checking state. | R3, R4 |
| TYP-06 | High | Import failure may degrade imports to `Untyped`, hiding ordinary source errors until runtime. | Strict default resolution; explicit test/interactive opaque-module mode only. | R3 |
| TYP-07 | High | Compiler reimplements partial type inference from strings (`infer_expression_type_name`, conventional `T/U/V/W`). | Lowering consumes resolved call/type/instance IDs from typed HIR. | R4, R7 |
| TYP-08 | High | Const function evaluation calls the full STUPID interpreter and therefore shares runtime/module/native complexity. | Restricted typed-HIR `ConstEvaluator` with deterministic capability set and resource limits. | R5 |
| TYP-09 | High | Move and borrow facts are encoded in `Set<Str>` alongside bindings. It is difficult to model aliases, joins, and diagnostics robustly. | Explicit places, loans, move paths, and CFG dataflow state. | R5 |
| TYP-10 | Medium | Child typecheck scopes copy broad maps repeatedly and semantically unrelated contexts are recreated ad hoc. | Persistent/stack scope environments plus explicit dataflow facts. | R4, R5 |
| TYP-11 | Medium | Trait coherence, default methods, auto traits, and dispatch are partly duplicated across typechecker/compiler/STUPID. | One trait solver result and dispatch plan in typed HIR. | R4, R7, R8 |
| TYP-12 | Deferred | Effect system, hidden borrow-scope inference, unsafe operations, and error propagation are not first-class semantic concepts. | Introduce minimal effects/capabilities and hidden scoped-borrow facts before FFI/concurrency; lifetime syntax is explicitly out of scope. | R5, R9, R10 |

## C. Runtime values, ownership, GC, and STUPID

| ID | Severity | Current gap / evidence | Required outcome | Planned phase |
|---|---|---|---|---|
| RUN-01 | High | `StorageCell` is simultaneously a frame slot, byte storage, object graph node, module carrier, GC record, native handle, type carrier, and lifecycle state. | Separate slot, value representation, heap object, type descriptor, module instance, and GC metadata. | R6 |
| RUN-02 | High | `TypeLayout`/`HeapStore` are largely separate from production aggregate representation (`opaqueRefs`, `namedRefs`, `bytes`). | Choose one authoritative runtime representation; layout metadata and tracing projections must agree. | R6 |
| RUN-03 | High | Copy/move/drop rules depend on storage class and clone path; `transfer_reference_ownership()` is empty. | Explicit type-directed copy/move/drop/trace operations and ownership states. | R5, R6 |
| RUN-04 | High | Drop/finalizer and sequence/member dispatch logic is duplicated in STUPID and VM. | Shared runtime services used by both execution engines. | R6, R8 |
| RUN-05 | High | STUPID imports execute modules using nested interpreter instances and process-global module registry entries. | Per-session module instantiation and exactly-once initialization state. | R3, R6 |
| RUN-06 | High | REPL parses then directly executes STUPID AST without the normal typecheck pipeline. | Incremental parse/resolve/typecheck/HIR execution in one session. | R2, R4, R8 |
| RUN-07 | Medium | GC roots rely on registered callbacks and `shared_ptr` object graphs; native/module state tracing policy is implicit. | Heap-owned tracing API, explicit roots, finalizer constraints, and no hidden global roots. | R6 |
| RUN-08 | Medium | Dynamic structural classification is based partly on layout kind/name heuristics. | Type descriptor kind and explicit runtime representation tags. | R6 |
| RUN-09 | Deferred | Precise moving GC, pinning, stack maps, deterministic destructors, and allocator strategy are not designed. | Start with non-moving traced heap plus explicit pinned/native region; decide future collector behind runtime API. | R6, R9 |

## D. Modules, artifacts, compiler, bytecode, and VM

| ID | Severity | Current gap / evidence | Required outcome | Planned phase |
|---|---|---|---|---|
| MOD-01 | High | `ModuleArtifact`, `ModuleInfo`, runtime module cell, registry entry, and cache overlap in responsibility. | Immutable `ModuleArtifact` plus per-`RuntimeSession` `ModuleInstance`. | R3, R6 |
| MOD-02 | High | File/module/native registries and caches are process-global and have unclear invalidation/isolation. | Resolver and artifact cache scoped to `CompilationSession`; runtime instances scoped to `RuntimeSession`. | R3, R6 |
| MOD-03 | High | Source and bytecode imports have different semantic fidelity and initialization behavior. | Same exported interface descriptor and module-init protocol for source/native/bytecode artifacts. | R3, R4, R7 |
| BC-01 | Stop-the-line | `BytecodeModule::merge()` hand-decodes instructions with missing opcode widths/remaps; imports and float constants are not fully merged. | Disable as production linker until replaced by schema-driven linker or remove merge in favor of runtime linking. | R1, R7 |
| BC-02 | Stop-the-line | VM bounds checks are inconsistent; several string-index paths index pools directly. | Mandatory artifact verifier and checked decoder before execution. | R1, R7 |
| BC-03 | High | VM scans raw instruction bytes to discover global count; operand bytes can resemble opcodes. | Global/frame metadata explicitly emitted and verified. | R1, R7 |
| BC-04 | High | Reusing a VM/compiler can retain stack/global/type/import state. | Per-run `VmExecution` and per-compile lowering context; deterministic reset tests. | R1, R7 |
| BC-05 | High | Compiler directly emits bytecode from AST and silently depends on mutable member state. | FlowIR-to-bytecode lowering with block/slot metadata and source maps. | R5, R7 |
| BC-06 | High | Opcode definitions, VM execution, module merge, docs, and tests maintain separate instruction knowledge. | Single opcode schema generates/validates all consumers. | R1, R7 |
| BC-07 | Medium | Artifact scalars are written in host representation while bytecode itself uses little-endian conventions. | Fixed-endian, versioned artifact v3 with strict limits and target ABI metadata. | R7 |
| BC-08 | Medium | No stable debug/source map, stack trace, disassembler, or profiling boundary. | Span tables and verified instruction offsets in artifact format. | R7, R11 |

## E. Native binding, C ABI, concurrency, and tooling

| ID | Severity | Current gap / evidence | Required outcome | Planned phase |
|---|---|---|---|---|
| FFI-01 | High | `NGCallable` exposes `StorageCell`, `RuntimeEnv`, and vector-of-slots as the host ABI. | Descriptor-based NG runtime ABI separate from C ABI; host bindings see typed handles/views. | R6, R9 |
| FFI-02 | High | VM `wrap_native` and STUPID native helpers use separate marshaling paths with incomplete argument validation. | One generated/descriptor-driven marshaler and one ownership/error contract. | R6, R9 |
| FFI-03 | High | Native opaque aliases do not yet state whether they represent by-value C object, pointer, owned handle, borrowed handle, or capability. | `opaque` declaration has explicit representation/ownership; by-value C records use a separate `repr(C)` facility. | R9; blocked by D-004 |
| FFI-04 | High | Existing C ABI proposal assumes raw `*T` and runtime calls but does not define callbacks, strings, aggregate layout, panic policy, or GC pinning. | ABI-safe type subset, unsafe boundary, callback policy, string/slice descriptors, and target ABI descriptor. | R9 |
| CON-01 | Deferred | Current globals, caches, GC, and module instances are not safe foundations for concurrency. | RuntimeSession isolation and ownership transfer before language-level tasks. | R6, R10 |
| CON-02 | Deferred | Current spawn proposal creates isolated VMs but does not specify `Send`/sharing, cancellation, structured lifetime, or foreign handles. | Structured concurrency, task-local sessions, `Send`/`Sync`-like capabilities, and deterministic transfer semantics. | R10; blocked by D-005 |
| TOOL-01 | Medium | Formatter/LSP/docgen/debugger plans depend on unstable parser/typechecker/bytecode APIs. | Stable syntax, HIR query APIs, source maps, and incremental sessions first. | R2–R4, R7, R11 |
| DOC-01 | Medium | User docs describe stale opcodes, runtime behavior, and FFI capabilities. | Generated opcode/ABI/reference docs and phase-gated language documentation. | R1, R7, R9, R11 |

## R0 characterization requirements

Before R1 changes, add tests that make the current differences explicit:

1. Parser AST tests for precedence, associativity, assignment, generic-vs-comparison, and nested `>>`.
2. Numeric literal corpus: limits, suffixes, exponent signs, invalid characters, overflow, and source spans.
3. STUPID/HIR-or-VM differential corpus for arithmetic, control flow, moves, refs, traits, modules, tagged unions, and native errors.
4. VM/compiler reuse tests in one process.
5. Artifact verifier corpus: every opcode, truncated operands, bad pools, bad jump targets, bad exports, and fuzzed artifacts.
6. Module tests: artifact-versus-source interface equality, initialization exactly once, import cycles, aliasing, and session isolation.
7. Runtime lifecycle tests: copy/move/drop graph cases, finalizer failures, native handles, and GC root reachability.

The test corpus is not compatibility policy. It is evidence used to choose and document vNext semantics in the decision log.
