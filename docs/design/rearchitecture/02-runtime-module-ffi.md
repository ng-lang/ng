# Runtime, Modules, and Native ABI Design

> **Status:** proposed. Primary implementation phases: R3, R6, R7, R9, R10.  
> **AI-assisted document:** drafted with AI assistance.

## 1. Core separation

The current implementation conflates compilation artifacts, global symbol tables, runtime modules, `StorageCell`, native state, and process-global caches. vNext separates them explicitly:

```text
CompilationSession
  owns resolver + source/artifact cache + semantic database

ModuleArtifact (immutable, shareable)
  owns interface descriptor + compiled code + imports + debug data

RuntimeSession (mutable, isolated)
  owns heap + native registry view + module instances + scheduler

ModuleInstance (mutable, one per artifact per RuntimeSession)
  owns globals + initialization state + imported instance bindings + native state
```

A source module, bytecode module, and native module differ in how their artifact is produced, not in how importers observe their interface.

## 2. Module artifacts and instances

### 2.1 Immutable `ModuleArtifact`

```cpp
struct ModuleArtifact {
  ModuleId id;
  ArtifactVersion version;
  SourceFingerprint source;
  TargetDescriptor target;

  ModuleInterface interface;
  Vec<ImportRequirement> imports;
  std::optional<BytecodePackage> bytecode;
  std::optional<NativeModuleDescriptor> native;
  DebugPackage debug;
};

struct ModuleInterface {
  Map<ExportName, DefId> exports;
  Map<DefId, PublicDefinitionDescriptor> definitions;
  Vec<PublicTraitDescriptor> traits;
  Vec<PublicImplDescriptor> impls;
};
```

Properties:

- immutable after successful compilation/loading;
- may be cached across compilation sessions only if content/target/config fingerprints match;
- contains no `StorageCell`, runtime globals, `std::function`, `shared_ptr<void>`, or execution state;
- semantic metadata uses `DefId`/serialized descriptor tables, never `TypeInfo::repr()` as a protocol;
- source, bytecode, and native artifacts expose the same import/export/trait/impl shape.

### 2.2 Mutable `ModuleInstance`

```cpp
enum class ModuleInitState {
  Uninitialized,
  Initializing,
  Initialized,
  Failed,
};

struct ModuleInstance {
  const ModuleArtifact* artifact;
  ModuleInitState state;
  Vec<Slot> globals;
  Map<ModuleId, ModuleInstance*> imports;
  NativeModuleState nativeState;
  std::optional<RuntimeError> initializationFailure;
};
```

Rules:

- a `RuntimeSession` owns exactly one instance of an artifact unless isolation policy explicitly requests another;
- initialization runs exactly once per instance;
- import cycles are represented in the graph and diagnosed at a defined phase; accidental recursive source compilation is not cycle handling;
- failed initialization is memoized and reported consistently;
- globals are not stored in `ModuleArtifact`, resolver cache, or process-global registry;
- a module's exported function may be called only according to the module initialization policy declared by the language.

### 2.3 Resolver and cache policy

`ModuleResolver` belongs to `CompilationSession`; it receives explicit search roots, lockfile/package information later, target configuration, and a source loader. It does not read mutable runtime state.

Cache keys include at least:

```text
canonical module id
content hash / source fingerprint
target descriptor
compiler semantic format version
feature/capability configuration
transitive interface fingerprints
```

Native module registration is a resolver input, not a process-global side effect. Test fixtures create a session-local registry.

## 3. Runtime session

```cpp
struct RuntimeSession {
  RuntimeConfig config;
  TypeRuntime typeRuntime;
  Heap heap;
  ModuleInstanceStore modules;
  NativeRegistry natives;
  Scheduler scheduler;
  DiagnosticSink diagnostics;
};
```

A session is the unit of:

- GC/allocator ownership;
- native resource ownership;
- module global state;
- task scheduling;
- host callbacks;
- deterministic test isolation.

No execution engine may depend on process-global module caches, global GC root providers, or global native handler maps.

## 4. Runtime value model

### 4.1 Design choice

vNext adopts a **descriptor-driven slot/object graph** as the initial authoritative representation. It is compatible with a bytecode VM and a future optimizing compiler while avoiding the current conflicting `bytes`/`opaqueRefs`/`namedRefs` representations.

- `Slot` is a typed storage location in a frame, global table, or aggregate field.
- `Value` is a typed immediate, inline aggregate, reference, or heap object handle.
- `HeapObject` owns object payload and GC metadata.
- `TypeDescriptor` defines layout, tracing, copy/move/drop, equality, formatting, member access, and ABI projection.
- A layout byte representation is an implementation detail of a type descriptor, not a second semantic object graph.

### 4.2 Proposed core types

```cpp
struct SlotId { uint32_t value; };
struct ObjectId { uint64_t value; };

struct Slot {
  TypeId type;
  SlotState state;       // Uninitialized, Initialized, Moved
  ValueStorage storage;
};

struct HeapObject {
  TypeDescriptorId descriptor;
  GcHeader gc;
  ObjectPayload payload;
};

struct Value {
  TypeId type;
  ValueRepr repr;
};
```

`ValueRepr` may initially support:

- fixed-width scalar;
- inline small aggregate;
- `ObjectId` handle;
- reference/borrow projection;
- opaque native handle token;
- function/closure handle.

The precise tagged union can evolve behind this boundary. Backends must use runtime accessors rather than inspect payload fields directly.

### 4.3 Type descriptors

```cpp
struct TypeDescriptor {
  TypeId type;
  RuntimeKind kind;
  LayoutDescriptor layout;
  TraceFn trace;
  CopyFn copy;
  MoveFn move;
  DropFn drop;
  EqualityFn equals;
  MemberDispatchTable members;
  AbiProjectionTable abi;
};
```

A descriptor is the single source of truth for:

- field order and offsets;
- enum tag/variant layout;
- pointer/reference tracing;
- copyability and move behavior;
- destruction;
- `repr(C)` eligibility and ABI lowering;
- native opaque-handle behavior.

`TypeLayout` may remain an internal component, but it cannot claim a field offset that runtime member access ignores.

## 5. Ownership, destruction, and GC

### 5.1 Static versus runtime responsibility

| Concern | Primary authority | Runtime role |
|---|---|---|
| Illegal use after move | FlowIR ownership analysis | debug assertion/safety diagnostic during transition |
| Borrow alias conflict | FlowIR ownership/hidden borrow-scope analysis | optional checked mode only |
| Copy legality | type/trait system | execute descriptor copy operation |
| Drop insertion | FlowIR cleanup edges | execute descriptor drop operation exactly once |
| Heap reachability | runtime heap | trace from session roots |
| Native resource release | descriptor/native handle state | execute declared finalizer policy |

### 5.2 Heap policy

Initial vNext heap requirements:

- non-moving tracing collector, so externally borrowed/pinned addresses are manageable;
- explicit root enumeration from VM frames, HIR evaluator frames, module globals, scheduler tasks, and registered host handles;
- type-descriptor trace function for every heap payload;
- finalizers run under documented constraints: no arbitrary re-entrant allocation assumptions, errors are reported through controlled runtime failure policy;
- no `shared_ptr` graph is the language heap ownership model;
- foreign/native resources use an explicit handle record, not `shared_ptr<void>` casts.

A moving collector, generational collector, reference counting optimization, or region allocator can be introduced later without changing the `RuntimeSession`/descriptor interface.

### 5.3 Native handles

```cpp
struct NativeHandleValue {
  NativeTypeId nativeType;
  uintptr_t address;
  HandleOwnership ownership; // Owned, Borrowed, Shared, Pinned
  CapabilitySet capabilities;
};
```

A handle descriptor declares:

- whether null is valid;
- whether the address may cross task boundaries;
- destructor/finalizer callback;
- clone/retain/release behavior;
- thread-safety capability;
- ABI representation (`pointer`, `integer token`, custom adapter).

## 6. Unified invocation model

All callable categories use a logical call descriptor:

```cpp
struct CallableDescriptor {
  DefId definition;
  FunctionSignature signature;
  ReceiverMode receiver;
  CallingConvention convention;
  EffectSet effects;
  AbiDescriptor abi;
};
```

Execution engines adapt it:

| Callable implementation | Invocation implementation |
|---|---|
| typed HIR function | HIR evaluator frame |
| bytecode function | VM frame |
| intrinsic | runtime service call |
| NG host-native function | typed host adapter |
| imported C function | C ABI adapter |
| callback/closure | closure environment or generated trampoline |

No host function should receive a raw `NGEnv`/`NGArgs` by default. A low-level embedding API may exist, but it is explicitly advanced/unsafe.

## 7. Native APIs: three distinct boundaries

The current implementation conflates them. vNext distinguishes them.

### 7.1 NG runtime-native API

This is for implementing standard library intrinsics or embedding NG in C++ without crossing the platform C ABI on every call.

```cpp
using NativeEntry = NativeResult (*)(NativeCallContext&, std::span<const ValueView>);

struct NativeFunctionDescriptor {
  CallableDescriptor callable;
  NativeEntry entry;
  NativeCapabilities capabilities;
  NativeErrorPolicy errors;
};
```

`ValueView`/`MutableValueView` are typed, validated wrappers supplied by the runtime. C++ templates can generate adapters from a declared `FunctionSignature`, but descriptor validation remains authoritative.

### 7.2 Imported C ABI

This calls a foreign symbol with a target C calling convention. It is not zero-cost in a bytecode VM because VM values must be marshaled. It may become direct in an AOT backend for ABI-safe signatures.

```ng
extern "C" {
    fun strlen(s: cstr) -> usize;
}
```

Only a restricted ABI-safe type subset is permitted directly:

| NG ABI type | C representation | Safety notes |
|---|---|---|
| `i8..i64`, `u8..u64`, `isize`, `usize` | matching fixed-width/integer type | target width verified |
| `f32`, `f64` | `float`, `double` | target IEEE policy verified |
| `unit` | `void` | return only |
| `T *const`, `T *mut` | `T*` | unsafe; pointee access is read-only/writable respectively; provenance and validity are not guaranteed |
| `extern fn(...)` | C function pointer | unsafe/callback policy applies |
| `repr(C) struct` | C struct | only if layout is verified ABI-safe |
| `cstr` | `const char*` | NUL-terminated borrowed string view with call-scoped validity |
| `CSlice<T>` | `{T*, usize}` by target ABI descriptor | explicit ownership and validity contract |
| opaque handle | usually `void*`/named pointer | representation declared by opaque type |

`string`, GC references, trait objects, arbitrary NG arrays, closures, and ordinary NG structs are **not** automatically C ABI values.

The implementation may use libffi for dynamic symbol calls in the first bytecode backend, but libffi is an implementation detail. The semantic ABI descriptor remains the source of truth. Static/direct bindings may use generated target thunks later.

### 7.3 Exported C ABI

NG may export a function only if its signature is ABI-safe and explicitly marked:

```ng
export extern "C" fun checksum(data: CSlice<u8>) -> u32 { ... }
```

The compiler generates a target-specific wrapper. It must define:

- initialization/RuntimeSession acquisition policy;
- panic/error behavior (never unwind through foreign C by default);
- allocation ownership for returned buffers;
- thread attachment requirements;
- callback and reentrancy policy.

A normal NG function is not automatically C-callable merely because it has primitive-looking parameters.

## 8. Opaque types and abstract/native wrappers

### Short answer

**Yes: an opaque type is the correct safe default wrapper for most foreign resources and C `struct T*` APIs.** It is not, by itself, enough to model every C ABI type.

An opaque wrapper can safely cover:

- `FILE*`, sockets, database handles, OpenGL/Vulkan object handles;
- OS descriptors and platform resources;
- C++ object pointers hidden behind C shim functions;
- foreign ownership protocols (`create/destroy`, retain/release);
- capability-bearing device/context objects.

It cannot safely or faithfully replace:

- a C struct passed/returned **by value**;
- a `repr(C)` shared record layout;
- a raw pointer used for pointer arithmetic;
- callback function pointers without a trampoline/registration-validity contract;
- C varargs;
- C unions/bitfields/platform-specific packed layouts.

### Proposed distinction

```ng
// A language-opaque handle; representation is hidden in safe NG.
extern opaque type FileHandle: pointer;

extern "C" fun fopen(path: cstr, mode: cstr) -> owning FileHandle?;
extern "C" fun fclose(file: owning FileHandle) -> c_int;
extern "C" fun fread(dst: u8 *mut, size: usize, count: usize,
                      file: borrowed FileHandle) -> usize;

// A record deliberately shared with C; layout is part of the public contract.
repr(C)
struct Timespec {
    tv_sec: i64,
    tv_nsec: i64,
}
```

`opaque type` is therefore not merely the existing `type X = native` spelling with a different implementation. It needs declared representation and ownership semantics. `repr(C)` records are a separate, explicit, unsafe-adjacent layout feature.

### Ownership annotations

The exact syntax is pending D-004, but semantic modes are required:

- `owning T`: call transfers/delivers one owned handle;
- `borrowed T`: call may use but not retain/destroy the handle;
- `shared T`: retain/release semantics supplied by descriptor;
- nullable handle: `T?` only if descriptor allows null;
- out parameter: explicit `out *mut T` / dedicated result wrapper, never inferred from `T**` silently.

The typechecker validates declared operations against descriptor capabilities. For example, an `owning FileHandle` cannot be copied unless its descriptor defines retain/clone semantics.

## 9. ABI and unsafe policy

### Required target descriptor

C ABI choices are target properties, not hard-coded x86 examples:

```cpp
struct TargetAbiDescriptor {
  Architecture arch;
  OperatingSystem os;
  CallingConvention defaultC;
  Endianness endianness;
  uint8_t pointerWidth;
  DataLayout layout;
};
```

`repr(C)` eligibility and ABI classification are computed from this descriptor. ARM64, Windows x64, SysV, RISC-V, and future targets cannot share hand-written assumptions.

### Unsafe boundary

The following require `unsafe` or an explicit unsafe extern declaration:

- raw pointer construction/dereference/arithmetic;
- foreign function invocation not proven ABI-safe;
- reinterpretation/layout casts;
- passing GC-managed addresses to foreign code without pinning/copying;
- callbacks that can outlive the call without an owned callback handle;
- mutable aliasing assertions;
- varargs.

Safe wrappers should be built in NG/native modules around these primitives.

## 10. Callback, string, and aggregate policies

These are commonly omitted and must be designed before R9 is complete.

### Callbacks

A callback passed to C needs a generated trampoline plus an owned registration token. The token keeps the closure/environment alive and controls deregistration. Foreign code may not retain an ordinary NG closure without this token.

### Strings

- `string` is an NG managed string and never implicitly aliases `char*`.
- `cstr` is a borrowed NUL-terminated view valid only for a documented call duration.
- owned C strings use an opaque owner/deallocator descriptor or an explicit `CBuffer` return convention.
- UTF-8/encoding expectations belong to the declaration.

### Aggregates

- only `repr(C)` records with verified fields may cross by value;
- no GC pointer field may be exposed by value without pin/copy policy;
- C unions/bitfields/packed structs require a later explicit raw-layout facility or bindgen-generated unsafe wrappers;
- variadic C functions are deferred until a precise typed varargs design exists.

## 11. Concurrency foundation

Concurrency is not just `spawn` opcodes. R10 begins only after R6 provides session-local heap/module/native state and R5 provides ownership facts.

Initial direction:

- the first `spawn` / `await` implementation is experimental and intentionally non-stable;
- it accepts direct callables plus moved/copyable arguments only; arbitrary captured `ref` values are forbidden;
- each task initially has an isolated child `RuntimeSession` and cannot share mutable module globals;
- task transfer requires a preliminary `Send`-like capability derived from type/descriptor structure;
- native handles declare thread affinity and sendability;
- task failure is returned through typed `Result` outcome;
- cancellation, detach, actor/channel APIs, shared heap policy, and stable task ABI are deferred.

The isolated-session implementation is an experiment, not the permanent concurrency contract. Artifact metadata and user-facing documentation must label it experimental until D-005 is redesigned from implementation evidence.

## 12. R6/R9 acceptance criteria

R6 is complete only when:

- no process-global heap, module runtime instance, or native handler map is required for normal execution;
- a fresh `RuntimeSession` runs a module without seeing another session's globals/native state;
- source/native/bytecode module artifacts instantiate through one module API;
- module initialization is exactly once and cycle/error behavior is tested;
- HIR evaluator and VM call shared lifecycle/member/sequence services;
- every heap object is traced through descriptor-defined edges.

R9 is complete only when:

- every native function has a declared `CallableDescriptor`, ownership contract, effect/capability set, and error policy;
- opaque handles have explicit pointer/value representation, nullability, ownership, drop, and sendability metadata;
- C ABI calls reject non-ABI-safe types before lowering;
- `repr(C)` layouts are target-verified and tested against a compiled C fixture;
- callbacks, strings, slices, returned ownership, panic behavior, and GC pin/copy policy are documented and tested;
- STUPID/HIR evaluator and VM invoke the same native descriptor adapter.
