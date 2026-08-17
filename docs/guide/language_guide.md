# NG Language Guide

Welcome to the NG language guide. NG is a statically-typed, multi-paradigm
programming language with a single clean pipeline:

**Lexer → Parser → Resolver (HIR) → Type Checker → FlowIR → QBE IL → Native executable**

## What's in a name?

**ng** is a unique sound in Chinese linguistics:

- **Velar nasal coda** — the ending `/ŋ/` that completes syllables like "zhōng" (中) and "běijīng" (京) in Mandarin
- **Initial consonant** — `/ŋ/` appears as a syllable onset in several Chinese dialects (Cantonese, Hakka, Wu, etc.)
- **New Generation** — a language designed for the next generation of software development

## What is NG?

- **Rich type system** — fixed-width integers (`i8`–`i64`, `u8`–`u64`),
  `f32`/`f64`, `bool`, `string`, arrays (dynamic and fixed-size), tuples,
  structs, tagged unions (`enum`), union annotations (`A | B`), ranges,
  references, trait views, opaque/native handles, generics, and
  higher-kinded type constructors.
- **Ownership model** — copy-first value semantics: `Copy` types copy on
  bind/call/return, affine nominal types move (`move`/`clone`) with
  `impl Drop` lifecycle, and scoped `ref`/`ref mut` views are borrow-checked
  with non-lexical loan release. No GC.
- **Traits** — trait declarations with default methods, impls (concrete and
  generic), `T: Trait` bounds, auto traits, `derive(Copy + Clone)`, and
  `ref<Trait>` dynamic views.
- **Compile-time programming** — `const if`, const declarations with pattern
  specialization, `const fun` (compile-time capable and runtime callable),
  where clauses, const generics, and const-capable native hosts.
- **Modules** — transitive imports with per-module visibility, a redesigned
  standard library (`lib/std`), and an embedding-friendly `native fun`
  interface.
- **ImGui binding** — a Dear ImGui binding over SDL3 and a minimal IDE
  written in NG itself.

## Why NG?

### Compact binaries

NG produces remarkably small native executables. The following table
shows an illustrative snapshot of "Hello World" binary sizes across
languages, measured on macOS 15 (arm64) with each toolchain's default
settings:

| Language | Hello World binary size | Toolchain |
|----------|------------------------|-----------|
| **NG**   | 55,992 bytes           | ngi --native (QBE + cc) |
| C++      | 318,040 bytes          | clang++ -std=c++23 |
| Rust     | 442,136 bytes          | rustc 1.x |
| Zig      | 1,873,096 bytes        | zig build-exe |

> **Note:** These numbers are illustrative and depend on platform,
> toolchain version, build flags, and linking mode. They demonstrate
> NG's minimal runtime overhead rather than absolute benchmarks.

NG's minimal runtime and efficient code generation mean your programs
carry less overhead — ideal for embedded systems, CLI tools, and
deployments where binary size matters.

## Quick Start

### Build

```bash
cmake -S . -B build -GNinja
cmake --build build -j
```

### Run a program

```bash
./build/ngi example/stdlib_basics.ng        # run an example file
./build/ngi --source 'import prelude; fun main() { print("hi"); }'
./build/ngi example/ng_ide.ng   # the imgui IDE (headless AOT stub)
```

`ngi --fuel <n>` bounds the instruction budget (`0` lifts it, used by
interactive programs).

### First program

```ng
import prelude;

fun main() -> i64 {
    let greeting = "hello";
    print(greeting);
    return 42;
}
```

## Language tours

- [Getting Started](/guide/getting-started) — setup, first programs, `ngi`
- [Basic Syntax](/guide/basic-syntax) — bindings, types, operators
- [Control Flow](/guide/control-flow) — if, loop/next, switch, const if
- [Functions](/guide/functions) — declarations, generics, native/const fun
- [Data Structures](/guide/data-structures) — structs, enums, tuples, arrays
- [Modules and Imports](/guide/modules-and-imports) — visibility, stdlib
- [References, Moves & Ownership](/guide/references-moves) — the ownership model
- [Traits](/guide/traits) — traits, impls, bounds, views
- [Generics](/guide/generics) and [Advanced Generics](/guide/advanced-generics)
- [Compile-Time Programming](/guide/compile-time-programming)
- [Standard Library](/guide/standard-library)
- [Memory Management](/guide/memory-management) — the GC-free heap
- [ImGui Integration](/guide/imgui-integration) — the binding and the IDE

## Reference

- [Internals](/ref/Internals) — the compiler pipeline
- [Memory](/ref/Memory) — the runtime value model
- [C++ Compatibility](/ref/cxx-compatibility)
