# Bytecode Module Loading

## Order

Recommended Issue order: 8.

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

## Loader Behavior

- If `.ngo` is present and compatible, load it directly.
- If `.ngo` is stale or incompatible and `.ng` exists, load source and optionally rebuild bytecode.
- If only `.ngo` exists, type checking relies on embedded type metadata.
- If both source and bytecode are missing, check native registry.

Compatibility checks:

- Module ID must match requested import.
- Bytecode ABI version must match VM/compiler.
- Type metadata schema version must match type checker.
- Imported dependency hashes can be checked later; initial implementation can warn or ignore.

## ORGASM VM Behavior

The VM should load imports by module ID rather than only pre-linked `BytecodeModule` pointers.

Required behavior:

- Resolve imported bytecode module through `ModuleRegistry`.
- Link function imports by exported symbol.
- Support native import fallback.
- Keep type/trait metadata available for dynamic dispatch and runtime diagnostics.

CLI direction:

```bash
ngi --emit-ngo build/foo.ngo src/foo.ng
ngi --run-bytecode build/foo.ngo
```

## Acceptance Criteria

- ORGASM can emit a `.ngo` artifact for a source module.
- ORGASM can execute an entry `.ngo`.
- ORGASM can execute an entry `.ngo` that imports another `.ngo`.
- `.ngo` and `.ng` modules expose the same exported symbols to importers.
- Incompatible ABI/schema versions fail before execution.
- Native imports from bytecode modules resolve through the native module registry.
