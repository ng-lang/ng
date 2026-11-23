# Accepted Language Decisions

> This is the normative record of the NG language rules that **are implemented and
> enforced today**. It is derived from the vNext decision log (D-001 … D-015);
> forward-looking proposals, rejected alternatives, and the rationale debate live
> in `.agents/plans/`, not here.
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

Allowed positions:

- a `let` initializer: `let v = f()?;`
- a `return` value: `return f()?;`
- a bare expression statement: `f()?;`

Rejected at compile time: mid-expression use (`f()? + 1`), use inside a closure
body, `?` on a destructuring `let`, and any use where the enclosing function
does not return `Result`.

## DEC-E — Ordering (`derive(Ord)`)

`derive(Ord)` synthesizes a **total order** for a struct. Fields compare
**lexicographically in declaration order**: the first differing field decides;
later fields matter only when all earlier ones are equal. Enums compare their
variant ordinal (declaration order) first.

- Operators: `<`, `<=`, `>`, `>=`. Equal values satisfy the inclusive operators
  and reject the strict ones.
- **Float fields are rejected** — NaN breaks totality, so they are excluded from
  v1 (`floats have no total order (NaN) and are excluded from v1`).
- Integer and `bool` fields are orderable. A nested struct/enum field must itself
  derive `Ord`. A `string` field passes derivation but is not yet comparable
  (string ordering is unimplemented).
- String ordering, when implemented, is **bytewise** (`memcmp`); for UTF-8 this
  equals code-point order.

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
