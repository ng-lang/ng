# Module System Redesign Overview

The module system work is intentionally split into several implementation-sized designs. The old single document mixed source imports, type metadata, native modules, bytecode persistence, and standard library layout; that made it easy to create Issues with unclear scope.

## Split Documents

Implement the module-system-specific work in this local dependency order. These local numbers are not global issue-order numbers:

1. [Module Artifact And Typechecker Integration](module_artifact_typechecker.md)
2. [Native Module Artifacts](native_module_artifacts.md)
3. [Bytecode Module Loading](bytecode_module_loading.md)
4. [Standard Library Modularization](stdlib_modularization.md)

## Overall Goal

Replace the early STUPID-era module loader with a single module system that works consistently across parsing, type checking, STUPID execution, ORGASM compilation, bytecode loading, native libraries, and the standard library.

The complete system must eventually support:

- Modular standard library imports such as `import std.prelude (*)`.
- File-based `.ng` source modules.
- Native module interop for C++-registered libraries.
- ORGASM bytecode modules loaded from compiled `.ngo` artifacts.
- Compatibility between `.ng` source modules and `.ngo` bytecode modules.
- `NG_MODULE_PATH` search path compatibility.
- Cross-module exported traits and impl evidence.

## Current Problems

- `FileBasedExternalModuleLoader` is source-file-first and tightly coupled to parsing.
- Type checking has partial import behavior and can fall back to `Untyped` for unresolved imports.
- STUPID runtime modules, typechecker module artifacts, and ORGASM bytecode modules carry different metadata.
- Native functions are registered through runtime hooks and are not represented as first-class module artifacts.
- ORGASM can compile imported source modules, but there is no stable `.ngo` artifact format or loader priority.

## Non-Goal

This overview is not the implementation issue. Use the split documents above when creating concrete Issues.
