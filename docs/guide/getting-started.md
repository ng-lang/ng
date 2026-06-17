# Getting Started with NG

Welcome to NG — a modern, statically-typed, multi-paradigm programming language. This guide will help you set up your environment and write your first NG programs.

## Overview

NG is designed with these core principles:

- **Simplicity** — A small, consistent set of features that compose well.
- **Readability** — Clean syntax that stays out of your way.
- **Performance** — Compiles to efficient bytecode (ORGASM) that runs close to the metal.
- **Safety** — Prevents null pointer dereferences, buffer overflows, and use-after-move errors at compile time.

## Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| C++ compiler | C++23 (Clang ≥15, GCC ≥13, MSVC 2022) | Build the `ngi` interpreter |
| CMake | ≥3.25.1 | Build system configuration |
| Ninja or Make | — | Build execution |
| Git | — | (for submodules) |

## Building from Source

### 1. Clone the repository

```bash
git clone <repository-url> ng
cd ng
git submodule update --init --recursive
```

### 2. Configure with CMake

```bash
cmake -S . -B build -GNinja
```

### 3. Build

```bash
cmake --build build -j
```

This produces two executables in `build/`:

| Executable | Description |
|---|---|
| `ngi` | The NG interpreter / bytecode runner |
| `ng_test` | The test suite (695+ tests) |

## Running Your First Program

Create a file called `hello.ng`:

```ng
// hello.ng
print("Hello, World!");
```

### Run with the AST interpreter (default backend):

```bash
./build/ngi example/01.id.ng
```

### Or use the ORGASM bytecode VM:

```bash
./build/ngi --run-bytecode example/01.id.ng
```

### Try the REPL:

```bash
./build/ngi
>> print("Hello from REPL!");
```

## Running the Examples

The `example/` directory contains **59 numbered examples** that progressively demonstrate NG's features:

```bash
# Run specific examples
./build/ngi example/01.id.ng
./build/ngi example/14.tuple.ng
./build/ngi example/16.tagged_union.ng

# Run all examples via the test suite
./build/ng_test "[OrgasmExample]"
```

## Running Tests

```bash
# Run the full test suite
ctest --test-dir build -j

# Or run the test binary directly
./build/ng_test

# Filter by category
./build/ng_test "generics*"
./build/ng_test "[Traits][TypeCheck]"
```

## Two Execution Backends

NG has two runtimes:

### 1. STUPID (AST Interpreter)

The default backend. Parses source → type-checks → interprets the AST directly. Use `--stupid` flag explicitly:

```bash
./build/ngi --stupid example/15.generics.ng
```

### 2. ORGASM (Bytecode VM)

Compiles the typed AST into bytecode modules (`.ngo`), then executes them in a register-based VM. More performant and supports native function bridging:

```bash
./build/ngi --run-bytecode example/01.id.ng
# Or just (no flag uses ORGASM by default):
./build/ngi example/01.id.ng
```

You can also emit bytecode without running:

```bash
./build/ngi --emit-ngo output.ngo example/01.id.ng
```

## Hello World Explained

```ng
// This is a single-line comment.
print("Hello, World!");
```

- `//` starts a comment that continues to the end of the line.
- `print` is a built-in function from the standard prelude that accepts any number of arguments.
- `"Hello, World!"` is a string literal.
- `;` terminates every statement.

## What's Next?

Continue with [Basic Syntax](basic-syntax.md) to learn about variables, types, and operators.

> **Tip:** All numbered examples in `example/` correspond to the topics covered in these guides. Run them alongside your reading!