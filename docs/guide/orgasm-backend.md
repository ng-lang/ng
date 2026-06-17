# ORGASM Backend

ORGASM is NG's bytecode compilation and virtual machine backend. It compiles type-checked AST into compact bytecode modules (`.ngo` files), which are then executed by a stack-based VM.

## Overview

The NG execution pipeline:

```
Source (.ng)
    → Lexer → Parser → AST
    → Type Checker
    → ORGASM Compiler → Bytecode (.ngo)
    → ORGASM VM → Result
```

## Compilation Pipeline

### Default Behavior

The `ngi` interpreter uses ORGASM by default:

```bash
./build/ngi example/01.id.ng
```

This triggers: parse → type-check → compile → run.

### Emitting Bytecode

Compile to `.ngo` without running:

```bash
./build/ngi --emit-ngo output.ngo example/01.id.ng
```

### Running Pre-compiled Bytecode

```bash
./build/ngi --run-bytecode output.ngo
```

### Using the AST Interpreter (STUPID)

```bash
./build/ngi --stupid example/01.id.ng
```

## Bytecode Modules (`.ngo`)

A `.ngo` file contains a complete compiled module with:

- **Constants pool** — string and numeric literals
- **Function definitions** — bytecode instructions per function
- **Type descriptors** — type information for runtime dispatch
- **Import/Export table** — module dependencies and visible symbols
- **Source hash** — for staleness detection

### Module Loading

When importing modules, the loader:

1. Looks for `module.ngo` alongside `module.ng`
2. If found, checks the source hash matches the current source
3. If hash matches, loads the bytecode directly (fast)
4. If hash is stale or no artifact, compiles from source

### Module Merging

When multiple modules are linked, ORGASM can **merge** bytecode modules:

- Remaps type operands across module boundaries
- Rebuilds function indices
- Prefixes exports to avoid name conflicts

## Bytecode Instruction Set

Key opcodes include:

| Category | Opcodes |
|---|---|
| **Constants** | `PUSH_I32`, `PUSH_F64`, `PUSH_BOOL`, `PUSH_STR`, `PUSH_UNIT` |
| **Variables** | `LOAD`, `STORE`, `LOAD_LOCAL`, `STORE_LOCAL`, `LOAD_GLOBAL` |
| **Operations** | `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`, `EQ`, `NE`, `LT`, `GT`, `LE`, `GE` |
| **Control** | `JUMP`, `JUMP_IF_FALSE`, `CALL`, `CALL_NATIVE`, `RETURN` |
| **Memory** | `NEW`, `FIELD_LOAD`, `FIELD_STORE`, `DEREF_LOAD`, `DEREF_STORE`, `MOVE` |
| **Arrays** | `ARRAY_NEW`, `ARRAY_GET`, `ARRAY_SET`, `ARRAY_LEN`, `ARRAY_APPEND` |
| **Tagged Unions** | `MAKE_VARIANT`, `VARIANT_TAG`, `SWITCH` |
| **Traits** | `TRAIT_IMPL_CALL`, `TRAIT_OBJ_CALL`, `TRAIT_CAST` |
| **Modules** | `CALL_IMPORT`, `LOAD_IMPORT` |

## Native Function Bridge

Native (C++) functions can be registered with the VM:

```cpp
#include "orgasm/vm.hpp"

NG::orgasm::VM vm{modulePaths};

// Register a simple native function
vm.register_native("my_function", [](int x, double y) -> string {
    return "result: " + std::to_string(x + y);
});

vm.run(bytecode);
```

### How Native Bridging Works

1. NG source declares `fun my_function(x: i32, y: f64) -> string = native;`
2. The compiler emits `CALL_NATIVE` with the function name
3. The VM looks up the registered native by name
4. The native bridge automatically converts between NG `StorageCell` values and C++ types
5. Return values are marshaled back into the VM stack

### Standard Library Natives

The prelude registers native functions:

```ng
// Prelude natives include:
fun print<T...>(args: T...) -> unit = native;
fun assert<T...>(condition: T...) -> unit = native;
fun not(value: bool) -> bool = native;
fun len<T>(xs: T) -> i32 = native;
```

### Custom Native Libraries

You can create and register your own native libraries:

```cpp
#include "orgasm/native_bridge.hpp"

// Register a group of related natives
NG::library::my_library::register_vm_natives(vm);
```

## VM Architecture

### Stack-Based Execution

The ORGASM VM uses an operand stack:

```
Before:   stack = [a, b]
Instruction: ADD
After:    stack = [a + b]
```

### Call Frames

Each function call creates a frame with:
- **Local variables** — slots for function parameters and locals
- **Operand stack** — for intermediate values
- **Return address** — where to continue after the call
- **Lexical scope** — for closure support

### Memory Management

The VM uses `StorageCell` slots managed with `std::shared_ptr`. The managed heap:

- Allocates cells for `new` expressions
- Tracks references through slot references and call-frame roots
- Runs a tracing garbage collector for cyclic garbage
- Supports finalizers (`Drop` trait) during collection

### Garbage Collection

```ng
type Node {
    next: ref<Node>;
}

val a = new Node { next: null };
val b = new Node { next: a };
a.next = b;   // Creates a cycle — GC handles this

// When a and b go out of scope, the GC collects the cycle
```

## Safety Features

The VM includes bounds checking and safety guards:

```ng
// Bounds-checked array access
arr[100];   // Runtime error: index out of bounds

// Stack underflow protection (VM internal)
// Use-after-move detection
val x = move y;
print(y);   // Runtime error: use after move
```

## Performance Considerations

- ORGASM bytecode is significantly faster than AST interpretation
- Native function calls have minimal overhead
- The VM uses direct threaded code execution
- Bytecode modules load faster than source compilation

## What's Next?

Continue to [ImGui Integration](imgui-integration.md) for GUI programming with NG.

> **Try it:** Run any example with `--run-bytecode` to see the VM in action
> **Try it:** The test suite `[OrgasmTest]` tag covers all VM features