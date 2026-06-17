# NG Programming Language Guide

Welcome to the NG programming language! This guide provides a comprehensive, progressive introduction to NG — from your first program to advanced metaprogramming.

## What is NG?

NG is a modern, statically-typed, multi-paradigm programming language implemented in C++23. It features:

- **Rich type system** — Generics, traits, tagged unions, nominal types, higher-kinded types
- **Ownership model** — Value semantics, references, moves, partial moves
- **Compile-time metaprogramming** — Const if, const predicates, const functions, type specialization
- **Dual backend** — AST interpreter (STUPID) and bytecode VM (ORGASM)
- **Native FFI** — Seamless C++ function binding
- **ImGui integration** — Immediate-mode GUI applications

## Quick Start

### Build from Source

```bash
git clone <repository-url> ng
cd ng
git submodule update --init --recursive
cmake -S . -B build -GNinja
cmake --build build -j
```

### Run Your First Program

```bash
./build/ngi example/01.id.ng
```

### Try the REPL

```bash
./build/ngi
>> print("Hello, World!");
```

### Run Tests

```bash
./build/ng_test
```

All 695+ tests pass with 3,327+ assertions.

## Learning Path

Start here and follow the guides in order:

| # | Guide | Topics |
|---|---|---|
| 1 | [Getting Started](getting-started.md) | Build, first program, REPL, backends |
| 2 | [Basic Syntax](basic-syntax.md) | Comments, variables, types, operators |
| 3 | [Control Flow](control-flow.md) | if/else, loop, switch, const if |
| 4 | [Functions](functions.md) | Definition, recursion, native functions, methods |
| 5 | [Data Structures](data-structures.md) | Arrays, tuples, objects, tagged unions, ranges |
| 6 | [Modules and Imports](modules-and-imports.md) | Module system, export/import, path resolution |
| 7 | [Generics](generics.md) | Type parameters, constraints, parameter packs |
| 8 | [References, Moves & Ownership](references-moves.md) | ref, move, partial moves, borrowing |
| 9 | [Traits](traits.md) | Definition, impl, bounds, objects, derive, auto traits |
| 10 | [Type System in Depth](type-system-in-depth.md) | Nominal types, inference, casting |
| 11 | [Compile-Time Programming](compile-time-programming.md) | const if, typeof, const predicates, const functions |
| 12 | [Advanced Generics](advanced-generics.md) | HKT, specialization, enhanced tuples, fold expressions |
| 13 | [Standard Library](standard-library.md) | Prelude, I/O, strings, collections, tuples |
| 14 | [Memory Management](memory-management.md) | Heap, GC, Drop, moves, smart pointers |
| 15 | [ORGASM Backend](orgasm-backend.md) | Bytecode compilation, VM, native bridge |
| 16 | [ImGui Integration](imgui-integration.md) | GUI programming with Dear ImGui |

## Example Programs

The `example/` directory contains 59 numbered examples that correspond to these guides:

| Range | Topic Area |
|---|---|
| 01–14 | Basics: identity, definitions, functions, strings, arrays, objects, imports, loops, tuples |
| 15 | Generics |
| 16–21 | Tagged unions, switch, const if, union types, recursion |
| 22–24 | References, moves, value semantics |
| 25–40 | Traits: Show, bounds, supertraits, defaults, objects, Copy, Clone, Drop |
| 42–47 | Compile-time: const predicates, specialization, native constraints |
| 48–49 | Higher-kinded generics, variadic HKT |
| 50–51 | Partial moves |
| 52 | Const array/vector/span |
| 53 | Const functions |
| 54 | Enhanced tuple types |
| 55 | Auto derive traits |
| 56 | Standard library modules |
| 57 | Ranges, slicing, pipeline |
| 58 | Fold expressions |
| 59 | List and sequence operations |

## Additional Resources

- **Design Documents** — `docs/design/` — Detailed design rationales for major features
- **Internal Architecture** — `docs/ref/Internals.md` — Compiler and runtime internals
- **Memory Model** — `docs/ref/Memory.md` — Memory management design
- **C++ Compatibility** — `docs/ref/cxx-compatibility.md` — FFI with C++

## Project Status

- ✅ **Compiler Pipeline**: Lexer → Parser → AST → Type Checker → Bytecode Compiler → VM
- ✅ **695+ tests passing** with 3,327+ assertions
- ✅ **59 runnable examples** demonstrating all features
- ✅ **Complete standard library** with prelude, I/O, strings, collections, tuples
- ✅ **ImGui integration** for GUI applications

## Contributing

See `CONTRIBUTING.md` for guidelines. Notable points:

- Use conventional commits (`feat:`, `fix:`, `chore:`)
- All C++23 with RAII and const correctness
- Tests use Catch2 v3 (vendored)
- AI-generated code must be disclosed