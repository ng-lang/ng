# Language Decisions Required for NG vNext

> **Status:** open decision log.  
> **AI-assisted document:** drafted with AI assistance.
>
> The project explicitly does not require source compatibility for vNext. These decisions should therefore optimize for a coherent long-lived language, not for preserving every current parser heuristic. Do not implement a syntax-facing phase until its blocking decision is accepted.

## Decision process

For each decision:

1. choose one option or record a new option;
2. state the semantic rule and at least three examples/counterexamples;
3. update grammar, HIR, type rules, runtime/ABI rules, and user guide together;
4. add positive and negative tests before implementation;
5. update status below from `Open` to `Accepted` with date and rationale.

## D-001 — Split overloaded `type` declarations

**Status:** Accepted — 2026-07-13  
**Blocks:** R2.6, R4 type construction, R9 opaque/native syntax

### Problem

Current `type` declarations cover aliases, structural objects, newtypes, abstract declarations, native opaque types, and tagged unions. Several forms are recognized heuristically. This prevents a simple grammar and hides important semantic differences.

### Recommended proposal

Use explicit declaration kinds:

```ng
// Transparent alias; type identity is the underlying type.
type Meters = f64;

// Nominal aggregate, value semantics according to Copy/ownership rules.
struct Point {
    x: i32,
    y: i32,
}

// Nominal tagged union / algebraic data type.
enum Result<T, E> {
    Ok(value: T),
    Err(error: E),
}

// Nominal wrapper with explicit representation conversion.
newtype UserId(i64);

// Publicly abstract/opaque language type; constructors are private or factory-only.
opaque type Connection;

// Foreign/native opaque handle; representation/ownership are declared.
extern opaque type FileHandle: pointer;
```

### Why this is recommended

- grammar directly selects syntax node kind;
- type identity/representation/constructor visibility become obvious;
- `enum` enables future pattern, derive, and layout rules cleanly;
- `opaque` naturally supports FFI wrapper types;
- `newtype` can stay zero-cost where backend layout permits, without pretending to be an alias.

### Accepted rules

- `type` means transparent alias only.
- `struct`, `enum`, `newtype`, `opaque type`, and `extern opaque type` are distinct syntax nodes and type kinds.
- `struct` is the selected nominal-record spelling; `record` is not a declaration keyword.
- Constructor syntax, field visibility, and module visibility remain implementation tasks, but may not collapse these declaration kinds back into a heuristic `type` grammar.

---

## D-002 — Ownership defaults, mutable bindings, and references

**Status:** Accepted — 2026-07-13  
**Revised by:** [D-015](#d-015-copy-first-ownership-revision) — 2026-08-14  
**Blocks:** R2 grammar finalization, R5 ownership rules, R6 runtime values, R10 concurrency

### Problem

The current language combines value copying, explicit `move`, `ref`, runtime moved sentinels, and partial moves. For a system language, the ownership model needs a crisp static default before native ABI and concurrency can be sound.

### Hard constraint

**Lifetime parameters, lifetime annotations, and lifetime names in diagnostics must never appear in NG source syntax or normal user-facing type output.** The compiler may use hidden borrow-scope/origin constraints internally, but it must not expose a Rust-style lifetime language.

### Accepted rules

1. Bindings are immutable by default and use `let` / `let mut`:

```ng
let value = Point { x: 1, y: 2 };
let mut total = 0;
```

2. Types implementing `Copy` copy by value. Other values are affine and move on consuming assignment, call, and return:

```ng
let a = Handle.open(...);
let b = a;          // move if Handle is not Copy

let n: i32 = 1;
let m = n;          // Copy
```

3. Safe scoped references use the existing `ref` keyword with canonical postfix type syntax:

```ng
T ref        // read-only scoped reference/view
T ref mut    // exclusive mutable scoped reference/view
ref<T>       // equivalent prefix generic spelling of T ref
```

Expressions use the same keyword:

```ng
let read_only = ref value;
let writable = ref mut value;
```

4. Borrow scope/origin is hidden compiler metadata. Lifetime parameters, annotations, names, bounds, and user-visible lifetime diagnostics are permanently forbidden.

5. A function may declare the owner source for a returned safe reference:

```ng
fun first(items: T[] ref) -> T ref(items) {
    items[0]
}

fun choose(
    a: T ref,
    b: T ref,
    choose_first: bool
) -> T ref(a | b) {
    if choose_first { a } else { b }
}
```

`ref(a)` means the result originates from `a`. `ref(a | b)` means it may originate from either source and is conservatively valid only while every listed source remains valid. Source names are resolved to parameter/receiver indexes in HIR and are not type identity.

6. Omitted return-origin contracts are inferred only for a unique source. Multiple possible sources require an explicit source set. A declared set is checked against every return path.

7. Sources may be parameters, `self`, or explicit owner parameters such as an arena/handle. Locals, temporaries, internal allocations, and undeclared captures cannot become safe return sources. `unsafe` cannot fabricate a safe `T ref`; it may only expose unsafe raw-pointer/foreign operations.

8. Safe refs are scoped and non-escaping. They cannot be stored in globals, heap objects, task payloads, opaque handles, or escaping closures. Higher-order callbacks may accept non-escaping ref captures; an escaping callback must own/copy/shared-own its captures.

9. Arena allocation is callback-scoped rather than lifetime-parameterized:

```ng
with_arena(|arena| {
    let point = arena.alloc(Point { x: 1, y: 2 });
    use(point);
    // point cannot escape this callback scope
});
```

Owning custom allocation uses `Box<T, A>`, `Vec<T, A>`, and related allocator-aware containers. Managed and shared domains are explicit: `Gc<T>` / `Weak<T>` and `Arc<T>` respectively. `ref` is never a heap, GC, or shared-ownership handle.

10. Raw pointers use canonical postfix type syntax:

```ng
T *const    // unsafe pointer with read-only pointee access
T *mut      // unsafe pointer with writable pointee access
```

`const` / `mut` describe pointee access; binding reassignment is controlled independently by `let` / `let mut`:

```ng
let p: i32 *mut = ...;
let mut q: i32 *mut = ...;
unsafe { *p := 1; }
```

### Compiler implementation rule

The compiler uses hidden `LoanId`/borrow-scope/origin facts in HIR and FlowIR. These facts are not generic arguments to source types. Diagnostics say, for example, "borrowed value escapes the scope that owns it" rather than exposing region names.

### Remaining implementation work

- Mutable refs are exclusive; interior mutability is provided by explicit library/runtime types, not implicit aliasing.
- Parser/type checker must reject raw pointers to scoped refs in safe ABI/storage contexts.
- `Drop`, `Copy`, `Clone`, `Send`, allocators, `Gc`, and `Arc` must be encoded as distinct descriptor/capability contracts.

---

## D-003 — `const if`, inactive branches, and const capability policy

**Status:** Accepted — 2026-07-13  
**Blocks:** R5.2–R5.4

### Problem

Current documentation says inactive `const if` branches may be invalid, while generic checking and diagnostics need a predictable rule.

### Recommended proposal

- The condition must resolve to a typed compile-time `bool` for a concrete generic instance.
- The selected branch is fully typechecked and lowered for that instance.
- The inactive branch is parsed and name-resolved structurally, but expressions that depend on the known-false condition are not required to typecheck for that instance.
- Syntax errors are never ignored.
- Non-generic `const if` is evaluated during module checking; generic `const if` is evaluated during instance checking.

Example:

```ng
fun describe<T>(x: T) -> string {
    const if (is_integer<T>()) {
        return integer_to_string(x);
    } else {
        return unsupported_operation(x); // allowed only if this branch is inactive for T
    }
}
```

### Const policy proposal

- `const` execution is deterministic and capability restricted.
- ordinary native functions are forbidden;
- specially registered `const extern` / `const native` descriptors may be called only if pure/deterministic and within resource limits;
- no IO, time, random values, module mutation, task creation, raw pointer dereference, or uncontrolled allocation.

### Accepted rules

- `const if` remains the selected spelling.
- The selected branch is fully checked/lowered per concrete instance; inactive branches are syntactically valid and structurally resolved but need not typecheck for that instance.
- Const evaluation remains deterministic and capability restricted as above.

---

## D-004 — Native, C ABI, opaque type, and unsafe surface syntax

**Status:** Accepted — 2026-07-13  
**Blocks:** R2 syntax declaration design, R9

### Problem

The existing `fun f(...) = native;` declaration does not express whether a function is:

- implemented by an NG runtime intrinsic;
- an imported C symbol;
- exported to C;
- a C++ host callback;
- pure at compile time;
- safe or unsafe;
- borrowing, consuming, retaining, or returning a foreign handle.

### Recommended proposal

Separate the surfaces:

```ng
// Runtime-native implementation supplied by the embedding/stdlib.
native fun print(value: Display) -> unit effects(io);

// Imported C symbol; ABI-safe types only unless unsafe is declared.
extern "C" {
    fun strlen(value: cstr) -> usize;
}

// Opaque wrapper for a foreign pointer/resource.
extern opaque type FileHandle: pointer;

extern "C" fun fopen(path: cstr, mode: cstr)
    -> owning FileHandle?;
extern "C" fun fclose(file: owning FileHandle) -> c_int;

// Deliberately shared record layout.
repr(C)
struct Timespec {
    tv_sec: i64,
    tv_nsec: i64,
}

// Export an ABI-safe function through a generated wrapper.
export extern "C" fun checksum(data: CSlice<u8>) -> u32 { ... }

unsafe extern "C" {
    fun ioctl(fd: c_int, request: usize, ...) -> c_int;
}
```

`native fun` is an NG runtime ABI declaration, not a promise of direct C ABI compatibility. `extern "C"` is a C ABI declaration. Both carry HIR `CallableDescriptor`s but lower differently.

### Opaque type rule

`extern opaque type T: pointer` represents a foreign handle, normally `T*` or `void*`, with an explicit descriptor for nullability, ownership, drop/retain/release, sendability, and ABI projection.

It does **not** represent an unknown-size C struct by value. Use `repr(C) struct` for verified by-value records; use unsafe raw layout facilities for unions/bitfields/packed records later.

### Accepted rules

- NG runtime-native declarations use `native fun`; `fun ... = native` is not part of vNext syntax.
- Foreign declarations use `extern "C"`; the initial supported foreign calling convention is only `"C"`.
- Ownership annotations are `owning`, `borrowed`, and `shared`.
- Nullable foreign handles use `T?`.
- `repr(C)` is accepted only for ABI-safe fields and target-verified layouts. Declaring such a record is safe; raw layout casts, pointer arithmetic, union/bitfield access, and ABI-unsafe calls remain `unsafe`.
- C++ ABI is not directly supported. C++ libraries are bound through an `extern "C"` shim.
- Raw pointer types use postfix `T *const` / `T *mut`, including in extern declarations.

---

## D-005 — Concurrency model and shared state

**Status:** Provisionally accepted, experimental/non-stable — 2026-07-13  
**Blocks:** R10

### Problem

A language cannot be called concurrency-native merely because it spawns host threads. The model must specify task lifetime, memory transfer, shared state, cancellation, errors, native handle affinity, and GC/session interaction.

### Accepted experimental MVP

Concurrency is deliberately **not a stable language/runtime contract in this first version**. The implementation may change without compatibility guarantees after experience with the new ownership/runtime model.

The MVP exposes only a minimal task surface:

```ng
let task = spawn compute(input);
let result = await task;
```

MVP rules:

- `spawn` accepts a direct callable plus moved/copyable arguments; arbitrary captured `ref` values are forbidden.
- The task receives an isolated child `RuntimeSession`/execution context and cannot share mutable module globals with its parent.
- Arguments and result must satisfy a preliminary `Send` capability check; native handles must explicitly opt in.
- `await` returns the task result or its typed `Result` failure.
- There is no stable detach API, actor API, channel API, shared heap API, cancellation API, async state machine, or guarantee that the MVP task representation survives unchanged.
- The implementation is labeled experimental in compiler diagnostics, standard-library docs, and artifact metadata.

### Deferred redesign topics

- final `Send`/`Sync` trait/capability design;
- cancellation, cleanup, scheduler fairness, and blocking foreign-call policy;
- actor/channel versus shared-memory primitives;
- shared heap/module-instance policy;
- async/await suspension and scoped refs across suspension;
- stable public task ABI.

---

## D-006 — Functions, closures, and effects

**Status:** Accepted — 2026-07-13  
**Blocks:** closure syntax in R2/R4; broader R5/R10 work

### Recommended proposal

- First-class closures capture by inferred mode: copy, move, shared scoped borrow, or mutable scoped borrow where permitted.
- Closure type is an anonymous nominal environment plus call trait/interface in HIR.
- Borrow-capturing closures are non-escaping; escaping closures must own/copy/shared-own their captures. The compiler tracks hidden capture scope/origin facts and never exposes lifetime parameters.
- Closure call traits distinguish reusable immutable, mutable, and consuming closures (`Fn`/`FnMut`/`FnOnce`-like semantics, final spelling TBD).
- Captures are resolved to `Place`/ownership facts before FlowIR lowering.
- Function effects are inferred/declared as a small capability set; public/native/const functions may require explicit effect annotations.

### Accepted rules

- Closure syntax is `|params| expression` or `|params| { statements }`.
- An explicit capture list is available: `[move data, ref config, ref mut state] |params| { ... }`.
- Recursion requires an explicit named binding; anonymous closures do not implicitly self-bind.
- Effects are inferred internally. Public, `native`, `const`, and `extern` functions may declare readable capability effects using `effects(io, foreign, ...)`.
- General algebraic effects are not an initial language feature. If later introduced, handlers are initially one-shot/non-resumable and cannot cross active mutable refs.

---

## D-007 — Error model

**Status:** Accepted — 2026-07-13  
**Blocks:** public native ABI error policy, concurrency task outcome, stdlib redesign

### Recommended proposal

- Recoverable domain errors use `Result<T, E>`/tagged unions at public APIs, native boundaries, task boundaries, and C ABI wrappers.
- `?` is desugared in HIR after a stable `Result` trait/interface is defined.
- Runtime safety violations, verifier failures, and internal invariants use a separate trap/panic path that never unwinds across C ABI by default.
- R5 records a hidden capability/effect summary including `may_raise<E>`, but borrowing/move/write contracts remain ownership facts rather than algebraic effects.
- A future `throws E` surface may be syntax sugar for `Result<T, E>` or a one-shot/non-resumable error effect. It must lower to an explicit ABI-safe result at public/native/C boundaries.
- General resumable algebraic effects/handlers are deferred until closure, cleanup, and task semantics are proven; they must not resume across active mutable borrows.

This supersedes any assumption that arbitrary C++ exceptions are the public language error model.

---

## D-008 — Numeric model and target sizes

**Status:** Accepted — 2026-07-13  
**Blocks:** R1.2, R4 TypeInterner, R7 ABI

### Accepted initial numeric policy

- Supported fixed-width integers are `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, and `u64`.
- Supported floating-point values are `f32` and `f64`.
- `isize` and `usize` are target-width ABI types; their layout is determined by `TargetAbiDescriptor`.
- `i128`, `u128`, `f16`, `f128`, `f256`, and any unimplemented suffix are rejected by lexer/parser/typechecker rather than silently narrowed.
- Integer literal text is preserved exactly until contextual type selection; range failure is a language diagnostic with a source span.
- Ordinary integer arithmetic is checked in both runtime and const evaluation. Wrapping and saturating behavior are explicit standard-library/intrinsic APIs.
- `f32`/`f64` use the target's documented IEEE-754 behavior; NaN comparison rules are documented and shared by const evaluation/runtime.
- Bytecode/artifact encoding is fixed-endian; target pointer width is explicit metadata, never inferred from host serialization.

---

## D-009 — Module items, declarations, and block statements

**Status:** Accepted — 2026-07-13
**Blocks:** R2 source grammar, R4 name resolution/HIR

### Accepted rules

Syntax distinguishes **module items** from **block statements**. This is a grammar and scope distinction, not a claim that all name introduction has one semantic representation.

```ebnf
SourceUnit      := ModuleDirective? ModuleItem*
ModuleItem      := ImportItem | ExportItem | Declaration
Declaration     := FunctionDecl | StructDecl | EnumDecl | NewtypeDecl
                 | TypeAliasDecl | OpaqueTypeDecl | TraitDecl | ImplDecl | ConstDecl
Block           := "{" Statement* TailExpression? "}"
Statement       := LetStatement | ExpressionStatement | ControlStatement
LetStatement    := "let" "mut"? Pattern (":" Type)? "=" Expression ";"
```

- A declaration is a module item that introduces one or more module-level `DefId`s. The initial vNext grammar has no arbitrary declaration statement inside a block.
- `let` is a `LetStatement`, not a module declaration. It creates a lexical local that later resolves to `LocalId`/`Place`, and its initializer participates in ordinary control-flow, move, borrow, and drop analysis.
- A block may contain `let` bindings, expression statements, and control statements. A final expression without `;` is the block value; all preceding expressions are statements.
- Module-level executable statements and module-level `let` bindings are not in the initial vNext grammar. Use `const` for a module item with compile-time semantics, or an explicit future initialization item once module initialization has a RuntimeSession contract.
- A named local callable is introduced through a `let` binding of a closure/function value. Local `fun` declarations are deferred; this avoids a second local-declaration scope model before closures and capture analysis are complete.
- `import`/`export` are module directives/items, not block statements. Visibility belongs to declarations/exports, never to a local `let`.

### HIR consequence

The resolver maintains separate namespace ownership:

- module declarations allocate stable `DefId`s and participate in module/interface artifacts;
- `let` bindings allocate lexical `LocalId`s and lower to FlowIR `Place`/storage operations;
- neither category is represented by mutating the immutable syntax tree.

---

## D-010 — `loop`, `next`, and tail recursion

**Status:** Accepted — 2026-07-13
**Blocks:** R2 control-flow grammar, R4 resolution/type checking, R5 FlowIR, R7 VM lowering

### Accepted rules

`loop` and `next` are dedicated control-flow constructs. They never parse as ordinary function calls and are never lowered through the legacy interpreter's exception/control-transfer mechanism.

```ng
fun sum(limit: i64) -> i64 {
    let mut total = 0;
    loop (index = 0) {
        total := total + index;
        if index < limit {
            next (index + 1);
        }
    }
    total
}

fun sum_tail(index: i64, total: i64) -> i64 {
    if index == 0 { return total; }
    next (index - 1, total + index);  // self tail recursion
}
```

- `loop (binding = initializer, ...) { ... }` introduces an ordered set of loop-local bindings. The initializers are evaluated once before the first iteration.
- Each `next (value, ...)` inside a loop targets the **innermost active loop**. Its values are evaluated left-to-right in the current iteration, then all loop bindings are replaced simultaneously and control jumps to the loop header.
- Falling out of a loop body completes the statement. The initial MVP has statement-valued loops; loop-as-expression and `break value` are deferred until typed HIR/FlowIR establishes their result and cleanup semantics.
- `next (...)` when no loop is active targets the enclosing function itself and is a self-tail-recursion terminator. It is legal only in a function body and must provide one value per non-implicit function parameter. Parameter defaults are not part of the vNext MVP.
- A loop shadows the enclosing function as the target of bare `next`. Tail self-recursion from inside a loop will receive an explicit spelling in a later design revision rather than silently changing the nearest-target rule.
- `next` is a terminator: expressions/statements after it in the same basic block are unreachable for FlowIR purposes. The parser may preserve them for recovery; the resolver/type checker diagnoses unreachable code separately.
- Loop binding annotations are optional. Inference uses initializer types; `next` values must match the corresponding resolved binding types. Tail-recursive `next` values must match the corresponding parameter types.
- Borrow/move checks evaluate every `next` argument before rebinding. Iteration-local loans end at the back-edge; no scoped `ref` may be carried into the next iteration unless its declared origin remains valid across that back-edge.

### HIR/FlowIR consequence

Resolved HIR records `NextTarget::Loop(LoopId)` or `NextTarget::Function(DefId)`. FlowIR lowers them to distinct terminators:

- `LoopBackedge { target: BlockId, arguments: Vec<ValueId> }`;
- `TailRecur { function: InstanceId, arguments: Vec<ValueId> }`.

The VM reuses the active frame for `TailRecur`; it does not push a normal call frame. Cleanup/drop edges execute before either terminator, and neither terminator may cross an active `ref mut`, foreign ABI boundary, or suspension point.

### Required diagnostics/tests

- `next` outside a function/loop;
- loop `next` arity/type mismatch;
- tail-recursive `next` arity/type mismatch;
- nested-loop nearest-target selection;
- simultaneous-rebinding behavior (`next (b, a)` swaps loop state);
- moved/borrowed loop state and cleanup on exit/back-edge;
- no C++ stack growth for tail recursion.

---

## D-011 — Traits as abstract types and dynamic `ref<Trait>` views

**Status:** Accepted — 2026-07-13
**Blocks:** R4 trait identity/solver, R5 ownership analysis, R6 runtime descriptors, R7 dispatch ABI, R9/R10 capability checking

### Accepted rules

A trait is both a capability contract and an **abstract type**. It is valid in
trait bounds and in type positions, but it has no by-value runtime layout and
cannot be instantiated directly:

```ng
trait Show {
    fun show(self: Self ref) -> string;
}

// Invalid: Show is abstract, so it has no by-value value or constructor.
// let value: Show = ...;
// let value = new Show {};
```

A fully instantiated trait may be the target of a safe reference:

```ng
fun render(item: ref<Show>) -> string {
    return item.show();
}

let counter = Counter { label: "seven" };
let view: ref<Show> = ref counter;
```

`ref<Trait>` (equivalently `Trait ref`) is a non-owning abstract-reference
view. Internally it contains a checked reference/typed handle to a concrete
referent and an immutable dispatch descriptor selected from the referent's
`ImplId`. It is not an erased by-value trait object, an owning box, or a
second user-visible ownership domain.

- The language introduces neither `dyn Trait` syntax nor `Box<dyn Trait>`.
- A coercion from `Concrete ref` to `Trait ref` is legal only when the trait
  solver proves the selected concrete `ImplId`; generic trait arguments must
  be fully instantiated at the coercion site.
- `ref<Trait>` preserves the existing hidden origin/loan contract of `ref`.
  It cannot outlive, own, heap-store, globally store, or task-transfer its
  referent; diagnostics must never expose lifetime names.
- `ref mut<Trait>` is an exclusive mutable abstract-reference view. A method's
  receiver capability is part of its typed ABI and controls whether it is
  callable through shared or mutable references.
- The initial dynamically callable trait subset is object-safe: no unbound
  generic methods, no by-value `Self`, no returned/stored unconstrained
  `Self`, and no method whose argument/result lacks a concrete reference-safe
  ABI. Static/associated methods remain statically dispatched and cannot be
  invoked through `ref<Trait>`.
- Supertrait dispatch descriptors are composed deterministically; ambiguity is
  a typecheck error. The selected method/impl descriptor is carried by Typed
  HIR and FlowIR, never re-resolved by method-name strings in bytecode or VM.
- The reference view itself has no independent referent lifecycle. Copy/move,
  tracing, drop, native capability, and sendability are determined by the
  concrete referent descriptor plus the reference capability.
- Vtables/dispatch descriptors are immutable artifact/type metadata resolved
  through session-owned descriptor registries; they are not mutable
  process-global maps.

### Completeness and conflict checks

- This is compatible with D-001: `trait` is a distinct abstract declaration
  kind, unlike `opaque type`, `struct`, `enum`, `newtype`, or alias.
- This is compatible with D-002: `ref<Trait>` is a scoped safe reference, not
  `Box`, `Gc`, `Arc`, an opaque handle, or a user-visible lifetime mechanism.
- This is compatible with D-004/D-005: FFI and task transfer may only accept a
  trait reference when the concrete descriptor and reference capability pass
  the declared ownership/thread constraints.
- Trait object coercion is deferred until the solver, descriptor runtime, and
  hidden loan/origin checks exist. Parser support alone is insufficient.

### Implementation staging

- Stage 1 (static, parallel with D-012/D-013/D-014): trait declarations,
  impls, static dispatch, default methods, and simple supertraits.
  **Implemented 2026-08-15:** `trait`/`impl` declarations, coherence
  (duplicate impls, unknown/extra methods, missing methods unless a default
  exists), supertrait closures (`impl Ord` provides `Eq` methods), default
  method bodies lowered as module functions, static dispatch on concrete
  receivers with implicit `ref`/`ref mut` receiver borrowing, qualified
  calls (`Trait.method(receiver)`), and `T: Trait` bounds in generic
  parameters and where clauses (impl evidence per instance). Method calls
  through bounded type parameters now monomorphize: concrete call
  substitutions clone the generic function (renumbered locals), re-check the
  body under concrete bindings, and dispatch to the selected impl method;
  bodies with deferred abstract calls are inert in the type-erased original.
  Calls with non-concrete substitutions from other generic bodies remain a
  span-carrying boundary error (`no trait bound provides method ...`).
- Stage 2 (dynamic, requires D-015 reference checking): `ref<Trait>` coercion
  and immutable dispatch descriptors.
- Stage 3: coherence/orphan policy confirmation (legacy #35's "any impl,
  conflict requires explicit `use impl`" versus a stricter rule), auto/derive
  traits. Associated types remain a non-goal per legacy #35.

---

## D-012 — Const declarations, const predicates, and const pattern specialization

**Status:** Accepted — 2026-08-14
**Blocks:** R4 const arenas, R5 const evaluation, where clauses (D-014), `const fun` (D-013)

### Problem

Legacy NG has compile-time constants parameterized by types — `const
is_ref<T>: bool = false;` with pattern specializations — used as where
predicates and in `const if`. vNext has checked const evaluation, const
generics, and `const if`, but no module-level `const` declaration. This
decision fixes the declaration form, evaluation domain, and specialization
rules so `where` and `const fun` can build on them.

### Accepted rules

A `const` declaration is a module item that introduces a module-level `DefId`
whose value is a canonical `ConstValueId`. It may declare type parameters and
const parameters, consistent with function generics:

```ng
// Primary declaration; the default case.
const is_ref<T>: bool = false;

// Pattern specialization: matches only T = ref<U>.
const<T> is_ref<ref<T>>: bool = true;

// Repeated-parameter pattern: matches when T and U are the same type.
const equal<T, U>: bool = false;
const<T> equal<T, T>: bool = true;

// Compiler/runtime-provided pure predicate.
const is_trait<T>: bool = native;
const is_abstract<T>: bool = native;
const is_enum<T>: bool = native;

// Negative declaration: the pattern may match, but selecting it is an error.
const<T> forbid_ref<ref<T>>: bool = delete;
```

- Initial body forms are `= <const expression>`, `= native`, and `= delete`.
  A `const` declaration never has a statement body; that is `const fun`
  (D-013).
- `= native` registers a pure, deterministic host predicate with a declared
  signature. This is the initial `ConstNativeDescriptor` surface (R5.3);
  compiler builtins such as `is_trait`/`is_abstract`/`is_enum`/`is_ref` are
  supplied through it, not through ad-hoc typechecker hardcoding.
- `= delete` keeps the legacy `generalized_delete.md` semantics: deleted
  declarations participate in matching normally; selecting one is a
  compile-time error whose diagnostic points at the deleted declaration. A
  more specific deleted pattern beats a less specific valid one.
- Specialization priority is: (1) exact full match, (2) constructor/pattern
  match such as `ref<T>`, (3) parameter-pack match (future), (4) primary
  template. Where clauses (D-014) additionally filter candidates.
- The declared type is initially `bool` or an integer type; results intern
  through the existing `ConstInterner`, so equivalent spellings share one
  `ConstValueId`.
- Const parameters may appear as generic parameters: `const is_pow2<const
  N: i64>: bool = ...`.
- Evaluation is deterministic and capability restricted (D-003 policy):
  no IO, time, randomness, module state, runtime mutation, or arbitrary
  native calls.

### Relationship to abstract types

The legacy body-less `type ImGui;` abstract declaration maps to the D-001
`opaque type ImGui;` spelling. Traits remain abstract types per D-011. This
preserves both the abstract-type concept and the future opaque/FFI handle
capability; neither needs a new declaration kind.

### Implementation status — 2026-08-15

- Module-level `const` declarations parse, resolve, and evaluate: primary
  declarations, prefixed pattern specializations, implicit primary
  parameters, `= <expr>` bodies (bool literals, comparisons, checked
  arithmetic, `!`), `= native`, and `= delete`.
- Specialization priority follows the accepted order (exact full match,
  pattern/constructor match, primary), including repeated-parameter patterns
  such as `equal<T, T>` and structural matching through `ref<T>`, arrays,
  tuples, structs, and enums. Mutability matches strictly: a `ref<T>`
  pattern does not match `T ref mut`.
- Predicate applications `name<types>` fold inside `const if` for concrete
  type arguments; applications to abstract type parameters, unregistered
  natives, deleted specializations, and ambiguities are compile-time errors
  with spans. `const if` in generic functions with concrete arguments
  evaluates at declaration time.
- Remaining D-012/D-013 work: const parameters on const declarations,
  registered const natives, `const fun` bodies, where clauses (D-014), and
  per-instance `const if`.

---

## D-013 — `const fun` and compile-time function execution

**Status:** Accepted — 2026-08-14
**Blocks:** R5.2 const evaluator extension, R5.3 const natives, D-014 where evaluation

### Problem

Const expressions cover arithmetic but not function calls. Where predicates
(`where is_pow2(N)`) and richer `const if` conditions need compile-time
function execution. The legacy implementation evaluated `const fun` through a
restricted STUPID runner, which violates the vNext rule that const evaluation
operates on typed HIR and never touches the legacy interpreter or runtime
state.

### Accepted rules

```ng
const fun is_power_of_two(value: u64) -> bool {
    ...
}

// Expression body sugar.
const fun isPositive(n: i64) -> bool => n > 0;

// Pure host predicate with a declared signature.
const fun is_integral<T>() -> bool = native;
```

- `const fun` is compile-time capable and runtime callable. At runtime it
  behaves like an ordinary `fun`; `const` means compile-time capable, not
  compile-time only.
- Compile-time execution uses a typed-HIR interpreter built on the existing
  `ConstEvaluator`: checked arithmetic is shared, and the interpreter adds
  locals, `if`, `return`, `loop`/`next`, recursion, and calls to other
  `const fun`s and `const` predicates. It never instantiates STUPID, runtime
  `StorageCell`, module instances, or process-global state.
- Bodies are capability checked before evaluation: only `Pure` operations and
  calls to other const functions/predicates; `native` calls only through
  registered pure const natives (D-012); no IO, time, randomness, task
  creation, module mutation, raw-pointer dereference, or uncontrolled
  allocation.
- Fuel, recursion depth, and aggregate-size budgets are enforced; exceeding
  them is a deterministic compile-time error with a source span and a const
  call stack.
- Generic `const fun` (const predicates over types) is the same declaration
  class as D-012's `= native` const; both may appear in where clauses.
- `const fun` calls are legal in `const if` conditions, const generic
  arguments, `const` initializers, and where clauses. Calls from ordinary
  runtime contexts are legal for any `const fun`.

### Implementation status — 2026-08-15

- `const fun` parses, typechecks, lowers, and executes at runtime like an
  ordinary function; expression bodies (`=> expr`) are supported for all
  functions.
- Compile-time execution uses a new typed-HIR `ConstInterpreter` over
  canonical `ConstValue`s: locals, checked arithmetic/comparisons, `if` and
  `const if`, `return`, `loop`/`next`, tail recursion, nested const calls,
  and const predicate applications inside bodies. It never touches the
  runtime value model, and fuel (1M steps) and recursion depth (64) are
  budgeted.
- `const if` conditions may call const functions (`const if (is_large(fact(4)))`);
  non-const targets, runtime locals in arguments, and generic const fun
  compile-time calls are span-carrying errors. Explicit generic arguments on
  calls (`identity<i64>(42)`, `make<3>()`) instantiate both type and const
  parameters.
- Remaining D-013 work: generic const fun compile-time calls, registered
  const natives (`= native`), and where clauses (D-014).

**Status:** Accepted — 2026-08-14
**Blocks:** R4 overload/specialization solver, R5 per-instance checking, D-012/D-013 consumers

### Problem

Legacy NG constrains generic declarations with `where` clauses mixing const
predicates and type constraints (`where is_numeric<T>()`, `where T is
string`, `where T: Copy && !is_ref<T>`). vNext has specialization ranking but
no constraint syntax; partial specialization and traits both need it.

### Accepted rules

Spelling: a `where` clause follows the generic parameter list and function
signature (or the declaration head):

```ng
fun describe<T>(value: T) -> string where is_numeric<T>() { ... }

// Direct type constraint (legacy spelling, retained).
fun describe<T>(value: T) -> string where T is string { ... }

// Boolean combination.
fun<T> convert(value: ref<T>) -> T where !is_abstract<T>() && !is_ref<T>() { ... }

// Trait bound; forward-compatible with D-011. Trait solving is staged.
trait Replicate<T> where T: Copy { ... }
impl<T> Show for Wrapper<T> where T: Show { ... }
```

- Constraint forms: const predicate application `name<TypeArgs>(ConstArgs?)`
  (D-012/D-013, must resolve to `bool`); direct type pattern `T is Type`;
  trait bound `T: Trait` / `T: Trait1 + Trait2`; `&&`, `!`, and parentheses
  combine them.
- Non-generic where clauses are checked during module checking. Generic
  where clauses are checked per concrete instance, like generic `const if`
  (D-003).
- Overload/specialization selection: candidates are filtered by the existing
  specificity ranking and by where satisfaction. A candidate whose where
  clause evaluates to `false` is discarded as non-matching; a `= delete`
  candidate whose where clause holds is a selected-forbidden error; all
  remaining ambiguity stays a compile-time error.
- `T is Type` is structural/nominal equality on the substituted type, not a
  subtyping or coercion test.
- Where clauses never change a declaration's `DefId` or generic signature;
  they only constrain instance validity. Satisfied constraints do not alter
  type identity.

### Implementation status — 2026-08-15

- Function declarations accept a `where` clause after the signature with
  predicate applications (`is_box<T>`), direct type patterns (`T is i64`),
  negation (`!is_box<T>`), `&&`/`||` combinations, and const fun calls over
  const parameters (`is_large(N)`).
- Generic where clauses are evaluated per concrete call instance after
  substitution, in both inference paths (`infer` and `inferExpected`), with
  span-carrying "call does not satisfy its where clause" errors. Non-generic
  where clauses are checked at module checking even for uncalled functions.
- Abstract type parameters inside a where clause (calls from other generic
  bodies) are a compile-time error; per-instance evaluation of such calls is
  deferred. Trait bounds (`T: Trait`) arrive with D-011 Stage 1; candidate
  filtering during overload selection remains a follow-up.

## D-015 — Copy-first ownership revision

**Status:** Accepted — 2026-08-14
**Revises:** D-002 (2026-07-13)

### Problem

The full D-002 model (affine defaults, exclusive `ref mut`, hidden borrow
scope/origin analysis) is the longest remaining implementation pole: it
blocks nominal values, module instances, FFI, traits, and concurrency. The
legacy engine validated a simpler model (#33 value semantics, #38/#41
partial moves) across the example corpus. This revision adopts a Copy-first
model: most programs need no borrow reasoning at all, while resource-bearing
nominal types stay affine.

### Hard constraint (unchanged)

Lifetime parameters, lifetime annotations, and lifetime names in diagnostics
must never appear in NG source syntax or normal user-facing type output.

### Accepted rules

1. `let` / `let mut` / `:=` syntax is unchanged (D-002 rule 1).

2. Copy by default:

```ng
let n: i64 = 1;
let m = n;              // Copy

let t = (1, "two");     // tuple of Copy is Copy
let u = t;              // deep element-wise copy

let a = [1, 2, 3];      // array of Copy
let b = a;              // deep element-wise copy; b and a do not alias
```

   - Copy types: all fixed-width integers and floats, `bool`, `unit`,
     `string` (shares immutable storage), and tuples whose elements are all
     Copy.
   - Copying a collection is a **deep element-wise copy**; two bindings never
     silently alias mutable storage through a copy. A future copy-on-write
     optimization may share storage as long as the observable semantics stay
     deep-copy.
   - Copy types have no `Drop`.

3. Affine by default: `array`/future `vector`, `struct`, `enum`, `opaque`
   handles, and resource types move on consuming use:

```ng
let a = Point { x: 1, y: 2 };
let b = a;              // move; use of a afterwards is a compile-time error
let c = clone a2;       // explicit clone for affine values
```

   - Use-after-move is a compile-time error tracked over place paths by the
     checker, not a runtime sentinel. Runtime moved flags may remain only as
     debug assertions.
   - `move expr` is an optional explicit spelling of the same consuming
     semantics (grammar sugar; the checker treats it as the move).

4. Partial moves (legacy #38/#41 model): moving a field marks that field
   moved; other fields stay readable; whole-object use is rejected until the
   moved field is restored by assignment; `Drop` is field-aware and runs only
   initialized fields. Diagnostics name fields, never lifetimes.

5. References enter the type system (they currently parse but are rejected
   by type resolution):

```ng
fun sum(values: array<i64> ref) -> i64 { ... }
let read_only = ref value;
let writable = ref mut value;
```

   - `T ref` / `T ref mut` keep the D-002 postfix spelling; `ref<T>` is
     equivalent prefix sugar.
   - Initial semantics are scoped views: refs may be passed down calls but
     not returned, stored in aggregates/globals/task payloads, or otherwise
     escaped. Returning refs and the D-002 origin contracts (`T ref(a)`,
     `T ref(a | b)`) are deferred until the full loan analysis lands.
   - Conflict checks are simple in the first slice: an active `ref mut` may
     not coexist with another active ref to the same place; use-after-move
     through a ref is an error. Full non-lexical borrow analysis is a later
     incremental step, not a precondition for this revision.
   - No user-visible lifetime appears anywhere.

6. `Drop` runs exactly once for initialized affine values on every FlowIR
   exit edge, including partial moves and early returns.

7. Heap domains are deferred: no `Gc`, `Arc`, `Box<T, A>`, arena callbacks,
   or `new` in this revision (D-002 rules 7–9 and 14 deferred). They return
   with the R6 runtime session work.

8. Raw pointers keep `T *const` / `T *mut` and the `unsafe` boundary
   (D-002 rule 10).

### Consequences for existing code

- `ref<Trait>` (D-011) now has a typecheckable reference foundation; the
  checker no longer rejects `T ref` annotations.
- Const predicates such as `is_ref<T>` (D-012) can pattern-match `ref<T>`
  types once references resolve.
- The runtime `Value` representation may share structural storage internally;
  the deep-copy rule is a language semantic enforced where copies occur.

### Implementation status — 2026-08-15

- Scoped `ref` / `ref mut` / `*` places run end to end through FlowIR
  (`MakeRef` / `LoadRef` / `AssignPlace`), bytecode, and the VM. Frame locals
  are canonical shared cells; reference values capture the root cell plus
  place steps, so writes through references stay visible after binding
  rebinds.
- Deep copy on bind/call/return is enforced in the VM (`Value::deepCopy`):
  bindings, call arguments, and returns never alias aggregate storage, while
  reference values stay views. Line and block comments are lexed and skipped.
- Remaining D-015 work: `move`/`clone` syntax and affine checking, partial
  moves, `Drop`, loan conflict checks, and rejection of escaped references.

---

## Decision summary

| ID | Topic | Status | Next owner action |
|---|---|---|---|
| D-001 | Type declaration split | Accepted | Use `type` / `struct` / `enum` / `newtype` / `opaque` / `extern opaque` as distinct declarations. |
| D-002 | Ownership/reference default | Revised by D-015 | Affine ownership, `let` / `let mut`, `T ref`, `T ref mut`, origin contracts, and postfix raw pointers. |
| D-003 | Const-if and const capability | Accepted | Per-instance selected branch; deterministic restricted const evaluation. |
| D-004 | Native/C ABI/opaque syntax | Accepted | `native fun`, `extern "C"`, `repr(C)`, `extern opaque`, and explicit ownership annotations. |
| D-005 | Concurrency model | Provisional / experimental | Isolated-session `spawn` / `await` MVP; deliberately unstable pending redesign. |
| D-006 | Closures/effects | Accepted | `|params|` closure syntax, optional capture list, inferred capability effects. |
| D-007 | Error model | Accepted | `Result<T, E>` + `?`; panic boundary separated; algebraic effects deferred. |
| D-008 | Numeric model | Accepted | i8–i64/u8–u64, f32/f64, isize/usize, checked arithmetic. |
| D-009 | Module items vs. block statements | Accepted | Module declarations create `DefId`; block `let` creates lexical `LocalId`; no local declarations in MVP. |
| D-010 | `loop` / `next` / tail recursion | Accepted | Explicit loop binders; nearest-target `next`; self-tail recursion outside loops; dedicated HIR/FlowIR terminators. |
| D-011 | Trait abstract types and `ref<Trait>` | Accepted | Traits have no by-value instance; object-safe dynamic dispatch uses non-owning abstract reference views plus immutable selected dispatch descriptors. |
| D-012 | Const declarations and const predicates | Accepted | `const name<T>: bool = ...` with `= expr` / `= native` / `= delete`, pattern specialization, exact→pattern→pack→primary priority; abstract types remain D-001 `opaque` + D-011 traits. |
| D-013 | `const fun` | Accepted | Typed-HIR compile-time interpreter over `ConstEvaluator`; `Pure` capability; runtime-callable; `= native` const predicates; fuel/recursion budgets. |
| D-014 | `where` clauses and constrained specialization | Accepted | Const predicates + `T is Type` + `T: Trait` bounds combined with `&&`/`!`; per-instance checking; unsatisfied candidates are discarded, `= delete` stays a selected-forbidden error. |
| D-015 | Copy-first ownership revision | Accepted — revises D-002 | Scalars/strings/tuples Copy (collections deep-copy), nominal/resource types affine with `clone`/`move`, field-aware partial moves and Drop, scoped non-returnable refs with simple conflict checks; lifetimes still forbidden; Gc/Arc/Box deferred. |
