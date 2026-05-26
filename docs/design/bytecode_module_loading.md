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
- Source hash or build hash.
- Implemented: export index.
- Implemented: import dependency list.
- Type metadata needed by type checker.
- Trait metadata and impl evidence.
- Implemented: ORGASM bytecode.
- Optional debug source mapping.

## Loader Behavior

- If `.ngo` is present and compatible, load it directly.
- If `.ngo` is stale or incompatible and `.ng` exists, load source and optionally rebuild bytecode.
- If only `.ngo` exists, type checking relies on embedded type metadata.
- If both source and bytecode are missing, check native registry.

Current implementation note:

- Implemented: file loader can load `.ngo` and `module.ngo` artifacts when no source module was found first.
- Implemented: `.ngo` deserialization checks magic, format version, ABI version, module id, and bounded container sizes.
- Remaining: bytecode-first preference when both `.ngo` and `.ng` exist should wait until embedded type metadata is complete, otherwise source imports would lose precise typechecker metadata.
- Remaining: stale/source-hash rebuild policy is not implemented yet.

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
- `.ngo` and `.ng` modules expose the same exported symbols to importers.
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

- Persist `ModuleArtifact` type exports, trait exports, impl evidence, and import metadata.
- Allow type checking source modules that import bytecode-only modules without falling back to `Untyped`.
- Validate metadata schema version independently from bytecode ABI version.

### Phase 3: Bytecode-First Loader Policy

- Prefer compatible `.ngo` over `.ng` when both exist.
- Fall back to source when `.ngo` is stale, incompatible, or missing required metadata.
- Add source/build hash checks.

### Phase 4: Native And Debug Metadata

- Persist native import descriptors needed for deterministic fallback.
- Add optional debug source mapping.
- Expose useful diagnostics for bytecode import failures.
