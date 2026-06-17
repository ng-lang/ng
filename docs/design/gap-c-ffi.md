# C ABI / External FFI

## Order

Recommended implementation order: **8**.

## Goal

Enable NG code to call C libraries directly without C++ wrapper code, through automatic C ABI binding generation.

## Motivation

Currently, native functions require **explicit C++ registration** in the VM:

```cpp
vm.register_native("my_function", [](int x) -> int { ... });
```

This means every C library needs a C++ shim. There is no way to:
- Call `libcurl`, `libsqlite3`, `libssl`, or any C library directly
- Bind to system APIs (POSIX, Win32) without writing C++ code
- Auto-generate bindings from C header files

## Proposed Design

### `extern` Declaration

```ng
// Direct C ABI binding
extern fun puts(s: string) -> i32;
extern fun getenv(name: string) -> string;
extern fun malloc(size: u64) -> *u8;
extern fun free(ptr: *u8) -> unit;
```

The `extern` keyword tells the compiler the function is available through C ABI, linked at runtime via `dlopen`/`dlsym`.

### Raw Pointer Type: `*T`

```ng
type Buffer {
    data: *u8;
    len: u64;
}
```

- `*T` is an unsafe raw pointer (no ownership tracking, no GC)
- Dereferencing a raw pointer does not increment reference counts
- Marked explicitly as an escape hatch, not for normal use

### Bindings Generator: `ng-bindgen`

A tool that reads C headers and generates NG bindings:

```bash
ng-bindgen /usr/include/sqlite3.h -o sqlite.ng
```

Generated output:

```ng
// Auto-generated from sqlite3.h
extern fun sqlite3_open(filename: string, ppDb: **sqlite3) -> i32;
extern fun sqlite3_close(db: *sqlite3) -> i32;
extern fun sqlite3_exec(db: *sqlite3, sql: string, ...) -> i32;

type sqlite3 is opaque;
```

### ABI Types

C-to-NG type mapping:

| C Type | NG Type |
|---|---|
| `int`, `int32_t` | `i32` |
| `int64_t` | `i64` |
| `float` | `f32` |
| `double` | `f64` |
| `char*` | `string` (copy) / `*u8` (zero-copy) |
| `void*` | `*u8` |
| `struct T` | `T` (opaque type) |
| `int*` | `*i32` |

### Dynamic Library Loading

```ng
// Manual loading (if not linked statically)
extern fun dlopen(path: string, flags: i32) -> *u8;
extern fun dlsym(handle: *u8, symbol: string) -> *u8;

val lib = dlopen("libcurl.so", RTLD_NOW);
val curl_easy_init = dlsym(lib, "curl_easy_init") as () -> *u8;
```

### Safety

- All `extern` functions are inherently `unsafe`
- Raw pointer arithmetic and dereference are `unsafe` operations
- Calling `extern` from safe code requires an `unsafe` block:

```ng
unsafe {
    val ptr = malloc(100);
    // ... raw pointer operations ...
    free(ptr);
}
```

## Dependencies

- Requires raw pointer type `*T` in the type system.
- Requires `unsafe` keyword and semantic checking.
- Requires dynamic library loading infrastructure in ORGASM VM.
- Unblocks: binding to any C library, system call access.

## Scope

**In scope:**
- `extern fun` declaration syntax
- `*T` raw pointer type
- `unsafe` keyword and block
- `dlopen`/`dlsym` integration
- ORGASM VM support for C ABI calls (via `dlsym` + FFI call)
- `ng-bindgen` tool (MVP: basic struct and function parsing)

**Out of scope:**
- C++ ABI support (C only, C++ ABI is unstable)
- COM / WinRT integration
- `union` type support in bindgen
- Callbacks from C into NG (function pointers as `extern` params)
- Automatic memory management for `*T` (GC does not track raw pointers)

## Acceptance Criteria

- An `extern fun` call executes a real C library function (e.g., `puts`)
- `*T` pointer arithmetic produces correct addresses
- `unsafe` block is required for raw pointer operations
- A compiled NG program can `dlopen` and call SQLite
- `ng-bindgen` produces valid NG bindings from a simple C header
- Native (C++) and `extern` (C ABI) functions can coexist in the same program

## Potential Challenges

- C ABI calling conventions vary by platform (x86-64 SysV vs Windows x64 vs ARM64).
- String marshaling: C `char*` may be UTF-8, ASCII, or arbitrary binary data.
- Struct layout must match the C compiler's ABI (alignment, padding).
- `extern` functions cannot participate in GC tracing — must be handled manually.
- Error handling: C functions typically return error codes, not `Result` — requires manual wrapping.
- The VM's stack-based architecture must bridge to C's register-based calling convention.