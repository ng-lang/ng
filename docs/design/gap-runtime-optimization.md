# Runtime Optimization: Native Compilation, WASM, And Embedding

## Order

Recommended implementation order: **13**.

## Goal

Extend NG beyond the bytecode VM to support native compilation, WASM deployment, and embedding in other applications.

## Motivation

Currently, NG code **only runs inside the interpreter or ORGASM VM**. This means:
- Every NG program requires the VM runtime (~37 MB binary)
- Performance is limited by bytecode interpretation
- No standalone executables
- No WASM deployment for web
- No C API for embedding NG in other applications

## Proposed Enhancements

### 1. AOT Native Compilation (LLVM Backend)

```bash
ng build --emit-native    # Compile to native code via LLVM
./my_app                   # Standalone executable
```

A new backend that compiles NG bytecode (or typed AST) to LLVM IR:

```
NG Bytecode → LLVM IR → Native Code (x86-64, ARM64)
```

Key considerations:
- GC integration: LLVM must emit GC root tracking for the existing managed heap
- Native function calls: direct call instead of VM bridge
- Inline caching for dynamic dispatch (trait objects)
- Object layout must match the existing runtime's `StorageCell` model

### 2. WASM Target

```bash
ng build --target wasm
```

Compile NG bytecode to WebAssembly:
- NG's GC maps to WasmGC proposal (or use a custom GC in linear memory)
- Native functions are replaced with WASM imports
- ImGui bindings become WASM DOM/Canvas bindings
- Module system maps to ESM modules

### 3. C API for Embedding

```c
// Embed NG in any C/C++ application
#include <ng/embed.h>

int main() {
    ng_vm_t* vm = ng_vm_create();
    
    // Register custom native functions
    ng_vm_register_native(vm, "my_func", my_callback);
    
    // Load and run NG code
    ng_result_t result = ng_vm_run_file(vm, "script.ng");
    
    if (result.type == NG_ERROR) {
        printf("Error: %s\n", result.error.message);
    }
    
    // Call NG functions from C
    ng_value_t val = ng_vm_call(vm, "compute", 42);
    
    ng_vm_destroy(vm);
    return 0;
}
```

### 4. JIT Compilation

Incremental optimization: hot functions are JIT-compiled from bytecode to native:

```bash
ng run --jit    # Enable JIT
```

- Profile-guided: count function call frequency
- Hot functions (>1000 calls) are queued for JIT compilation
- Compiled code replaces the bytecode handler
- Fall back to interpreter for cold/rare paths

## Dependencies

- AOT: requires LLVM as a build dependency (or vendored).
- WASM: requires a WASM runtime or compiler toolchain (LLVM + Emscripten).
- Embedding: requires refactoring the VM into a library with a stable C API.
- JIT: requires LLVM OR a simple code generation backend.
- Unblocks: standalone CLI tools, web deployment, game/app embedding.

## Scope

**In scope:**
- C API for embedding (`ng/embed.h`)
- VM refactoring as a shared/static library (`libng`)
- AOT compilation via LLVM (MVP: simple functions, no GC)
- WASM target (MVP: basic computation, no I/O)

**Out of scope:**
- JIT compilation (post-MVP optimization)
- GPU compute / CUDA integration
- iOS / Android embedding
- Full GC support in AOT mode (requires precise stack maps — complex LLVM integration)

## Acceptance Criteria

- `ng_build --emit-native` produces a working executable
- Native executable runs without the `ngi` binary
- WASM-compiled NG code runs in a browser
- C API: `ng_vm_create()` / `ng_vm_run_file()` / `ng_vm_call()` work correctly
- Embedded VM does not leak memory when destroyed
- AOT performance is at least 2x faster than the bytecode VM
- GC works correctly in the embedded context

## Potential Challenges

- LLVM integration is a significant engineering effort (LLVM is ~10M lines, rapidly evolving).
- GC + AOT = need accurate stack maps, which LLVM supports but requires careful setup.
- WASM target: NG's GC model does not map cleanly to WasmGC (generational, tracing, with finalizers).
- Embedding API must be thread-safe if the host application uses threads.
- Standalone binary size: even minimal LLVM output may be large due to runtime support code.
- Cross-compilation (e.g., building for ARM on x86) adds complexity.