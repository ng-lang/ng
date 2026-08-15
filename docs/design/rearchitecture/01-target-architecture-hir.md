# Target Compiler Architecture and HIR

> **Status:** proposed. Primary implementation phases: R2–R5 and R7.  
> **AI-assisted document:** drafted with AI assistance.

## 1. Goals and non-goals

NG vNext targets a multi-paradigm, functional-focused, type-safe, memory-efficient, concurrency-native systems language. The compiler representation must therefore support all of the following without making every backend reimplement semantics:

- immutable values and algebraic data types;
- imperative local mutation through explicit places;
- generic functions/types, traits, associated behavior, and monomorphization;
- const generics and deterministic compile-time computation;
- ownership transfer, scoped borrowing, destruction, and capability checking without user-visible lifetimes;
- closures, callbacks, async/task lowering, and pattern matching;
- native/foreign calls with explicit ABI and ownership semantics;
- bytecode now, and optimized/native/WASM backends later.

Non-goals for the first HIR implementation:

- SSA everywhere;
- user-written lifetime parameters, lifetime annotations, or lifetime names in diagnostics;
- serialized HIR cache format;
- a stable plugin ABI;
- implicit conversion rules beyond those explicitly accepted by the type system.

The first representation must make semantics explicit and preserve spans. Optimizations may introduce SSA later at a dedicated backend boundary.

## 2. Representation pipeline

```text
SourceFile / trivia
    ↓ lexer
TokenStream (immutable, spans, EOF)
    ↓ parser
Syntax AST (tree ownership; no semantic facts)
    ↓ module graph + name resolution
Resolved HIR (DefId/ModuleId references; desugaring)
    ↓ type, trait, generic, and capability checking
Typed HIR (TypeId, resolved calls, patterns, effects, places)
    ↓ const evaluation + generic instance planning
Typed HIR / Instance graph
    ↓ ownership dataflow and control-flow lowering
FlowIR (basic blocks, places, explicit cleanup/drop edges)
    ├── const evaluator input where applicable
    ├── typed-HIR reference interpreter
    └── bytecode/native/WASM lowering
```

Dependency direction is mandatory:

```text
syntax → resolver → semantic/type system → HIR/FlowIR → runtime ABI → backends
```

No lower layer may ask an upper layer to parse source, infer type strings, or mutate syntax to communicate a result.

## 3. Syntax AST contract

The syntax tree is a lossless-enough structural representation for diagnostics, formatting, and source tools.

### Required properties

- Nodes own children with `std::unique_ptr` (or an arena with equivalent single ownership).
- Each node has `SpanId`; source text/trivia is owned by `SourceManager`, not copied per node.
- `TokenStream` has a distinct EOF token.
- Parser AST contains names as spelled by the user, not `DefId`, `TypeId`, mangled names, inferred types, or evaluated const values.
- Parser AST never has a semantic map keyed by generic instance.
- Syntax is immutable after `parse()` returns.

### Explicit syntax categories

R2 should split the current overloaded type syntax into syntax nodes selected by grammar, subject to D-001:

```cpp
namespace ng::syntax {
  struct TypeAliasItem;
  struct StructItem;
  struct EnumItem;
  struct NewtypeItem;
  struct OpaqueTypeItem;
  struct TraitItem;
  struct ImplItem;
  struct ExternBlockItem;
}
```

The parser may preserve sugar, but it must not decide semantic equivalence. For example, `T[]`, `vector<T>`, and a suffix generic form can be desugared later to one HIR type constructor.

## 4. Stable identities and arenas

All cross-referenceable entities use compact, strongly typed IDs. They are session-local in the first implementation and are never reconstructed by parsing display strings.

```cpp
struct ModuleId { InternedString canonicalPath; };
struct DefId    { uint32_t value; };
struct BodyId   { uint32_t value; };
struct HirId    { uint32_t value; };
struct TypeId   { uint32_t value; };
struct TraitId  { DefId value; };
struct ImplId   { uint32_t value; };
struct InstanceId { uint32_t value; };
struct ConstValueId { uint32_t value; };
struct SpanId   { uint32_t value; };
```

`CompilationSession` owns:

```cpp
struct CompilationSession {
  SourceManager sources;
  StringInterner strings;
  ModuleResolver resolver;
  ModuleGraph modules;
  DefArena definitions;
  HirArena hir;
  TypeInterner types;
  ConstInterner constValues;
  TraitDatabase traits;
  DiagnosticSink diagnostics;
  ArtifactCache artifacts;
};
```

### Identity rules

- A nominal type identity includes its defining `DefId`; two modules may both define `Point` without collision.
- A trait identity includes `DefId`, not its local import alias.
- A function overload is a distinct `DefId`; its local lookup name resolves to an overload set.
- A generic instance is interned by `(generic DefId, normalized type args, normalized const args, target/capability configuration)`.
- `TypeInfo::repr()` becomes `format(TypeId, FormatOptions)` and is diagnostic-only.

## 5. Type representation

`TypeId` is interned and canonical. A minimal initial algebra:

```cpp
enum class TypeKind {
  Error,
  Never,
  Unit,
  Bool,
  Int, Float,
  Tuple,
  Array, Slice,
  Function,
  Nominal,
  TraitObject,
  Reference,
  RawPointer,
  OpaqueHandle,
  TypeParameter,
  AssociatedType,
  InferVariable,
};

struct FunctionType {
  Vec<TypeId> parameters;
  TypeId result;
  FunctionEffects effects;
  CallingConvention callingConvention;
  ReceiverMode receiver;
};
```

Important distinctions that must not collapse into a name string:

| Concept | Required semantic representation |
|---|---|
| `type Alias = T` | Alias definition; normalized to `T` when transparent |
| `newtype UserId wraps i64` | Distinct nominal `DefId`; explicit representation conversion only |
| `opaque type File` | Distinct opaque-handle `DefId`; no field/layout access in safe NG |
| `struct Point` | Nominal aggregate definition and ordered field descriptor |
| `enum Result<T,E>` | Nominal tagged union definition and variant descriptors |
| `T ref` / `T ref mut` | Non-owning compiler-checked read-only/exclusive view; scope/origin is hidden metadata, while a function return may declare `T ref(source)` or `T ref(a | b)` |
| `*T` | Unsafe raw pointer type with address-space/nullable metadata later |
| `fn(...) -> ...` | Function type plus effect/calling-convention metadata |

`Error` is a recoverable error type used to continue diagnostics. It must not silently behave as `Untyped` at runtime. Dynamic/opaque test-harness imports, if retained, are explicit capability-bearing definitions rather than an accidental universal type.

## 6. Resolved HIR

Resolved HIR removes syntax sugar and resolves names, but may still contain inference variables.

```cpp
struct HirModule {
  ModuleId module;
  Vec<DefId> items;
  Map<DefId, HirBody> bodies;
  ExportTable exports;
  ImportTable imports;
};

struct HirBody {
  BodyId id;
  DefId owner;
  Vec<HirLocal> locals;
  HirExprId root;
  SpanId span;
};
```

### Core expressions

```cpp
enum class HirExprKind {
  Literal,
  Local,
  Def,
  Block,
  Let,
  Assign,
  Call,
  MethodCall,
  TraitCall,
  Closure,
  If,
  Match,
  Loop,
  Break,
  Continue,
  Return,
  Tuple,
  Array,
  StructInit,
  EnumInit,
  Field,
  Index,
  Borrow,
  Dereference,
  Move,
  Cast,
  Binary,
  Unary,
  ConstBlock,
  Spawn,
  Await,
  UnsafeBlock,
};
```

The final list may grow, but each variant must have a lowering owner. Parser-only forms such as pipeline syntax, postfix folds, suffix generic syntax, and shorthand fields should be normalized before typed HIR where feasible.

### Calls are already resolved

A typed call must not store only a spelling:

```cpp
struct ResolvedCall {
  CalleeId callee;              // Def, trait slot, closure value, extern symbol, intrinsic
  Vec<TypeId> typeArguments;
  Vec<ConstValueId> constArguments;
  InstanceId instance;          // if monomorphized
  CallingConvention convention;
  CallEffects effects;
};
```

The bytecode compiler therefore never performs overload search, parses generic type strings, or guesses trait dispatch.

## 7. Typed HIR

Typed HIR associates every expression and pattern with checked facts:

```cpp
struct TypedExpr {
  HirExprKind kind;
  TypeId type;
  ValueCategory category;
  EffectSet effects;
  SpanId span;
  HirExprPayload payload;
};

enum class ValueCategory {
  Value,       // ordinary rvalue
  Place,       // assignable location
  BorrowedPlace,
  TypeValue,
  Diverges,
};
```

### Places and projections

Ownership and assignment must operate on a structural place, not a string such as `"a.b.0"`.

```cpp
struct Place {
  PlaceBase base; // Local, Static, Deref, Captured, Temporary
  Vec<Projection> projections;
};

struct Projection {
  enum Kind { Field, TupleField, ConstantIndex, DynamicIndex, Downcast } kind;
  DefId fieldOrVariant;
  std::optional<HirExprId> dynamicIndex;
};
```

Examples:

```text
person.address.city → Place(Local(person), [Field(address), Field(city)])
*ptr                 → Place(Deref(Local(ptr)), [])
tuple.0              → Place(Local(tuple), [TupleField(0)])
array[i]             → Place(Local(array), [DynamicIndex(Local(i))])
```

This is the basis for assignment checking, partial moves, borrow conflicts, native out-parameters, and later optimizer alias analysis.

### Patterns

Pattern matching should also be structural:

```cpp
enum class HirPatternKind {
  Wildcard, Binding, Literal, Tuple, Struct, EnumVariant, Or, Slice
};
```

A match checker receives a typed scrutinee and produces:

- binding definitions;
- exhaustiveness result;
- unreachable-pattern diagnostics;
- move/borrow behavior for each binding;
- a dispatch plan for FlowIR.

## 8. Generic instances and traits

Generic checking is split into declaration checking and instance selection.

1. Check a generic declaration once with `TypeParameter`/const parameter placeholders.
2. Gather constraints into a normalized `ConstraintSet`.
3. At each call/type use, infer or validate arguments.
4. Solve trait/const constraints.
5. Intern an `InstanceId`.
6. Record the selected overload, impl, associated method, and normalized substitutions in HIR.
7. Monomorphize only the reachable instance graph for a concrete backend target.

Trait solving returns evidence, not strings:

```cpp
struct TraitEvidence {
  TraitId trait;
  TypeId subject;
  ImplId implementation;
  Vec<TraitEvidence> whereEvidence;
  DispatchKind dispatch; // static, vtable/trait object, extern adapter
};
```

Coherence rules, orphan policy, specialization, negative/auto traits, and associated items need explicit RFC decisions before broad expansion. The initial solver should favor deterministic errors over implicit fallback.

## 9. Compile-time evaluation

Compile-time evaluation runs **after** expression/type checking and operates on typed HIR only.

```cpp
struct ConstEvalContext {
  CompilationSession& session;
  Map<DefId, ConstValueId> constGlobals;
  ResourceBudget budget;
  ConstCapabilitySet capabilities;
};

enum class ConstValueKind {
  Unit, Bool, Integer, Float, String, Tuple, Array,
  Enum, Type, FunctionInstance, PointerToken,
};
```

### Rules

- A const evaluator never receives `StorageCell`, `RuntimeEnv`, `ModuleInstance`, or arbitrary `shared_ptr<void>`.
- A `const fun` body is type checked and capability checked before evaluation.
- Runtime IO, mutable module globals, thread creation, arbitrary native calls, arbitrary allocation, and unsafe pointer dereference are rejected.
- Native compile-time functions use a separate `ConstNativeDescriptor` and must declare deterministic/pure capability.
- Integer operations use the language's chosen overflow policy; evaluation errors have spans and a const call stack.
- Fuel, recursion depth, aggregate size, and total allocation budget prevent compiler denial-of-service.

`const if` is resolved per normalized `InstanceId`. The result is stored in typed HIR/instance data, not syntax AST. The selected branch is the only branch lowered for that instance; the inactive branch is parsed and name-checked enough for diagnostics according to the decision in D-003.

## 10. Ownership and effect analysis

R5 introduces a deliberately scoped first ownership model:

- `Copy` types may be copied by value.
- Non-`Copy` values are affine: passing/assigning consumes unless borrowed or explicitly cloned.
- `Drop` runs exactly once for initialized owned values on every FlowIR exit edge.
- `ref value` / `ref mut value` create typed loans over a `Place`; canonical type syntax is `T ref` / `T ref mut` and prefix `ref<T>` is equivalent to `T ref`.
- Borrow scope/origin is inferred and stored as hidden compiler metadata. It is never a source-language generic parameter, annotation, printed type component, or diagnostic name.
- A function return may explicitly name a finite source contract: `T ref(a)`, `T ref(self)`, or `T ref(a | b)`. Names resolve to parameter/receiver indexes in HIR. A multi-source result is conservatively valid only while every listed source is valid.
- The initial safe model permits scoped, non-escaping refs. Borrowed values cannot be stored in globals, heap objects, task payloads, opaque handles, or escaping closures. Scoped arena APIs use callback-scoped allocation and reject escape.
- `unsafe` is required for raw-pointer dereference, FFI alias claims, and unchecked operations. Canonical raw pointer syntax is `T *const` / `T *mut`.

This is a semantic redesign, not a runtime moved-sentinel redesign. Runtime moved checks may remain as debug assertions during transition but are not the source of truth.

### Effects

Every function has an effect/capability summary. The initial set should be intentionally small:

```cpp
enum class Effect {
  Pure,
  Allocates,
  ReadsGlobal,
  WritesGlobal,
  Foreign,
  Unsafe,
  MayBlock,
  Spawns,
};
```

`const` permits a strict subset. `extern` and native calls declare effects. Concurrency and FFI later use effects to reject invalid transfer or compile-time execution.

## 11. FlowIR: the control-flow and cleanup boundary

Typed HIR is convenient for language constructs. FlowIR is a non-SSA, place-oriented CFG designed for ownership analysis and portable lowering.

```cpp
struct FlowBody {
  InstanceId instance;
  Vec<FlowLocal> locals;
  BasicBlockId entry;
  Vec<BasicBlock> blocks;
  SpanTable spans;
};

struct BasicBlock {
  Vec<FlowStatement> statements;
  FlowTerminator terminator;
};
```

Representative statements:

```cpp
enum class FlowStatementKind {
  StorageLive, StorageDead,
  Assign, MoveOut, CopyOut,
  Borrow, EndBorrow,
  Call, Drop,
  Assert, Nop,
};

enum class FlowTerminatorKind {
  Goto, SwitchInt, SwitchEnum,
  Return, Unreachable,
  CallThen, DropThen,
};
```

Why non-SSA first:

- places, projections, partial moves, drop flags, and borrows are naturally represented;
- bytecode VM has locals/slots rather than SSA registers;
- cleanup edge insertion can be verified directly;
- later optimization may lower selected FlowIR bodies into SSA without changing language semantics.

### Required FlowIR invariants

1. Every local has an explicit live/dead range.
2. Every control-flow edge has a well-defined initialized/moved state.
3. Every owned initialized path reaching scope exit gets exactly one `Drop` or an explicit move transfer.
4. A call carries a selected callee/ABI/effect descriptor.
5. A branch/switch uses resolved discriminant layout/variant information.
6. A terminator target is a `BasicBlockId`, never a byte offset.
7. Every FlowIR statement has a source span or synthetic-span provenance.

## 12. Backend contracts

### Typed-HIR reference evaluator

The reference evaluator executes typed HIR or FlowIR with `RuntimeSession` services. It exists to validate language semantics, not to provide a second parser-AST semantics implementation.

### Bytecode lowering

Bytecode lowering consumes FlowIR and only performs:

- local/temporary slot allocation;
- block-to-instruction layout;
- constant pool interning;
- bytecode ABI lowering;
- source-map emission;
- verified cleanup and call lowering.

It cannot:

- resolve names;
- infer types;
- choose overloads;
- inspect `repr()` to infer generic arguments;
- execute const conditions;
- synthesize trait implementations.

### Future native/WASM lowering

FlowIR and ABI descriptors provide the common input. Native lowering may add target-specific SSA/optimization, but must preserve FlowIR ownership, hidden borrow-scope constraints, and ABI contracts.

## 13. Acceptance criteria for R4/R5

R4 is complete only when:

- parser AST has no fields for resolved callee/mangling/const-if evaluation;
- same syntax tree can be checked in two sessions without state leakage;
- two same-named nominal types from different modules get distinct `TypeId`s;
- source and artifact interfaces use structured IDs/descriptors, not `repr()` parsing;
- compiler receives an explicit typed module/instance graph.

R5 is complete only when:

- const function evaluation does not instantiate STUPID or use runtime `StorageCell`;
- all const operations have a capability/resource policy and diagnostic spans;
- ownership errors are derived from `Place`/CFG facts rather than encoded binding strings;
- FlowIR cleanup tests prove exactly-once drop across return, branch, loop, panic/error edge, and partial move cases;
- a small typed-HIR evaluator and bytecode lowering consume the same resolved call/trait/instance facts.
