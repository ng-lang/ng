# Runtime Optimization

> **Status:** Restructured from 4-feature monolith into 3 independent sub-proposals. JIT removed (too speculative).
> Each sub-proposal is independently estimable and deliverable.

---

## Sub-Proposal A: C Embedding API (4-6 weeks)

### Goal

Expose the ORGASM VM through a stable C API, allowing NG to be embedded in any C or C++ application.

### Motivation

Currently, NG can only run standalone via `ngi`. There is no way to:
- Call NG functions from C++
- Embed NG as a scripting language
- Load and execute NG bytecode from another application

### Design

#### C API Header (`include/ng/embed.h`)

```c
#ifndef NG_EMBED_H
#define NG_EMBED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- VM Lifecycle ---
typedef struct ng_vm_t ng_vm_t;

ng_vm_t* ng_vm_create(const char* const* module_paths, size_t num_paths);
void ng_vm_destroy(ng_vm_t* vm);

// --- Running Code ---
typedef enum {
    NG_OK,
    NG_ERROR_PARSE,
    NG_ERROR_TYPE,
    NG_ERROR_RUNTIME,
    NG_ERROR_IO,
} ng_result_kind_t;

typedef struct {
    ng_result_kind_t kind;
    const char* message;     // Owned by VM, valid until next ng_ call
    uint32_t line;
    uint32_t column;
} ng_result_t;

// Run a source file (parses, type-checks, compiles, executes)
ng_result_t ng_vm_run_file(ng_vm_t* vm, const char* path);

// Run a source string
ng_result_t ng_vm_run_string(ng_vm_t* vm, const char* source, const char* name);

// --- Calling NG Functions ---
typedef struct ng_value_t ng_value_t;

// Supported NG value types
typedef enum {
    NG_TYPE_UNIT,
    NG_TYPE_BOOL,
    NG_TYPE_I32,
    NG_TYPE_I64,
    NG_TYPE_F32,
    NG_TYPE_F64,
    NG_TYPE_STRING,
    NG_TYPE_ARRAY,
    NG_TYPE_TUPLE,
    NG_TYPE_OBJECT,
} ng_value_type_t;

// Create values from C
ng_value_t* ng_value_unit(void);
ng_value_t* ng_value_bool(bool v);
ng_value_t* ng_value_i32(int32_t v);
ng_value_t* ng_value_i64(int64_t v);
ng_value_t* ng_value_f32(float v);
ng_value_t* ng_value_f64(double v);
ng_value_t* ng_value_string(const char* v);
ng_value_t* ng_value_array(const ng_value_t* const* elements, size_t count);

// Read values in C
ng_value_type_t ng_value_get_type(const ng_value_t* v);
bool ng_value_as_bool(const ng_value_t* v);
int32_t ng_value_as_i32(const ng_value_t* v);
int64_t ng_value_as_i64(const ng_value_t* v);
float ng_value_as_f32(const ng_value_t* v);
double ng_value_as_f64(const ng_value_t* v);
const char* ng_value_as_string(const ng_value_t* v);
size_t ng_value_array_len(const ng_value_t* v);
const ng_value_t* ng_value_array_get(const ng_value_t* v, size_t index);

// Destroy a value created by C (values returned from NG functions are owned by VM)
void ng_value_destroy(ng_value_t* v);

// Call a top-level NG function by name
ng_result_t ng_vm_call(ng_vm_t* vm, const char* function_name,
                       const ng_value_t* const* args, size_t num_args,
                       ng_value_t** out_result);

// --- Registering Native Functions ---
typedef ng_value_t* (*ng_native_fn_t)(ng_vm_t* vm,
                                       const ng_value_t* const* args,
                                       size_t num_args,
                                       void* user_data);

void ng_vm_register_native(ng_vm_t* vm, const char* name,
                            ng_native_fn_t fn, void* user_data);

// --- Memory Management ---
void ng_vm_collect_garbage(ng_vm_t* vm);

// --- Error Reporting ---
const char* ng_result_message(const ng_result_t* result);

#ifdef __cplusplus
}
#endif

#endif // NG_EMBED_H
```

#### VM Refactoring

The current `ngi` executable is refactored into a library (`libng`) with a thin CLI wrapper:

```
src/
├── main.cpp               → Now: thin CLI wrapper (calls ng_vm_run_file)
└── libng/
    ├── api.cpp            → C API implementation
    ├── VM.cpp             → (moved from orgasm/)
    ├── Compiler.cpp       → (moved from orgasm/)
    └── module.cpp         → (moved from orgasm/)
```

#### Build System Changes

```cmake
# In CMakeLists.txt
add_library(ng SHARED ${NG_LIB_SRC})     # Shared library for embedding
add_library(ng_static STATIC ${NG_LIB_SRC}) # Static library
add_executable(ngi src/main.cpp)          # CLI tool (links libng)
target_link_libraries(ngi PRIVATE ng)
```

### Sub-Proposal A Acceptance Criteria

- A C program can call `ng_vm_create()`, `ng_vm_run_string()`, `ng_vm_destroy()`
- A C program can call `ng_vm_call()` to invoke a top-level NG function
- Values can be created in C, passed to NG, and read back
- Native functions registered from C can be called from NG code
- The embedding library does not leak memory (verified with valgrind/ASan)
- The existing `ngi` CLI continues to work unchanged

### Sub-Proposal A Effort Estimate

| Component | Effort |
|---|---|
| Refactor VM into library (move files, CMake changes) | 1 week |
| C API header and implementation | 2 weeks |
| Value marshaling (C ↔ NG) | 1 week |
| Native function registration API | 1 week |
| Tests | 1 week |
| **Total** | **6 weeks** |

---

## Sub-Proposal B: LLVM AOT Compilation — Deferred to Post-MVP

**Verdict: NOT recommended for current phase.**

Rationale:
- LLVM backend is a 3-6 month project with high complexity (GC stack maps, trait dispatch, cross-platform)
- The bytecode VM is sufficient for all current use cases
- WASM via Emscripten provides browser deployment with far less effort

The proposal text below is preserved as a reference design for future contributors.

### Architecture (Reference Design)

```
NG Source → Type Checker → ORGASM Bytecode → LLVM IR → Native Code
```

### Key Challenges
[Original text preserved below for reference]

```
NG Source → Type Checker → ORGASM Bytecode → LLVM IR → Native Code (.exe, .so)
                                                       ↑
                                               LLVM Backend (new)
```

The LLVM backend is a new component in `src/orgasm/`:

```
src/orgasm/llvm/
├── LLVMBackend.cpp      # Entry point: BytecodeModule → LLVM IR
├── LLVMBackend.h
├── FunctionCompiler.cpp # Per-function IR generation
├── RuntimeSupport.cpp   # GC integration, memory allocation
└── Target.cpp           # Object file emission
```

### Key Challenges

#### 1. GC Integration

The LLVM backend must emit **GC stack maps** (via `llvm.gc.statepoint` / `llvm.gc.relocate`) so the tracing GC can find roots in native frames.

```cpp
// Emit GC root tracking in LLVM IR
auto statepoint = builder.CreateGCStatepoint(
    call,                   // The call that may trigger GC
    {&stack, &localVars},   // Root pointers
    None,                   // No deopt args
    None,                   // No transition args
    None                    // No attach
);
```

#### 2. Object Layout

AOT-compiled code must use the same `StorageCell` layout as the VM. Existing headers (`buffer_runtime.hpp`, `value_access.hpp`) define this layout and are reused.

#### 3. Dynamic Dispatch

Trait object dispatch (`ref dyn Trait`) requires a vtable or equivalent mechanism. The LLVM backend generates vtables for each trait/impl combination.

### Sub-Proposal B Acceptance Criteria (MVP)

- A simple function (`fun add(a: i32, b: i32) -> i32 => a + b`) compiles and runs natively
- The standalone executable runs without loading the VM interpreter
- String operations work natively
- Primitive control flow (if/loop/switch) works natively
- MVP does NOT need: GC, trait objects, tagged unions, closures

### Sub-Proposal B Future Phases

| Phase | Scope | Effort |
|---|---|---|
| B1: Primitives + arithmetic + control flow | No GC, no heap | 2 months |
| B2: GC support (stack maps) | Heap allocation, GC roots | 2 months |
| B3: Full type coverage | Tagged unions, arrays, tuples, strings | 1 month |
| B4: Dynamic dispatch | Trait objects, vtables | 1 month |

---

## Sub-Proposal C: WASM Target (3-4 months, deferred)

### Goal

Compile NG bytecode to WebAssembly, enabling NG to run in browsers.

### Motivation

- Browser deployment for the online playground
- Shared logic between server and client
- WASM ecosystem compatibility

### Architecture

Two approaches:

#### Approach A: NG VM → WASM

Compile the C++ VM itself to WASM (via Emscripten), then load NG bytecode into it:

```
NG Source → .ngo bytecode → WASM (with embedded VM) → Browser
```

**Pros:** Full NG feature support. Works today (Emscripten can compile C++ to WASM).
**Cons:** Large binary (~10-20 MB). Slow startup.

#### Approach B: NG → WASM Direct Compilation

A new backend that compiles NG bytecode directly to WASM:

```
NG Source → Type Checker → ORGASM Bytecode → WASM → Browser
```

**Pros:** Small binary. Fast startup. Native WASM toolchain integration.
**Cons:** Complex implementation. GC/memory model mismatch.

#### Recommendation

Start with **Approach A** (VM in WASM) for the online playground MVP, then pursue Approach B if performance is insufficient.

### Sub-Proposal C Acceptance Criteria

- `ng build --target wasm` produces a `.wasm` file
- The WASM module runs in a browser and prints output
- Basic I/O works (readFile, print → mapped to JS console/FS)
- A simple interactive NG program runs in the browser

---

## Summary: Implementation Order

| Order | Sub-Proposal | Effort | Dependencies | Impact |
|---|---|---|---|---|
| 1 | **C Embedding API** | 6 weeks | VM refactoring | 🔴 High |
| 2 | LLVM AOT (Phase B1) | 2 months | Embedding API, LLVM dependency | 🟡 Medium |
| 3 | LLVM AOT (Phase B2-B4) | 4 months | GC integration | 🟡 Medium |
| 4 | WASM target | 3-4 months | Embedding API or LLVM backend | 🔵 Low |