# Module System Redesign

## Goal

Replace the early STUPID-era module loader with a single module system that works consistently across parsing, type checking, STUPID execution, ORGASM compilation, bytecode loading, native libraries, and the standard library.

The module system must support:

- Modular standard library imports such as `import std.prelude (*)`.
- File-based `.ng` source modules.
- Native module interop for C++-registered libraries.
- ORGASM bytecode modules loaded from compiled artifacts.
- Compatibility between `.ng` source modules and `.ngo` bytecode modules.
- `NG_MODULE_PATH` search path compatibility.

## Current Problems

The current implementation grew from the interpreter-only era:

- `FileBasedExternalModuleLoader` is source-file-first and tightly coupled to parsing.
- Type checking has its own partial import behavior and can fall back to `Untyped` for unresolved imports.
- STUPID runtime modules, typechecker module artifacts, and ORGASM bytecode modules carry different metadata.
- ORGASM can compile imported source modules, but there is no stable bytecode module artifact format or loader priority.
- Native modules are registered through separate runtime hooks and are not represented as first-class module artifacts.

## Core Model

Introduce a unified `ModuleArtifact` abstraction.

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
  Str originPath;         // empty for built-in native modules
  Str version;
  ASTRef<CompileUnit> ast;
  TypeIndex typeIndex;
  BytecodeModule bytecode;
  RuntimeRef<StorageCell> runtimeModule;
  ModuleExportIndex exports;
  ModuleImportIndex imports;
  ModuleTraitIndex traits;
  ModuleImplIndex impls;
  NativeModuleDescriptor native;
};
```

This artifact is the contract between all stages:

- Parser produces AST for `.ng`.
- Type checker consumes `ModuleArtifact` exports and publishes type/trait/impl metadata.
- ORGASM compiler consumes the same artifact and may publish bytecode.
- STUPID consumes runtime module metadata.
- Native modules publish the same export/type/trait/native-call metadata without AST.

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
- Module identity is stable across source, bytecode, and native modules.

## Search Path Resolution

Module resolution uses ordered roots:

1. Explicit compiler/interpreter module paths.
2. `NG_MODULE_PATH`, split by platform path separator.
3. Current entry file directory.
4. Project `lib/`.
5. Installed standard library roots.
6. Registered native module table.

For import `a.b.c`, each filesystem root is probed with:

- `a/b/c.ngo`
- `a/b/c.ng`
- `a/b/c/module.ngo`
- `a/b/c/module.ng`

Default priority is bytecode first, source second. A development flag can force source first:

```text
--module-source-first
```

Rationale: production imports should prefer compiled `.ngo` artifacts when present, while development can force recompilation from source.

## `NG_MODULE_PATH`

`NG_MODULE_PATH` is a list of roots:

```bash
NG_MODULE_PATH=/path/to/project/lib:/path/to/vendor
```

Rules:

- It augments, not replaces, built-in paths.
- Earlier roots win over later roots.
- CLI-provided paths win over `NG_MODULE_PATH`.
- Standard library roots are always appended last unless explicitly overridden by a compiler flag.
- The resolved artifact records the root and physical path for diagnostics.

## Import Syntax

Keep the existing syntax:

```ng
import std.prelude (*);
import std.string (join, split);
import vendor.math as math;
```

The current parser already supports alias-like imports via post-path identifier. The grammar should be normalized:

```ebnf
ImportDecl := "import" ModulePath ImportAlias? ImportList? ";"
ImportAlias := "as" Ident
ImportList := "(" ("*" | Ident ("," Ident)*) ")"
```

Compatibility rule:

- Existing `import a.b alias (...);` remains accepted initially.
- New examples and docs should use `as`.

## Export Model

A module exports symbols and evidence:

- Values.
- Functions.
- Types and type aliases.
- Traits.
- Trait impl evidence.
- Const predicates.
- Native functions.
- Native opaque types.

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
- Native module exported impl evidence.

Rules:

- Duplicate visible exact impl `(Trait, Type)` is an error.
- Overlapping visible generic/concrete impls are an error unless selected.
- Explicit selection is:

```ng
use impl module::Trait for Type;
use impl Trait for Type;
```

The module-qualified form selects an imported module's evidence. The unqualified form is local or single-visible selection.

Diagnostics must include:

- Trait name.
- Target type.
- Candidate module IDs.
- Suggested `use impl module::Trait for Type;` when applicable.

## Native Modules

Native modules should be first-class `ModuleArtifact`s.

```cpp
struct NativeModuleDescriptor {
  Str moduleId;
  Map<Str, NGCallable> functions;
  Map<Str, RuntimeRef<NGType>> types;
  Set<Str> traits;
  Vec<TraitImplEvidence> impls;
  TypeIndex typeIndex;
};
```

Rules:

- `type T = native;` remains the NG-side opaque value declaration.
- Native functions must have NG signatures available either from `.ng` stub modules or native descriptors.
- Native modules can be imported with normal `import`.
- Native modules can export native opaque types, functions, traits, and impl evidence.
- Standard library native modules should have `.ng` signature stubs so type checking is source-visible and runtime binding is native.

Recommended layout:

```text
lib/std/prelude.ng
lib/std/imgui.ng
native/std/prelude      // registered C++ descriptor
native/std/imgui        // registered C++ descriptor
```

## Source And Bytecode Compatibility

`.ng` and `.ngo` modules expose the same artifact contract.

`.ngo` should contain:

- Magic/version.
- Canonical module ID.
- Compiler ABI version.
- Source hash or build hash.
- Export index.
- Import dependency list.
- Type metadata needed by type checker.
- Trait metadata and impl evidence.
- ORGASM bytecode.
- Optional debug source mapping.

Loader behavior:

- If `.ngo` is present and compatible, load it directly.
- If `.ngo` is stale or incompatible and `.ng` exists, load source and optionally rebuild bytecode.
- If only `.ngo` exists, type checking relies on embedded type metadata.
- If both are missing, check native registry.

Compatibility checks:

- Module ID must match requested import.
- Bytecode ABI version must match VM/compiler.
- Type metadata schema version must match type checker.
- Imported dependency hashes can be checked later; initial implementation can warn or ignore.

## Standard Library

Standard library should be organized as normal modules:

```text
lib/std/prelude.ng
lib/std/string.ng
lib/std/array.ng
lib/std/imgui.ng
```

Rules:

- Prelude is imported automatically only by entrypoint policy, not hardcoded into every module artifact.
- `std.prelude` should re-export stable common APIs.
- Native-backed standard modules use NG stubs plus native descriptors.
- ORGASM bytecode stdlib artifacts can be shipped next to source:

```text
lib/std/prelude.ngo
lib/std/prelude.ng
```

## Loader API

Replace source-specific loading with a resolver plus loaders.

```cpp
class ModuleResolver {
  ModuleResolution resolve(ModuleId id, ModuleLoadOptions options);
};

class ModuleLoader {
  ModuleArtifact load(ModuleResolution resolution);
};

class SourceModuleLoader;
class BytecodeModuleLoader;
class NativeModuleLoader;
```

`ModuleRegistry` stores artifacts by canonical module ID:

```cpp
class ModuleRegistry {
  Map<Str, ModuleArtifact> artifacts;
  ModuleArtifact &load(ModuleId id, ModuleLoadOptions options);
};
```

The registry must support:

- Cycle detection.
- Per-stage artifact completion.
- Cache invalidation.
- Source and bytecode coexistence.
- Native fallback.

## Compilation Pipeline

For source entrypoint:

1. Parse entry source.
2. Resolve imports recursively.
3. Build dependency graph.
4. Type-check modules in dependency order, with cycle handling.
5. Publish export/type/trait/impl metadata.
6. Compile to ORGASM bytecode if requested.
7. Save `.ngo` artifacts if requested.
8. Execute entry module via STUPID or ORGASM.

For bytecode entrypoint:

1. Load `.ngo`.
2. Load dependency artifacts.
3. Validate metadata/ABI.
4. Execute ORGASM bytecode.

## ORGASM Module Loading

ORGASM VM should load imports by module ID rather than only pre-linked `BytecodeModule` pointers.

Required VM behavior:

- Resolve imported bytecode module through `ModuleRegistry`.
- Support native import fallback for native modules.
- Link function imports by exported symbol.
- Keep type/trait metadata available for dynamic dispatch and runtime diagnostics.

Persistence:

```bash
ngi --emit-ngo build/foo.ngo src/foo.ng
ngi --run-bytecode build/foo.ngo
```

The first implementation can serialize bytecode plus minimal export/import metadata. Type metadata serialization can follow, but the artifact type should reserve space for it.

## STUPID Compatibility

STUPID should consume the same `ModuleArtifact` graph:

- Source modules can still be interpreted directly.
- Native modules bind through runtime module metadata.
- Trait names and impl evidence must be imported consistently.
- Runtime module cells should reflect the artifact export table.

STUPID remains useful for debugging, but it should not define a separate module semantics.

## Implementation Phases

### Phase 1: Artifact Model And Resolver

- Add `ModuleArtifact`, `ModuleResolver`, and load options.
- Parse `NG_MODULE_PATH`.
- Keep existing `.ng` loading internally but expose it through the new artifact API.
- Add tests for resolution priority and module ID validation.

### Phase 2: Typechecker Integration

- Move module artifact type exports, trait exports, and impl evidence out of ad-hoc static maps.
- Type-check imports through `ModuleRegistry`.
- Preserve fallback only for explicitly native/test modules.
- Add ambiguity diagnostics for duplicate imported impls.

### Phase 3: Native Module Artifacts

- Represent native modules as artifacts.
- Move prelude/imgui native registrations behind native descriptors.
- Ensure `.ng` stubs and C++ descriptors agree.
- Add tests for native type/function import.

### Phase 4: Bytecode Artifact Loading

- Define `.ngo` binary format.
- Implement bytecode read/write.
- Let ORGASM compiler emit `.ngo`.
- Let VM load imported `.ngo` modules through registry.

### Phase 5: Standard Library Modularization

- Split stdlib into modules with explicit exports.
- Compile stdlib to `.ngo` optionally.
- Keep `std.prelude` as public facade.
- Ensure both source-first and bytecode-first modes pass examples.

## Acceptance Criteria

- `NG_MODULE_PATH` affects source and bytecode module lookup.
- `import std.prelude (*)` works from source and bytecode.
- `.ngo` and `.ng` modules expose the same exported symbols to importers.
- Native modules can be imported with the same syntax as source modules.
- A native-backed stdlib module has NG-visible signatures and C++ runtime handlers.
- Duplicate visible impls from different modules fail deterministically.
- `use impl module::Trait for Type;` selects one visible impl.
- ORGASM can execute an entry module that imports another compiled `.ngo`.
- STUPID and ORGASM agree on import/export visibility for functions, types, traits, and impl evidence.
