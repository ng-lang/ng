# Accepted Language Decisions

> This is the normative record of the NG language rules that **are implemented and
> enforced today**. Rejected alternatives and the debate that produced a rule live
> in git history, not in this tree.
>
> AI-assisted document: drafted with AI assistance.

## D-001 — Declaration kinds are distinct

- `type` means a **transparent alias only**.
- `struct`, `enum`, `newtype`, `opaque type`, and `extern opaque type` are distinct
  syntax nodes and type kinds.
- `struct` is the nominal-record spelling; `record` is not a keyword.

> Note: general type aliases (`type X = T;`) are **not** implemented — the only
> accepted form today is the opaque declaration `type X = native;` and the abstract
> `type X;`. See [Type System in Depth](/guide/type-system-in-depth).

## D-002 — Ownership, bindings, and references

1. Bindings are immutable by default: `let` / `let mut`. Reassignment uses `:=`.
2. `Copy` types copy by value; everything else is **affine** and moves on consuming
   assignment, call, and return.
3. Scoped references use postfix `T ref` / `T ref mut`, equivalently the prefix
   generic spelling `ref<T>`. In expressions:

   ```ng
   let read_only = ref value;
   let writable  = ref mut value;
   ```

4. Borrow scope/origin is **hidden compiler metadata**. Lifetime parameters,
   annotations, names, and user-visible lifetime diagnostics are permanently
   forbidden.
5. A function may declare the owner source of a returned reference via an origin
   set, e.g. `-> T ref(items)` or `-> T ref(a | b)`. References **cannot** be
   returned without such a declared origin set.

> Verified: `array<ref<i64>>` and `array<ref<Trait>>` **are** legal — references may
> be stored in aggregates (contrary to an earlier draft rule).

## D-003 — `const if`

- `const if` is the selected spelling.
- The selected branch is fully checked/lowered per concrete instance; inactive
  branches stay syntactically valid and structurally resolved but need not
  typecheck for that instance.
- Evaluation is deterministic and capability-restricted.

> Verified: `const if` works with **concrete** type arguments. A const predicate over
> an abstract type parameter inside a generic body is rejected
> (`cannot evaluate const declaration ... for abstract type parameter T`).

## D-004 — Native, C ABI, and opaque surface

- NG runtime-native declarations use `native fun`.
- Foreign declarations use `extern "C"`; `"C"` is the only supported convention.
- Ownership annotations are `owning`, `borrowed`, `shared`.
- Nullable foreign handles use `T?`.

> Verified (INC-29 step 3c): a `|T...| -> R` closure type in an
> `extern "C"` signature is a C function pointer. Callback values must be
> **named, non-capturing functions** whose signature matches exactly; closure
> literals and closure-valued locals are rejected (there is no userdata /
> trampoline convention yet). Each callback function gets an internal C ABI
> thunk that widens parameters to NG slots and narrows the result; the value
> passed to C is that thunk's address. Callback parameter/result types must be
> ABI-safe scalars.

> Verified (INC-29 step 3b): an `extern "C"` signature may end in `...`,
> making the call C-variadic. `...` is restricted to `extern "C"` declarations
> with at least one fixed parameter; it is rejected on `native fun`, regular
> functions, and exported C definitions. Fixed parameters follow the ABI-safe
> rules; each trailing argument keeps its own ABI-safe scalar type at the
> call site, and lowering emits the QBE variadic marker between fixed and
> trailing arguments.

> Verified (INC-29 step 3a): `export extern "C" fun name(...) -> T { body }`
> defines an NG function with a C-visible symbol. The NG body keeps its
> internal symbol and i64/f64 slots; an emitted wrapper translates the C ABI
> boundary (sub-word extension, f32 widening/narrowing) and forwards to it.
> Signatures must be ABI-safe and non-generic; aggregate parameters/results
> and sub-word integer results are deferred. No RuntimeSession initialization
> policy is needed because the language has no module globals or init
> statements (D-009).

> Verified (INC-29 step 2): `T?` is a nullable handle type, valid only when
> `T` is a native opaque handle (`type T = native;`); `i64?` and abstract
> `type A?` are type errors. It occupies the same 64-bit slot as `T`, with the
> zero bit pattern denoting null. The `null` literal adopts an expected `T?`,
> a plain `T` widens implicitly to `T?`, and `==` / `!=` against `null` are the
> supported comparisons. Nullable handles are ABI-safe in `extern "C"`.
> Unrecognized trailing tokens in a type annotation are now rejected (previously
> `T?` was silently collapsed to `T`).

> Verified (INC-29 step 1): an ownership annotation precedes the parameter type
> (`native fun f(x: owning H) -> unit;`) and is accepted only on `native fun` and
> `extern "C"` declarations; on an ordinary function it is a type error.
> `owning` requires a by-value type. At native call sites `borrowed` passes the
> value without the default deep copy, `owning` moves it, and `shared` (like an
> unannotated parameter) keeps the copy-first default; retain/release semantics
> for `shared` are descriptor-driven and are not part of this slice.
- C++ ABI is not directly supported; bind C++ through an `extern "C"` shim.
- Opaque types are declared `type X = native;` (or abstract `type X;`).

> Note: raw pointer types (`T *const` / `T *mut`) parse but have **no QBE mapping**
> and abort lowering — treat them as unusable today rather than available.

## D-006 — Functions, closures, and effects

- Closure syntax is `|params| expression` or `|params| { statements }`.
- Explicit capture lists: `[move data, ref config, ref mut state] |params| { … }`.
- **Capture semantics:** a capture value is threaded into **every** call, so:
  - `[move x]` consumes the enclosing local; later uses of that local are
    rejected (use-after-move).
  - `[x]` (copy capture) of an **affine** value requires the type to implement
    `Clone` — the capture is deep-copied per call, so the original stays live
    and remains usable after calls. Copy-capturing an affine non-`Clone` value
    is a compile error; use `[move x]`, `[ref x]`, or add `Clone`.
  - Capturing a scoped reference makes the closure an anonymous borrower of the
    referent: the loan lives until every bearer has been used for the last time.
- Recursion requires an explicit named binding; anonymous closures do not
  implicitly self-bind.
- Effects are inferred internally; `native`/`const`/`extern` declarations may
  declare readable capability effects with `effects(io, foreign, …)`.
- General algebraic effects are not a language feature.

## D-007 — Error model: `Result` and the `?` operator

Inside a function returning `Result<T, E>`, `expr?` unwraps `Ok` to its payload
and otherwise returns `Result.Err(...)` early from the enclosing function.

`?` may appear in expression position (including `f()? + g()?`). Each `?` is
desugared in evaluation order into a statement-level unwrap; `Err` returns from
the enclosing function.

Rejected at compile time: use inside a closure body, `?` on a destructuring
`let`, and any use where the enclosing function does not return `Result`.

## DEC-E — Ordering (`derive(Ord)`)

`derive(Ord)` synthesizes a **total order** for a struct. Fields compare
**lexicographically in declaration order**: the first differing field decides;
later fields matter only when all earlier ones are equal. Enums compare their
variant ordinal (declaration order) first.

- Operators: `<`, `<=`, `>`, `>=`. Equal values satisfy the inclusive operators
  and reject the strict ones.
- **Float fields are rejected** — NaN breaks totality, so they are excluded from
  v1 (`floats have no total order (NaN) and are excluded from v1`).
- Integer, `bool`, and `string` fields are orderable. A nested struct/enum field
  must itself derive `Ord`.
- String ordering is **bytewise** (`memcmp`); for UTF-8 this equals code-point
  order. A shorter string that is a prefix of a longer one is less.

## D-009 — Module items vs. block statements

A declaration is a **module item** introducing module-level `DefId`s; `let` is a
`LetStatement` creating a lexical local. Blocks contain `let` bindings, expression
statements, and control statements; a final expression without `;` is the block
value. There are no arbitrary nested declaration statements, module-level
executable statements, or module-level `let` bindings — use `const` for a module
item with compile-time semantics.

## D-010 — `loop`, `next`, tail recursion, `while`

- `loop (binding = initializer, …) { … }` introduces ordered loop-local bindings,
  initialized once.
- `next (value, …)` targets the **innermost active loop**, replacing all bindings
  simultaneously.
- `next (…)` with no active loop targets the enclosing function and is a
  self-tail-recursion terminator.
- Per DEC-B: a bare `next` is captured only by a `loop` **with bindings**; inside
  `while` a tail `next` still targets the function. `continue` (no arguments) exists
  for `while` only; `break` works in both `loop` and `while` and yields unit.

## D-011 — Traits as abstract types, `ref<Trait>` views

A trait is both a capability contract and an **abstract type**: valid in bounds and
type positions, with no by-value runtime layout and no direct instantiation.

```ng
trait Show { fun show(self: Self ref) -> string; }

fun render(item: ref<Show>) -> string { return item.show(); }
let view: ref<Show> = ref counter;
```

- `ref<Trait>` (equivalently `Trait ref`) is a non-owning abstract-reference view
  with a dispatch descriptor; it is **not** an erased by-value trait object or an
  owning box.
- There is neither `dyn Trait` syntax nor `Box<dyn Trait>`.
- Coercion `Concrete ref` → `Trait ref` is legal only when the trait solver proves
  the concrete impl; generic trait arguments must be fully instantiated.
- `ref<Trait>` obeys the hidden origin/loan contract of `ref`.
- A method whose return type mentions `Self` (for example `clone`) may be
  called on a view only when that view was bound from a known concrete value;
  the result is an independent value of that concrete type. A parameter or
  other erased `ref<Trait>` still cannot project `Self`.
- A bare trait **value** is rejected: ``trait `X` is not a value type; use `ref<X>` ``.

> Verified caveat: a supertrait bound does **not** make the supertrait's methods
> callable through a `T:` bound inside a generic body; the concrete receiver gets
> them, a bounded type parameter does not.

## D-012 — `const` declarations, predicates, pattern specialization

```ng
const is_ref<T>: bool = false;
const<T> is_ref<ref<T>>: bool = true;      // pattern specialization
const equal<T, U>: bool = false;
const<T> equal<T, T>: bool = true;         // repeated-parameter pattern
const is_trait<T>: bool = native;          // pure host predicate
const<T> forbid_ref<ref<T>>: bool = delete;
```

- Body forms are `= <const expression>`, `= native`, and `= delete`. A `const`
  declaration never has a statement body (that is `const fun`).
- `= native` registers a pure, deterministic host predicate with a declared
  signature; compiler builtins (`is_trait`, `is_abstract`, `is_enum`, `is_ref`,
  `is_tuple`, `tuple_size`, `sizeof_pack`) come through it rather than typechecker
  hardcoding.
- Tuple predicates require a `tuple<…>` wrapper: `is_tuple<tuple<i64, string>>`,
  `tuple_size<tuple<…>>`, `tuple_element<tuple<…>, 0>`, `tuple_concat<…>`.
  `is_tuple<(i64, string)>` is a parse error.

## D-013 — `const fun` (compile-time capability)

```ng
const fun is_power_of_two(value: u64) -> bool { … }
const fun isPositive(n: i64) -> bool => n > 0;
const fun is_integral<T>() -> bool = native;
```

- `const fun` is compile-time capable **and** runtime callable; `const` means
  compile-time capable, not compile-time only.
- Compile-time execution uses a typed-HIR interpreter sharing the `ConstEvaluator`
  (checked arithmetic included) with locals, `if`, `return`, `loop`/`next`,
  recursion, and calls to other `const fun`s and predicates.
- Bodies are capability-checked: only pure operations, other const functions, and
  registered pure const natives. No IO, time, randomness, module mutation, or raw
  pointer dereference. An impure call is rejected:
  ``native `readLine` is not const-capable``.
- Generic `const fun` (const predicates over types) is the same declaration class
  as D-012's `= native` const; both may appear in where clauses.

## D-015 — Copy-first ownership

- Copy types: all fixed-width integers and floats, `bool`, `unit`, `string`, and
  tuples whose elements are all `Copy`.
- Copying a collection is a **deep element-wise copy**; two bindings never silently
  alias mutable storage. A future copy-on-write optimization may share storage as
  long as observable semantics remain deep-copy.
- Copy types have no `Drop`.
- Affine by default: `array`, `struct`, `enum`, opaque handles, and resource types
  move on consuming use.
- Use-after-move is a **compile-time** error tracked over place paths by the
  checker, not a runtime sentinel.
- `move expr` is an explicit spelling of the same consuming semantics. A bare
  `move s;` statement consumes `s` and drops the discarded value immediately.
- Affine nominal types use `move` / `clone` plus an optional `impl Drop`.

> Verified: partial moves are field-aware for **structs**, not tuples. Moving out of
> a tuple index moves the whole tuple (`use of moved value`).

## DEC-D — Union switches

Structural unions (`A | B | C`) are full citizens and may be `switch` scrutinees.

- **Representation:** a tagged box `{ member index, payload }`. Coercing a member
  value into a union slot (let/assign/return, call argument, struct field, enum
  payload) boxes it under the member it adopts: exact for `bool`/`string`, first
  integer member for integers, first float member for floats.
- **Type pattern** `case (name: T)` binds the payload when the active member is `T`;
  `T` must be a member of the union, the binding is required (`case (i64)` is a
  syntax error), and duplicate member patterns are rejected.
- **Member-literal pattern** `case 42 | "go" | 2.5 | true` matches by value.
- **`otherwise`** matches everything else.
- **Order:** source order, first match wins.
- **Exhaustiveness** is checked against the static member set: without
  `otherwise`, every member needs a type pattern (literals never count).
- **Role split:** an `enum` is nominal and `case Variant(x)` destructures variant
  payloads; a union is structural and matches by member type or value. Variant
  patterns on a union are rejected.
- **Tuple pattern** `case (a, b)` (INC-33) destructures a tuple scrutinee by
  position. At least two names are required so `case (n: T)` stays a union
  type pattern. The binding count must match the tuple arity. A matching
  tuple pattern is exhaustive.

```ng
fun describe(value: bool | i64 | string) -> string {
    switch (value) {
        case (b: bool) { if (b) { return "yes"; } return "no"; }
        case (n: i64)  { return "int"; }
        case (s: string) { return s; }
    }
}
```

Known limits: negative-float literal patterns (`case -2.5`) and union element types
(`array<i64 | string>`) do **not** parse.

## DEC-A — No implicit mixed-type comparison

Implicit comparison between variables of differing signedness (i64 ↔ u64) and
between int and float is a **compile error**. Same-signedness cross-width comparison
is allowed (it is exact in the i64 slot). Literals keep contextual adoption
(`f == 1` promotes the literal to f64). Explicit conversions `toFloat` / `toSigned`
are provided. Equality and ordering across numeric widths therefore compare fine,
but mixed **arithmetic** across widths is a type error.

## D-016 — Target-width integers (`isize` / `usize`)

- `isize` and `usize` are built-in integer types whose width is the target's
  pointer width. On the supported targets (arm64, x86_64) that width is **64
  bits**, so `isize` behaves as a signed 64-bit integer and `usize` as an
  unsigned 64-bit integer. A target ABI descriptor drives the width; it is never
  inferred from host serialization.
- They follow the same arithmetic, comparison, and literal rules as the
  fixed-width tower:
  - Same-type arithmetic (`isize + isize`, `usize + usize`) with per-width
    checked overflow (a trap on wrap; for these types the 64-bit width check is
    a no-op since the slot already holds the full range).
  - Literal suffixes `isize` / `usize` (e.g. `5usize`, `-3isize`) and contextual
    adoption of an expected `isize`/`usize` (DEC-A still governs mixed types).
  - Literal magnitude is range-checked: `isize` accepts the i64 domain and
    `usize` the u64 domain (`18446744073709551616usize` is out of range).
  - Const evaluation and the `const fun` interpreter cover both types.
- DEC-A applies unchanged: mixed **arithmetic** between `isize`/`usize` and any
  other integer type is a type error, and a mixed-sign comparison (`isize` ↔
  `usize`) or mixed integer/float comparison is a compile error requiring
  `toFloat` / `toSigned`. Same-signedness cross-width comparison is exact.
- At the ABI boundary `isize`/`usize` are ABI-safe fixed-width integers (QBE
  `l`, C `intptr_t`/`size_t` class) usable in `extern "C"` signatures and as
  `repr(C)` struct fields. Because they share the `i64`/`u64` slot, library
  bindings declare `isize`/`usize` overloads alongside the fixed-width ones, and
  an unsuffixed integer literal reaching such a binding adopts the matching
  target-width parameter (canonical `i64` → `isize`, `u64` → `usize`) exactly as
  it adopts `i64`/`u64`.

## D-017 — Module interface artifacts (INC-30)

Every source module exposes an **interface artifact**: an immutable, textual
description of its public surface.

- An artifact lists the module's exported functions and every type declaration
  (struct, enum, trait, `const`, opaque) it introduces, plus its private
  functions marked `(private)`.
- The visible surface an importer may bind is exactly that artifact: importing a
  selective name (`import m (a, b)`) requires each name to be an exported
  function or a type declaration of `m`. A name that is private to the module is
  not on its interface and cannot be imported.
- Artifacts are an interface layer over the same merged compilation unit the
  loader produces; requesting them (`ngi --emit=artifact <file>`) prints the
  per-module summary and verifies the import boundary, and never changes the
  lowered program. Plain compilation and `--emit=ssa` output is unaffected.

## D-018 — Compilation sessions (INC-31)

A **compilation session** owns the compilation-side state of one compile run:
the source-unit loader and the build-once module caches.

- Within a session, each distinct module is parsed and its interface artifact
  built **exactly once**, including under diamond and cyclic import graphs. The
  loader caches modules by canonical path, so a repeated module (however many
  importers reach it) contributes one parse and one artifact.
- Across sessions, no mutable state is shared: a fresh session rebuilds from
  scratch and produces an equivalent but independent result. Two sessions may
  be alive at once without interfering.
- The language has no module-level globals or init statements (D-009), so there
  is no runtime global state to split into immutable artifacts and mutable
  instances yet. The session layer is the compile-time half of that split; when
  module globals arrive, per-session instance state attaches here without
  changing callers.
