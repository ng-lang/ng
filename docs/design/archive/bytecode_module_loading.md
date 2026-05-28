# Bytecode Module Loading

## Order

Recommended Issue order: 8.
Module-system-local order: 3.

## Goal

Persist and load ORGASM bytecode modules as `.ngo` artifacts that expose the same import/export contract as source modules.

## Dependencies

Prerequisites:

- [Module Artifact And Typechecker Integration](module_artifact_typechecker.md)
- Stable generic and const-generic mangling from [Constant Generic Parameters](constant_generic_parameters.md)
- [Native Module Artifacts](native_module_artifacts.md), for native import fallback

Unblocks:

- Shipping precompiled standard library artifacts.
- [Standard Library Modularization](stdlib_modularization.md) bytecode-first mode.

## Scope

In scope:

- `.ngo` binary format.
- Bytecode artifact read/write.
- Export/import metadata persistence.
- ABI and metadata version checks.
- ORGASM VM import loading through `ModuleRegistry`.
- Native fallback for bytecode imports.

Out of scope:

- Source module resolver basics.
- Native descriptor model.
- Full debug info format.
- Incremental recompilation.

## `.ngo` Contents

`.ngo` should contain:

- Implemented: magic/version.
- Implemented: canonical module ID.
- Implemented: compiler ABI version.
- Implemented: source hash or build hash.
- Implemented: export index.
- Implemented: import dependency list.
- Implemented: type metadata needed by type checker for exported symbols.
- Implemented: trait shape metadata, impl metadata, and method evidence.
- Implemented: ORGASM bytecode.
- Optional debug source mapping.

## Loader Behavior

- If `.ngo` is present and compatible, load it directly.
- If `.ngo` is stale or incompatible and `.ng` exists, load source and optionally rebuild bytecode.
- If only `.ngo` exists, type checking relies on embedded type metadata.
- If both source and bytecode are missing, check native registry.

Current implementation note:

- Implemented: file loader can load `.ngo` and `module.ngo` artifacts before source probes when the artifact is compatible.
- Implemented: `.ngo` deserialization checks magic, format version, ABI version, metadata schema version, module id, and bounded container sizes.
- Implemented: bytecode-only imports restore exported type/trait metadata into `ModuleArtifact`, so source importers can type check exported function calls and trait-bound code.
- Implemented: stale/source-hash fallback to source when a matching `.ng` exists.
- Implemented: fallback to source when `.ngo` is incompatible or an export is missing required type metadata.

Compatibility checks:

- Module ID must match requested import.
- Bytecode ABI version must match VM/compiler.
- Type metadata schema version must match type checker.
- Imported dependency hashes can be checked later; initial implementation can warn or ignore.

## ORGASM VM Behavior

The VM should load imports by module ID rather than only pre-linked `BytecodeModule` pointers.

Required behavior:

- Implemented: resolve imported bytecode module through `ModuleRegistry`, with lazy file loading by module ID.
- Link function imports by exported symbol.
- Implemented: support native import fallback.
- Keep type/trait metadata available for dynamic dispatch and runtime diagnostics.

CLI direction:

```bash
ngi --emit-ngo build/foo.ngo src/foo.ng
ngi --run-bytecode build/foo.ngo
```

## Acceptance Criteria

- Implemented: ORGASM can emit a `.ngo` artifact for a source module.
- Implemented: ORGASM can execute an entry `.ngo`.
- Implemented: ORGASM can execute an entry `.ngo` that imports another `.ngo`.
- Implemented: `.ngo` and `.ng` modules expose the same exported symbols to importers.
- Implemented: incompatible ABI/schema versions fail before execution.
- Implemented: native imports from bytecode modules resolve through the native module registry.

## Phased Implementation Plan

### Phase 1: Bytecode Artifact Container

- Implemented: binary `.ngo` read/write for `BytecodeModule`.
- Implemented: magic, format version, ABI version, module id, imports, exports, strings, constants, functions, types, and variants.
- Implemented: CLI `--emit-ngo <path> source.ng`.
- Implemented: CLI `--run-bytecode module.ngo`.
- Implemented: VM lazy loads imported `.ngo` modules from configured module paths.
- Implemented: tests cover artifact roundtrip and an entry module importing another `.ngo`.

### Phase 2: Embedded Typechecker Metadata

- Implemented: persist exported type reprs, exported trait shape metadata, and exported impl evidence.
- Implemented: restore exported type and trait metadata into `ModuleArtifact` for source modules importing bytecode-only modules.
- Implemented: tests cover source type checking against `.ngo` function and trait metadata.
- Implemented: validate metadata schema version independently from bytecode ABI version.

### Phase 3: Bytecode-First Loader Policy

- Implemented: prefer compatible `.ngo` over `.ng` when both exist.
- Implemented: fall back to source when `.ngo` is stale or incompatible.
- Implemented: source/build hash checks.
- Implemented: fall back to source when `.ngo` is missing required metadata.

### Phase 4: Native And Debug Metadata

- Implemented: persist import dependency records and resolve native fallback through the VM native registry.
- Optional: add debug source mapping.
- Implemented: expose fail-fast diagnostics for invalid bytecode magic, ABI/schema mismatches, module id mismatches, and missing imports.
