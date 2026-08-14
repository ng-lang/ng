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
| Typed functions, direct calls, returns | `01`, `03`, `12`, `58` | Supported for scalar values and initial string/array/tuple aggregate returns | General descriptor-directed runtime ABI and ownership rules remain. |
| Local scopes and shadowing | `09` | Supported in function blocks | Module globals remain a module/session feature. |
| Mutable local bindings | `09`, `10` | Supported as `let mut` / `:=`, including checked array/tuple/struct places and dereferenced reference places | Member/deref places follow nominal types and references. |
| Strings | `04`, `05`, `07`, `11`, `18` | Supported for literals, concatenation, equality, calls, artifacts, and CLI values | Descriptor-directed storage and broader string APIs remain. |
| Arrays and indexing | `06`, `18`, `24`, `56`, `58`, `59` | Dynamic/fixed canonical types, nested literals, reads, mutable index places, and checked bounds are supported | Affine move/clone policy, slices, and descriptor-directed storage remain. |
| Structural tuples | `14`, `50`, `54` | Heterogeneous literals, canonical layouts, numeric projections, mutation, destructuring, calls, and artifacts are supported | Spread/rest patterns, partial move paths, and `.size` remain. |
| Arithmetic, comparison, prefix, logical, bitwise | `03`, `10`, `58` | Supported for checked `i64` | Short-circuit and overflow diagnostics are implemented; no float/suffix compatibility. |
| `if`, lexical `loop`, `next`, tail recursion | `02`, `10`, `12` | Supported in vNext grammar | Legacy shorthand loop grammar is intentionally not accepted. |
| Bytecode modules and direct calls | `01`, `03` | Supported | Artifact now has a versioned verified scalar/module encoding. |
| CLI `main` arguments | no dedicated example | Supported for typed `i64` and `string` arguments | Aggregate CLI ABI follows RuntimeSession/descriptor work. |

## Ordered feature ladder

| Priority | Feature family | Legacy examples | Blocking architectural work | Target phase |
|---|---|---|---|---|
| 1 | Strings and immutable string operations | `04`, `05`, `07`, `11`, `18` | Introduce one tagged vNext `Value` representation, string constant pool, typed string operations, and VM error/ABI rules. | R6/R7 slice |
| 2 | Arrays, indexing, bounds checks, place assignment | `06`, `18`, `24`, `56`, `58`, `59` | Sequence descriptors, aggregate values, index place lowering, copy/move policy. | R5/R6/R7 slice |
| 3 | Tuples and structural product values | `14`, `50`, `54` | Typed tuple layouts, numeric/member projection, destructuring and partial move paths. | R4–R6 |
| 4 | Structs, enums, constructors, pattern matching | `07`, `11`, `16`, `20`, `21` | Explicit `struct`/`enum` declarations, qualified constructors, and `switch` dispatch (`case Variant(binding)` / `otherwise`, exhaustiveness checked, payload bindings) are supported (`example/vnext/enum_match.ng`); recursive tagged unions and structural patterns (tuple/literal/or) remain. | R4–R7 |
| 5 | Source modules, imports, exports, prelude | `08`, `13`, `18`, `56`, `59` | `CompilationSession`, module graph/interface, immutable artifact versus runtime instance. | R3/R6/R7 |
| 6 | `ref`, places, move/copy/drop | `11`, `21`–`24`, `39`, `41`, `50`, `51` | Scoped `ref`/`ref mut`/`*` places with cell-backed bindings and deep-copy bind/call/return semantics run end to end (`example/vnext/ref_swap.ng`, `ref_places.ng`); `move`, `clone`, `Drop` lifecycle contracts, and loan conflict checks remain. No user-visible lifetime syntax. | R5/R6 |
| 7 | Ordinary generics and canonical specialization instances | `15`, `43`, `44` | Generic enum instances, generic function definitions, direct call inference, type substitution, explicit generic call arguments (`name<types>(...)`, type and const), and expression bodies (`=>`) are supported in the vNext slice | `GenericDefId`/`InstanceId` graph, overload sets, partial specialization, and monomorphized artifacts remain. | R4 |
| 8 | Restricted const execution and `const if` | `17`, `42`, `46`, `47`, `53` | `const if` folding, module-level `const` predicates with pattern specialization (exact/pattern/primary priority, repeated parameters, `= delete`), prefix `ref<T>` sugar, `const fun` (typed-HIR interpreter with loops, recursion, tail recursion, and runtime callability), and `where` clauses (predicates, `T is Type`, negation, const fun calls over const parameters, checked per instance and at module level) are supported (`example/vnext/const_predicates.ng`, `example/vnext/const_fun.ng`, `example/vnext/where_clauses.ng`); `= native` hosts, trait bounds (`T: Trait`), generic const fun compile-time calls, and per-instance `const if` remain. | R5 |
| 9 | Const generics | `46`, `47`, `53`, `54` | Const substitution in `InstanceId`, typed const parameter/value equality, ABI/layout rules. | R4/R5 |
| 10 | Variadic type/value packs, ranges/slices, folds | `49`, `54`, `57`, `58`, `59` | Pack kinds/substitution, aggregate descriptors, checked slice places, effect/move-aware expansion and fold lowering. | R4–R7 |
| 11 | Higher-kinded generics | `48`, `49` | Explicit kind system and kind-checked type constructor application. | R4 |
| 12 | Static traits and generic bounds | `25`–`33`, `37`–`39`, `46`, `55`, `59` | Trait declarations, impls with coherence, supertraits, default methods, qualified calls, static dispatch on concrete receivers, and `T: Trait` bounds in generics/where clauses are supported (`example/vnext/traits.ng`); method calls through type parameters (monomorphization), generic impls, auto/derive, and `ref<Trait>` dynamic views remain. | R4–R7 |
| 13 | Abstract trait types and `ref<Trait>` dispatch | `34`–`36`, `40`, `59` | Abstract-type legality, object-safety, checked reference coercion, immutable vtable descriptor, reference-view ABI. | R4–R7 |
| 14 | FFI and opaque/native handles | `39`, `45`, `51` | Declared native ABI, capabilities, handle descriptor/lifecycle policy. | R9 |
| 15 | Concurrency | none in the stable legacy corpus | RuntimeSession isolation, ownership transfer/capabilities, experimental-only design. | R10 |

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
- Do not introduce `dyn Trait`, `Box<dyn Trait>`, or a separately owning erased
  trait-value container. A trait is an abstract type; `ref<Trait>` is the only
  dynamic-trait value form in the initial design and is a non-owning reference
  view to a concrete referent plus immutable dispatch metadata.

## Acceptance gate for each ladder row

1. A vNext-only feature test covers parsing, resolution, type validation,
   lowering, bytecode, and VM execution.
2. Edge cases include exact diagnostics and source spans, malformed artifacts,
   and runtime exceptional paths where relevant.
3. The feature has one authoritative vNext runtime/ABI representation; it does
   not call legacy STUPID, legacy ORGASM, or a compatibility shim.
4. The relevant row above is updated only after the full test suite passes and
   the focused implementation commit is created.
