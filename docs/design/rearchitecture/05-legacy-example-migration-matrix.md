# Legacy Example Migration Matrix

> **Status:** living vNext migration inventory.
>
> **AI-assisted document:** drafted with AI assistance and based on the current
> `example/*.ng` corpus. Legacy syntax and output are evidence, not a vNext
> compatibility commitment.

## Purpose

The legacy examples cover a much wider language than the current vNext scalar
slice. This matrix prevents an accidental feature order driven by whichever
legacy parser path happens to be easiest to copy. A feature is marked supported
only when it has immutable Syntax, Resolved HIR, typed validation, FlowIR,
bytecode, VM, and negative diagnostics coverage in vNext.

## Current vNext core

| Capability | Evidence in examples | vNext status | Next required work |
|---|---|---|---|
| Typed functions, direct calls, returns | `01`, `03`, `12`, `58` | Supported for `i64`/`bool`/`unit` | Add general runtime values before non-scalar ABI. |
| Local scopes and shadowing | `09` | Supported in function blocks | Module globals remain a module/session feature. |
| Mutable local bindings | `09`, `10` | Supported as `let mut` / `:=` | Add places for member/index assignment. |
| Arithmetic, comparison, prefix, logical, bitwise | `03`, `10`, `58` | Supported for checked `i64` | Short-circuit and overflow diagnostics are implemented; no float/suffix compatibility. |
| `if`, lexical `loop`, `next`, tail recursion | `02`, `10`, `12` | Supported in vNext grammar | Legacy shorthand loop grammar is intentionally not accepted. |
| Bytecode modules and direct calls | `01`, `03` | Supported | Artifact now has a versioned verified scalar/module encoding. |
| CLI integer `main` arguments | no dedicated example | Supported for `i64` | General typed value ABI follows RuntimeSession/Value work. |

## Ordered feature ladder

| Priority | Feature family | Legacy examples | Blocking architectural work | Target phase |
|---|---|---|---|---|
| 1 | Strings and immutable string operations | `04`, `05`, `07`, `11`, `18` | Introduce one tagged vNext `Value` representation, string constant pool, typed string operations, and VM error/ABI rules. | R6/R7 slice |
| 2 | Arrays, indexing, bounds checks, place assignment | `06`, `18`, `24`, `56`, `58`, `59` | Sequence descriptors, aggregate values, index place lowering, copy/move policy. | R5/R6/R7 slice |
| 3 | Tuples and structural product values | `14`, `50`, `54` | Typed tuple layouts, numeric/member projection, destructuring and partial move paths. | R4–R6 |
| 4 | Structs, enums, constructors, pattern matching | `07`, `11`, `16`, `20`, `21` | Explicit declaration grammar, type identities/layout descriptors, match exhaustiveness and enum runtime tags. | R4–R7 |
| 5 | Source modules, imports, exports, prelude | `08`, `13`, `18`, `56`, `59` | `CompilationSession`, module graph/interface, immutable artifact versus runtime instance. | R3/R6/R7 |
| 6 | `ref`, places, move/copy/drop | `11`, `21`–`24`, `39`, `41`, `50`, `51` | Place/move-path/loan dataflow, descriptor-directed lifecycle services. No user-visible lifetime syntax. | R5/R6 |
| 7 | Type aliases/newtypes, generics, const-if | `15`, `17`, `42`–`54` | Type interner, instance graph, restricted typed const evaluator. | R4/R5 |
| 8 | Traits, dynamic trait objects, standard collections | `25`–`41`, `55`, `59` | Trait evidence/dispatch, object-safe ABI, shared runtime descriptors. | R4/R6/R7 |
| 9 | FFI and opaque/native handles | `39`, `45`, `51` | Declared native ABI, capabilities, handle descriptor/lifecycle policy. | R9 |
| 10 | Concurrency | none in the stable legacy corpus | RuntimeSession isolation, ownership transfer/capabilities, experimental-only design. | R10 |

## Deliberate non-goals during the scalar-to-aggregate transition

- Do not copy legacy `val`, implicit top-level execution, numeric suffix
  permissiveness, loop shorthand, or mutable AST behavior merely to make an
  example parse.
- Do not lower index/member syntax to an integer placeholder; it remains a
  typecheck error until the corresponding runtime descriptor and place
  semantics exist.
- Do not introduce source-visible lifetimes while adding `ref` and ownership.
- Do not make imports, globals, or native state process-global in order to run
  legacy module examples.

## Acceptance gate for each ladder row

1. A vNext-only feature test covers parsing, resolution, type validation,
   lowering, bytecode, and VM execution.
2. Edge cases include exact diagnostics and source spans, malformed artifacts,
   and runtime exceptional paths where relevant.
3. The feature has one authoritative vNext runtime/ABI representation; it does
   not call legacy STUPID, legacy ORGASM, or a compatibility shim.
4. The relevant row above is updated only after the full test suite passes and
   the focused implementation commit is created.
