# Module Artifact And Typechecker Integration

## Order

Recommended Issue order: 1.
Module-system-local order: 1.

## Goal

Introduce the shared module artifact model and make the type checker consume imports through that model. This is the root dependency for later native modules, bytecode modules, stdlib modularization, and cross-module trait impl visibility.

## Dependencies

Prerequisites:

- Current parser, typechecker, STUPID, and ORGASM module behavior.

Unblocks:

- [Native Module Artifacts](native_module_artifacts.md)
- [Bytecode Module Loading](bytecode_module_loading.md)
- [Standard Library Modularization](stdlib_modularization.md)
- [Auto Traits And Derive Traits](auto_derive_traits.md), for cross-module impl coherence.

## Scope

In scope:

- `ModuleId` with canonical dotted names.
- `ModuleArtifact` as the shared contract between parser, typechecker, STUPID, ORGASM, and later loaders.
- `ModuleResolver` and load options for source modules.
- `NG_MODULE_PATH` parsing and resolution priority.
- Import/export metadata for values, functions, types, type aliases, traits, const predicates, and impl evidence.
- Typechecker import integration through `ModuleRegistry`.
- Duplicate visible impl diagnostics.

Out of scope:

- Native descriptors.
- `.ngo` binary format.
- Stdlib physical reorganization.
- Runtime bytecode import loading.

## Core Model

```cpp
enum class ModuleFormat {
  SourceNg,
  BytecodeNgo,
  Native,
};

struct ModuleId {
  Str canonicalName;      // e.g. "std.prelude"
  Vec<Str> pathSegments;  // e.g. ["std", "prelude"]
};

struct ModuleArtifact {
  ModuleId id;
  ModuleFormat format;
  Str originPath;
  Str version;
  ASTRef<CompileUnit> ast;
  TypeIndex typeIndex;
  BytecodeModule bytecode;
  RuntimeRef<StorageCell> runtimeModule;
  ModuleExportIndex exports;
  ModuleImportIndex imports;
  ModuleTraitIndex traits;
  ModuleImplIndex impls;
};
```

The artifact is the contract between stages:

- Parser produces AST for `.ng`.
- Type checker consumes artifact exports and publishes type/trait/impl metadata.
- ORGASM compiler consumes artifact metadata and may publish bytecode later.
- STUPID consumes runtime module metadata.

## Module Identity

Module names are canonical dotted names:

```ng
module std.prelude exports *;
import std.prelude (*);
```

Rules:

- The canonical module ID is the dotted import path.
- File path is only a resolution mechanism, not identity.
- `module foo.bar;` inside a file must match import path `foo.bar` when imported by name.
- A source file without a `module` declaration gets the canonical ID from its import path or CLI entrypoint.
- Module identity must stay stable across source, bytecode, and native modules.

## Search Path Resolution

Module resolution uses ordered roots:

1. Explicit compiler/interpreter module paths.
2. `NG_MODULE_PATH`, split by platform path separator.
3. Current entry file directory.
4. Project `lib/`.
5. Installed standard library roots.
6. Registered native module table, after [Native Module Artifacts](native_module_artifacts.md).

For import `a.b.c`, each filesystem root is probed with:

- `a/b/c.ng`
- `a/b/c/module.ng`

Bytecode probes are added later by [Bytecode Module Loading](bytecode_module_loading.md).

## Import Syntax

Keep the existing syntax:

```ng
import std.prelude (*);
import std.string (join, split);
import vendor.math as math;
```

Normalize grammar:

```ebnf
ImportDecl := "import" ModulePath ImportAlias? ImportList? ";"
ImportAlias := "as" Ident
ImportList := "(" ("*" | Ident ("," Ident)*) ")"
```

Compatibility rule:

- Existing `import a.b alias (...);` remains accepted initially.
- New examples and docs should use `as`.

## Export Model

A module exports:

- Values.
- Functions.
- Types and type aliases.
- Traits.
- Trait impl evidence.
- Const predicates.

Export forms:

```ng
module m exports *;
export fun f() -> unit { ... }
export type T { ... }
export trait Show { ... }
export impl Show for T { ... }
```

Rules:

- `exports *` exports local public definitions only.
- Re-export requires explicit export of imported symbols.
- Imported symbols are not re-exported by `exports *`.
- Impl evidence is not callable by name, but it participates in trait satisfaction and coherence.
- `export impl` is the preferred explicit form for impl evidence.

## Trait Impl Visibility

Visible impls come from:

- Local module impls.
- Imported modules' exported impl evidence.

Native module impl evidence is added later.

Rules:

- Duplicate visible exact impl `(Trait, Type)` is an error.
- Overlapping visible generic/concrete impls are an error unless selected by a later explicit selection feature.
- Diagnostics must include trait name, target type, and candidate module IDs.

## Acceptance Criteria

- `NG_MODULE_PATH` affects source module lookup.
- `import std.prelude (*)` resolves through `ModuleRegistry`.
- Source modules expose exported functions, types, traits, and impl evidence to importers.
- Imported impl evidence participates in trait satisfaction.
- Duplicate visible impls from different modules fail deterministically.
- STUPID and ORGASM agree on import/export visibility for source modules.
