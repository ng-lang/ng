# C ABI / External FFI — Calling Convention & Type Mapping

> **Status:** Refined with calling convention details and platform ABI mapping.

## Type Mapping Table

```
┌─────────────────┬────────────────────┬──────────────────────────────┐
│ C Type          │ NG Declaration     │ Notes                        │
├─────────────────┼────────────────────┼──────────────────────────────┤
│ void            │ unit               │ Function return only         │
│ int8_t          │ i8                 │                              │
│ int16_t         │ i16                │                              │
│ int32_t         │ i32                │                              │
│ int64_t         │ i64                │                              │
│ uint8_t         │ u8                 │                              │
│ uint16_t        │ u16                │                              │
│ uint32_t        │ u32                │                              │
│ uint64_t        │ u64                │                              │
│ float           │ f32                │                              │
│ double          │ f64                │                              │
│ char*           │ *u8 (raw)          │ Zero-copy, unsafe            │
│ char*           │ string             │ Copy into NG string, safe    │
│ void*           │ *u8                │ Opaque pointer               │
│ int*            │ *i32               │ Pointer to int               │
│ struct T        │ opaque type        │ Size-known opaque handle     │
│ struct T*       │ *u8                │ Pointer to struct            │
│ int32_t(*)()    │ *u8 (func ptr)     │ Raw function pointer         │
└─────────────────┴────────────────────┴──────────────────────────────┘
```

## Calling Convention Bridge

When an `extern fun` is called, the ORGASM VM must transition from its stack-based calling convention to the platform's C ABI.

### x86-64 (System V) ABI Mapping

```
NG Stack                     C ABI (x86-64 SysV)
─────────                    ───────────────────
Top of stack                 RDI (1st arg)
2nd from top                 RSI (2nd arg)
3rd                          RDX (3rd arg)
4th                          RCX (4th arg)
5th                          R8  (5th arg)
6th                          R9  (6th arg)
Remaining                    Stack (right-to-left)
Return value → result        RAX (return value)
```

### Implementation

```cpp
// src/orgasm/extern_bridge.cpp

#include <cstdint>
#include <cstring>

// Platform-specific calling convention bridge
#if defined(__x86_64__) && !defined(_WIN32)
// System V ABI

struct SysVABIArgs {
    uint64_t rdi, rsi, rdx, rcx, r8, r9;
    std::vector<uint64_t> stackArgs;
};

extern "C" uint64_t sysv_call(const void* func, SysVABIArgs args);

// Assembly trampoline (in extern_bridge_amd64.S):
//   mov rdi, args.rdi
//   mov rsi, args.rsi
//   mov rdx, args.rdx
//   mov rcx, args.rcx
//   mov r8,  args.r8
//   mov r9,  args.r9
//   ; push stack args (in reverse order)
//   call func
//   ret

#elif defined(_WIN64)
// Windows x64 ABI (different register mapping)
// RCX, RDX, R8, R9, then stack
#endif

// NG value → C value marshaling
uint64_t marshalToC(const StorageCell &cell, ExternType targetType) {
    switch (targetType) {
        case ExternType::I32: return cell.as<int32_t>();
        case ExternType::I64: return cell.as<int64_t>();
        case ExternType::F32: return bit_cast<uint32_t>(cell.as<float>());
        case ExternType::F64: return bit_cast<uint64_t>(cell.as<double>());
        case ExternType::PTR: return cell.as<uint64_t>();  // Raw pointer
    }
}
```

### Dynamic Library Loading

```cpp
// src/orgasm/dlloader.cpp

#include <dlfcn.h>   // POSIX
// #include <windows.h>  // Windows

struct DynamicLibrary {
    void* handle;
    
    static auto open(const std::string &path) -> std::optional<DynamicLibrary> {
        auto* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) return std::nullopt;
        return DynamicLibrary{handle};
    }
    
    auto symbol(const std::string &name) -> void* {
        return dlsym(handle, name.c_str());
    }
    
    ~DynamicLibrary() { if (handle) dlclose(handle); }
};
```

### Extern Function Resolution

```cpp
// VM opcode handler for CALL_EXTERN:
// 1. Pop function name from stack
// 2. Look up address in loaded libraries (dlsym)
// 3. Marshal NG stack values to C ABI registers
// 4. Call through assembly trampoline
// 5. Marshal return value back to NG StorageCell
// 6. Push result onto NG stack

Opcode::CALL_EXTERN: {
    auto funcName = stack.popString();
    auto funcPtr = lookupExtern(funcName);
    
    // Marshal arguments
    SysVABIArgs args;
    args.rdi = marshalToC(stack.pop(), externParams[0].type);
    args.rsi = marshalToC(stack.pop(), externParams[1].type);
    // ...
    
    // Call
    auto result = sysv_call(funcPtr, args);
    
    // Marshal back
    auto ngResult = marshalFromC(result, externReturnType);
    stack.push(ngResult);
    break;
}
```

## Effort Estimate

| Component | Effort |
|---|---|
| `extern fun` syntax (parser + AST) | 1 week |
| `*T` raw pointer type (type checker) | 1 week |
| `unsafe` keyword + blocks | 1 week |
| Calling convention via `libffi` | 1 week |
| Dynamic library loading | 1 week |
| Type mapping and marshaling | 1 week |
| Platform support (Windows ARM) | 2 weeks |
| Tests | 1 week |
| `ng-bindgen` tool (MVP) | 2 weeks |
| **Total** | **11 weeks** |

## Dependency: `libffi`

Instead of hand-writing platform-specific assembly trampolines, the implementation uses **libffi** (Foreign Function Interface):

```cpp
#include <ffi.h>

// libffi handles ABI details for all platforms:
ffi_cif cif;
ffi_prep_cif(&cif, FFI_DEFAULT_ABI, numArgs, &returnType, argTypes);
ffi_call(&cif, funcPtr, &result, argValues);
```

Benefits:
- Cross-platform: x86-64, ARM64, WASM, RISC-V
- No assembly code to maintain
- Well-tested (used by Python, LuaJIT, Ruby FFI)
- Vendored as CMake dependency

## GC Safety for `*T`

`*T` raw pointers into GC-managed memory are **unsafe** by design:
- A GC collection may move or free the memory pointed to by `*T`
- Phase 1: `*T` only valid for non-GC memory (allocated by `malloc`, `dlopen`)
- Phase 2 (future): GC pinning for `*T` to GC-managed objects
- The `unsafe` keyword explicitly acknowledges this risk