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

## Decision summary

| ID | Topic | Status | Next owner action |
|---|---|---|---|
| D-001 | Type declaration split | Accepted | Use `type` / `struct` / `enum` / `newtype` / `opaque` / `extern opaque` as distinct declarations. |
| D-002 | Ownership/reference default | Accepted | Affine ownership, `let` / `let mut`, `T ref`, `T ref mut`, origin contracts, and postfix raw pointers. |
| D-003 | Const-if and const capability | Accepted | Per-instance selected branch; deterministic restricted const evaluation. |
| D-004 | Native/C ABI/opaque syntax | Accepted | `native fun`, `extern "C"`, `repr(C)`, `extern opaque`, and explicit ownership annotations. |
| D-005 | Concurrency model | Provisional / experimental | Isolated-session `spawn` / `await` MVP; deliberately unstable pending redesign. |
| D-006 | Closures/effects | Accepted | `|params|` closure syntax, optional capture list, inferred capability effects. |
| D-007 | Error model | Accepted | `Result<T, E>` + `?`; panic boundary separated; algebraic effects deferred. |
| D-008 | Numeric model | Accepted | i8–i64/u8–u64, f32/f64, isize/usize, checked arithmetic. |
| D-009 | Module items vs. block statements | Accepted | Module declarations create `DefId`; block `let` creates lexical `LocalId`; no local declarations in MVP. |
