# NG Core Rearchitecture Index

> **Status:** in progress — the vNext pipeline is now the only pipeline; the legacy orgasm/interpreter implementation, stdlib, tests, and example corpus have been removed (R8 complete). Remaining work is planned in the [post-cutover plan](07-post-cutover-plan.md).
>
> **Scope:** this is the authoritative plan for the compiler pipeline, runtime, module system, bytecode VM, native ABI, and the prerequisites for safe concurrency. It is intentionally a **vNext design without source or binary compatibility guarantees**.
>
> **AI-assisted document:** drafted with AI assistance; every implementation change derived from it still requires normal review, tests, and explicit design decisions.

## Why this plan exists

The repository has a broad feature surface and a passing test suite, but the current architecture has crossed the point where additive work is safe:

- syntax ASTs are mutated by type checking and consumed directly by both execution backends;
- type checking, STUPID, the bytecode compiler, and the VM independently implement overlapping semantics;
- module artifacts, module runtime state, caches, and process-global registries have no clean ownership boundary;
- runtime layouts and the actual `StorageCell` object graph are two divergent representations;
- bytecode instruction decoding is duplicated and incomplete;
- the existing `= native` API is coupled to `StorageCell`, VM calling conventions, and `std::function` rather than a declared ABI.

This plan uses a **replacement-first rewrite**, not an additive shim strategy. Each phase creates the new owned implementation and its tests in new source boundaries, proves the full suite, then deletes the replaced legacy implementation in the same phase or immediately after a short differential-test window. Compatibility with current source syntax, bytecode, runtime APIs, and internal structures is not a goal. The old implementation is a temporary differential oracle only; no compatibility adapter may become a permanent production path.

## Documents

| Document | Purpose |
|---|---|
| [Gap register](00-gap-register.md) | Audited defects, missing boundaries, risks, and the phase that closes each gap. |
| [Target architecture and HIR](01-target-architecture-hir.md) | Syntax AST, resolved/typed HIR, FlowIR, type identity, const evaluation, ownership, and lowering contracts. |
| [Runtime, modules, and native ABI](02-runtime-module-ffi.md) | RuntimeSession, ModuleArtifact/ModuleInstance split, values, GC, lifecycle, C ABI, opaque wrappers, and native registration. |
| [Delivery plan](03-delivery-plan.md) | Executable phases, entry/exit criteria, concrete work packages, tests, and deletion gates. |
| [Language decisions required](04-language-decisions.md) | Syntax and semantic choices that need owner confirmation before their phase begins. |
| [Legacy example migration matrix](05-legacy-example-migration-matrix.md) | vNext feature ladder derived from the legacy example corpus; evidence, not compatibility policy. |
| [Const evaluation and generic instances](06-const-evaluation-and-generic-instances.md) | ConstValue identity, restricted evaluator, const generics, and generic instance construction. |
| [Post-cutover plan](07-post-cutover-plan.md) | Remaining vNext work after legacy removal: decided-edge completion, gated decisions, and unstarted phases. |
| [QBE native backend](08-qbe-native-backend.md) | Decision for QBE over LLVM, FlowIR → QBE IL lowering, tiered value representation, the native/ABI rewrite, workflow and artifacts, and the CMake toolchain integration. |

## Progress board

`Not started` means no code change is authorized by that phase yet. A phase may not be marked complete merely because its tests pass: it must satisfy its removal gate and documentation gate.

| ID | Phase | Status | Depends on | Exit artifact |
|---|---|---|---|---|
| R0 | Governance, invariants, and characterization | Complete — decisions accepted; characterization carried by the migration matrix and the example sweep | — | Baseline test matrix and RFC decisions |
| R1 | Correctness and bytecode safety stop-the-line fixes | Complete — the vNext pipeline replaced the legacy frontends; verifier and safe decoding ship | R0 | Pratt parser, verifier, safe VM reset/decoding |
| R2 | Source model, lexer, parser, and syntax AST | Complete — immutable syntax nodes with spans; modules/items/expressions/blocks/types all parse | R1 | Immutable syntax tree with spans and recovery |
| R3 | CompilationSession and module graph | Complete — transitive `import` loading with per-module visibility (exports, selective imports, transitive re-export) and deterministic diagnostics | R1 | Session-scoped resolver/artifact cache |
| R4 | Symbols, types, resolved HIR, and typed HIR | Complete — canonical `DefId`/`LocalId`/`TypeId` identity, side-table typed HIR, monomorphized instances | R2, R3 | Canonical IDs and no semantic AST mutation |
| R5 | Const evaluator, ownership analysis, and FlowIR | Complete — const evaluation (incl. generic const funs and const-capable hosts), affine ownership with partial moves and Drop, NLL borrow release, folds/map comprehensions, switch lowering | R1 | Verified per-function CFGs and const evaluation |
| R6 | RuntimeSession and module instances | Partial — one runtime value model ships (`Value`); the session-scoped artifact/instance split and heap domains remain | R3, R5 | One runtime value model and lifecycle service |
| R7 | Bytecode v3 compiler, VM, and artifact format | Complete — verified bytecode, versioned artifacts, dispatch tables, and the interpreter; runtime/module interface descriptors remain with R6 | R5, R6 | Verified bytecode consuming FlowIR |
| R8 | Reference interpreter convergence and legacy removal | Complete — legacy sources, headers, stdlib, tests, and example corpus removed; `ngi`/`ng_test` link only vNext | R5, R6, R7 | vNext-only repository; post-cutover plan |
| R9 | Native ABI, C FFI, opaque types, and bindgen | Partial — native intrinsics with registries, const-capable hosts, opaque handles, the imgui binding and NG IDE ship; declared C ABI (`extern "C"`), `repr(C)`, and bindgen remain | R4, R6, R7 | Declared ABI descriptors and safe C boundary |
| R10 | Concurrency-native runtime and language layer | Not started | R5, R6, R7, R9 | Structured concurrency with ownership-aware transfer |
| R11 | Tooling, documentation, performance, and stabilization | Partial — documentation rewritten (`docs/guide`, `docs/ref`), legacy docs archived, VitePress refreshed; formatter/LSP/debug metadata/benchmarks remain | R2–R10 as applicable | Formatter/LSP/debug metadata/benchmarks |

## Non-negotiable architectural invariants

Every phase must preserve or establish these invariants:

1. **One-way dependencies.** Syntax does not depend on semantic analysis; semantic analysis does not depend on runtime execution; lowering does not infer types.
2. **Syntax is immutable after parsing.** Semantic facts live in HIR, side tables, or arenas, never in parser AST nodes.
3. **Stable identity is not text.** `ModuleId`, `DefId`, `TypeId`, and generic instances are canonical IDs; `repr()` is diagnostic output only.
4. **Replacement executable boundary.** The `ngi` target links only the vNext frontend/runtime libraries. It must never call, link, or fall back to the legacy interpreter path.
5. **One instruction schema.** Encoding, decoding, verification, disassembly, remapping, and bytecode tests use the same opcode descriptor.
6. **One runtime value model.** A value cannot have competing `bytes`, layout offsets, and object-graph meanings without an explicit representation contract.
7. **Artifact is immutable; instance is mutable.** A compiled module can be shared; globals, initialization state, and native state belong to a `ModuleInstance` in one `RuntimeSession`.
8. **Compile-time evaluation is capability restricted.** It evaluates typed IR and `ConstValue`; it cannot accidentally invoke arbitrary runtime/module/IO behavior.
9. **Native boundaries are declared.** Every host callable has a signature, ownership contract, ABI/capability declaration, and error policy.
10. **Unsafe is explicit.** Raw pointers, arbitrary C ABI calls, unchecked layout casts, and shared mutable foreign state require an explicit unsafe boundary.
11. **Concurrency is data-race safe by construction.** Cross-task transfer is governed by ownership/capabilities; process globals are not an implicit sharing mechanism.

## Relationship to older documents

- [`docs/refactoring_plan_2026_06.md`](../../refactoring_plan_2026_06.md) remains a useful record of local cleanup work, but it is **not sufficient** for the vNext boundary redesign and should not be used to justify new feature work that depends on the old AST/runtime coupling.
- Existing documents under `docs/design/archive/` describe implemented historical baselines. They are evidence and migration input, not vNext constraints.
- Existing `gap-*.md` proposals must be reclassified against R0–R11 before implementation. Features such as LSP, formatter, package management, C FFI, and concurrency must not build permanent APIs on the legacy semantic/runtime model.

## How to use this plan

1. Start only the next phase whose dependencies are complete.
2. Create a tracking issue per work package in [the delivery plan](03-delivery-plan.md), including its acceptance tests before implementation starts.
3. Record any language-facing decision in [the decision log](04-language-decisions.md) before changing grammar or public semantics.
4. Build the replacement in a new owned boundary; do not add a production shim to the legacy implementation. Keep old code only for a short test-only differential window, then delete it.
5. Run the complete test suite and relevant sanitizer/documentation checks after every completed vertical slice.
6. Make one focused commit after each successful vertical slice; never include unrelated pre-existing worktree changes.
7. Update this progress board, the relevant acceptance checklist, and user-facing documentation in the same commit that changes phase status.
