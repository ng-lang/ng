# Legacy Example Migration Matrix

> **Status:** migration complete — the legacy corpus has been removed and every
> surviving feature surface is implemented on the vNext pipeline. Remaining
> edge work is tracked in the
> [post-cutover plan](07-post-cutover-plan.md), not here.
>
> **AI-assisted document:** drafted with AI assistance and based on the
> former `example/*.ng` corpus. Legacy syntax and output were evidence, not a
> vNext compatibility commitment.

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
| Arrays and indexing | `06`, `18`, `24`, `56`, `58`, `59` | Dynamic/fixed canonical types, nested literals, reads, mutable index places, checked bounds, value spreads splicing range/slice elements into array literals, and value-semantics append (`xs << value`, legacy 52's `<<`) (`[...(1..5)]`, `[0, ...xs, 9]`, `example/std_string.ng`) are supported | Affine move/clone policy and descriptor-directed storage remain. |
| Structural tuples | `14`, `50`, `54` | Heterogeneous literals, canonical layouts, numeric projections, mutation, destructuring (incl. rest patterns `let (first, ...rest) = tuple;` binding the remaining elements as a tuple, `example/tuple_rest_patterns.ng`), calls, and artifacts are supported | Partial move paths remain. |
| Arithmetic, comparison, prefix, logical, bitwise | `03`, `10`, `58` | Supported across the fixed-width integer tower (`i8`–`i64`, `u8`–`u64`) and `f32`/`f64` with contextual literal typing, numeric suffixes, per-width range checks, and cross-width numeric equality/ordering (`example/numeric_types.ng`, `example/floats.ng`) | `isize`/`usize` remain. |
| `if`, lexical `loop`, `next`, tail recursion | `02`, `10`, `12` | Supported in vNext grammar | Legacy shorthand loop grammar is intentionally not accepted. |
| Bytecode modules and direct calls | `01`, `03` | Supported | Artifact now has a versioned verified scalar/module encoding. |
| CLI `main` arguments | no dedicated example | Supported for typed `i64` and `string` arguments | Aggregate CLI ABI follows RuntimeSession/descriptor work. |

## Ordered feature ladder

| Priority | Feature family | Legacy examples | Blocking architectural work | Target phase |
|---|---|---|---|---|
| 1 | Strings and immutable string operations | `04`, `05`, `07`, `11`, `18` | Introduce one tagged vNext `Value` representation, string constant pool, typed string operations, and VM error/ABI rules. | R6/R7 slice |
| 2 | Arrays, indexing, bounds checks, place assignment | `06`, `18`, `24`, `56`, `58`, `59` | Sequence descriptors, aggregate values, index place lowering, copy/move policy. | R5/R6/R7 slice |
| 3 | Tuples and structural product values | `14`, `50`, `54` | Typed tuple layouts, numeric/member projection, destructuring and partial move paths. | R4–R6 |
| 4 | Structs, enums, constructors, pattern matching | `07`, `11`, `16`, `20`, `21` | Explicit `struct`/`enum` declarations, qualified constructors, and `switch` dispatch (`case Variant(binding)` / `otherwise`, exhaustiveness checked, payload bindings) are supported (`example/enum_match.ng`), multi-field variants with tuple payloads, recursive tagged unions through `ref<Node<T>>` payloads with positional switch destructuring (`example/recursive_enums.ng`), union-type annotations (`A | B`, member-typed construction, equality/ordering against members, parameter flow, `example/unions.ng`), and scalar literal-or switch patterns (`case 1 | 2` / `case "a"` / `case true` / `case -1` over integer, bool, and string scrutinees with optional `otherwise`, typed literal adoption with range checks, duplicate and mixed-pattern diagnostics, `example/switch_patterns.ng`); tuple patterns remain. | R4–R7 |
| 5 | Source modules, imports, exports, prelude | `08`, `13`, `18`, `56`, `59` | Transitive `import` loading (`import name;` / selective list), `export` parsing, merged multi-module compilation with deterministic cycle/missing-module diagnostics, and a vNext stdlib (`lib/std`: prelude/io/string with print/assert and string utilities) reachable through the `lib/std` search path (`example/modules/`, `example/stdlib_basics.ng`), name-privacy enforcement (per-module visible-name sets: own names plus selective import lists or imported exported surfaces with transitive re-export; non-exported functions are invisible to importers), and a redesigned stdlib (`lib/std`: string length/charAt/substring/toLower, seq sum/arrayContains, a recursive-enum List<T> with length/get/contains plus immutable builders (listFrom/pushFront/append/reverseList, `example/list_builders.ng`), and a GC-free memory module with native handles and a Drop-released Box, `example/std_list.ng`/`std_seq.ng`/`heap_box.ng`); module interfaces/artifacts remain. | R3/R6/R7 |
| 6 | `ref`, places, move/copy/drop | `11`, `21`–`24`, `39`, `41`, `50`, `51` | Scoped `ref`/`ref mut`/`*` places with cell-backed bindings and deep-copy bind/call/return semantics run end to end (`example/ref_swap.ng`, `ref_places.ng`); `move`/`clone` expressions with affine use-after-move and partial-move checking (branch/loop/switch merging, assignment revival), `impl Drop` execution at returns and fall-through for live affine values, and simple borrow conflict checks (shared/mut exclusivity, block-scoped release, escaped-reference rejection) run end to end (`example/move_semantics.ng`, `example/drop_raii.ng`); field-aware partial-move drops and block-scoped drop edges are done; non-lexical loan release landed as its first slice (loans release at the ref's last use, inline call-site refs release after their statement, `example/nll_borrows.ng`), with returning refs, origin contracts, and aggregate ref fields remaining. A GC-free native-handle Box with Drop-driven release covers the legacy 41 spirit (`example/heap_box.ng`); generic `Box<T>` and Gc/Arc stay deferred to the R6 heap domain. No user-visible lifetime syntax. | R5/R6 |
| 7 | Ordinary generics and canonical specialization instances | `15`, `43`, `44` | Generic enum instances, generic function definitions, direct call inference, type substitution, explicit generic call arguments (`name<types>(...)`, type and const), and expression bodies (`=>`) are supported in the vNext slice | `GenericDefId`/`InstanceId` graph, overload sets, partial specialization, and monomorphized artifacts remain. | R4 |
| 8 | Restricted const execution and `const if` | `17`, `42`, `46`, `47`, `53` | `const if` folding, module-level `const` predicates with pattern specialization (exact/pattern/primary priority, repeated parameters, `= delete`), prefix `ref<T>` sugar, `const fun` (typed-HIR interpreter with loops, recursion, tail recursion, and runtime callability), and `where` clauses (predicates, `T is Type`, negation, const fun calls over const parameters, checked per instance and at module level) are supported (`example/const_predicates.ng`, `example/const_fun.ng`, `example/where_clauses.ng`), and per-instance `const if` inside const-generic functions evaluates against each monomorphized instance's const bindings (`example/const_if_instances.ng`); const-capable native hosts evaluate pure string natives at compile time (`example/const_native_hosts.ng`), and generic const funs evaluate at compile time per type argument with concrete where-clause checks and per-instance deferral (`example/generic_const_fun.ng`); trait bounds (`T: Trait`) in const predicates remain. | R5 |
| 9 | Const generics | `46`, `47`, `53`, `54` | Const substitution in `InstanceId`, typed const parameter/value equality, ABI/layout rules. | R4/R5 |
| 10 | Variadic type/value packs, ranges/slices, folds | `49`, `54`, `57`, `58`, `59` | Heterogeneous variadic type packs (`T...` parameters/returns, tuple-literal spreads, per-instance tuple lowering) call-site tuple spreads with static flattening, `range<i64>` values with checked array slicing, array map comprehensions with range sources and filter markers (`[f(xs)...]`, `[f(r)...]`, `[p(xs)?...]` lowered to runtime loops), and fold calls (`f(acc, xs...)` left folds / `f(xs..., acc)` right folds over arrays, slices, and ranges, lowered to runtime loops with per-element calls and accumulator threading) are supported (`example/variadic_packs.ng`, `example/ranges_slicing.ng`, `example/map_comprehension.ng`, `example/folds.ng`), built-in tuple/pack introspection (`is_tuple<T>`, `tuple_size<T>`, `sizeof_pack<T...>` as const predicates plus `tuple_element<T, I>`/`tuple_concat<A, B>` type constructors, `example/enhanced_tuples.ng`) are supported; the legacy 58 surface (maps, filters, folds, ranges, slices, mixed comprehensions) is fully covered. | R4–R7 |
| 11 | Higher-kinded generics | `48`, `49` | Generic structs (`struct Box<T>`) instantiate per concrete argument list, and `F<_>` type-constructor parameters of kind `* -> *` apply as `F<T>` in parameter/return types with explicit (`accept<Box, i64>(ref box)`, declaration order) and inferred (`accept(ref box)`) instantiation (`example/generic_structs.ng`, `example/hkt.ng`), variadic constructor kinds (`F<_, ...>`) with parameterized opaque templates (`type Variadic<Head, Tail...> = native;`, per-instance instantiation) (`example/variadic_hkt.ng`). | R4 |
| 12 | Static traits and generic bounds | `25`–`33`, `37`–`39`, `46`, `55`, `59` | Trait declarations, impls with coherence, supertraits, default methods, qualified calls, static dispatch on concrete receivers, and `T: Trait` bounds in generics/where clauses are supported, and method calls through bounded type parameters monomorphize per concrete instance (`example/traits.ng`), auto traits (`auto trait Send {}`, implicit for every concrete type) with `derive(Copy + Clone)` synthesized impls and a deep-copying `clone()` method (`example/derive.ng`), and `ref<Trait>` dynamic views with per-concrete dispatch tables, view coercions (bindings, parameters, array elements), and dynamic method calls (`example/trait_objects.ng`); default methods through views now dispatch against per-concrete instantiated defaults (`view.bracketed()`, `example/trait_defaults.ng`), and generic impls (`impl<T> Trait for List<T>` / `array<T>`) instantiate per concrete receiver, satisfy trait bounds, and feed trait-view tables with concrete impls winning (`example/generic_impls.ng`); Self-typed view methods remain. | R4–R7 |
| 13 | Abstract trait types and `ref<Trait>` dispatch | `34`–`36`, `40`, `59` | `ref<Trait>` views coerce from implementing values (bindings, call arguments, array elements) and dispatch methods dynamically through per-concrete dispatch tables embedded in the bytecode module (`example/trait_objects.ng`); bare trait values are rejected, non-implementors and unknown methods are type errors, default methods through views run per-concrete instantiations (`example/trait_defaults.ng`), and Self-typed view methods remain. | R4–R7 |
| 14 | FFI, opaque/native handles, and host bindings | `39`, `45`, `51`, imgui corpus | `native fun` runtime intrinsics with a registry and `print`/`assert` builtins (`example/native_io.ng`), opaque type declarations (`type X = native;` native handles, `type X;` abstract types) with built-in `is_trait<T>`/`is_abstract<T>` const predicates (`example/opaque_types.ng`); declared C ABI (`extern "C"`) and native handle lifecycle policies remain. A redesigned Dear ImGui binding over the SDL3 GPU backend (`lib/std/imgui.ng` + `src/imgui_natives.cpp`), the `ngi_imgui` frontend, a self-hosting `runNgi` native (compile+run nested NG source from NG code, diagnostics captured as a string), an unlimited-fuel `--fuel 0` mode, and a minimal NG IDE built on the binding (`example/ng_ide.ng`, runnable end to end and CI-covered through headless stub natives in `test/imgui_binding_test.cpp`) validate the host-boundary slice. | R9 |
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
