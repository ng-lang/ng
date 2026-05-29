# Native Module Artifacts

## Order

Recommended Issue order: 5.
Module-system-local order: 2.

Implementation status: initial implementation landed on the module artifact branch. Native
descriptors are now first-class registry inputs for the type checker, STUPID, and ORGASM.

## Goal

Represent native modules as first-class module artifacts so native functions, native opaque types, traits, and impl evidence can be imported with the same syntax and metadata path as source modules.

## Dependencies

Prerequisites:

- [Module Artifact And Typechecker Integration](module_artifact_typechecker.md)

Unblocks:

- Native-backed standard library modules.
- [Standard Library Modularization](stdlib_modularization.md)
- More reliable `type T = native;` type checking.

## Scope

In scope:

- `NativeModuleDescriptor`.
- Native module registration in `ModuleRegistry`.
- NG-visible native signatures through `.ng` stubs or descriptor metadata.
- Native opaque types via `type T = native;`.
- Native functions imported with normal `import`.
- Native-exported trait and impl evidence.
- Descriptor/stub agreement checks.

Out of scope:

- `.ngo` bytecode persistence.
- Splitting the standard library into final module layout.
- Procedural native code generation.

## Core Model

```cpp
struct NativeModuleDescriptor {
  Str moduleId;
  Map<Str, NGCallable> functions;
  TypeIndex typeIndex;
  ModuleExportIndex exports;
  ModuleTraitIndex traits;
  ModuleImplIndex impls;
  Str origin;
  Str version;
  bool requireSignatures = false;
};
```

Rules:

- Native modules are `ModuleArtifact{.format = ModuleFormat::Native}`.
- `type T = native;` remains the NG-side opaque value declaration.
- Native functions must have NG signatures available either from `.ng` stub modules or native descriptors.
- Native modules can export opaque types, functions, traits, and impl evidence.
- Native module impl evidence participates in duplicate impl diagnostics.
- Descriptor registration publishes a shared `ModuleArtifact` and registers a runtime module.
- If a source stub runtime module has already been loaded, descriptor registration augments it
  with native handlers instead of replacing NG-defined helper functions.
- If no source stub exists, STUPID falls back to the native runtime module registered by the
  descriptor.
- ORGASM treats imported native functions as external imports backed by VM native handlers.

Recommended layout:

```text
lib/std/prelude.ng
lib/std/imgui.ng
native/std/prelude      // registered C++ descriptor
native/std/imgui        // registered C++ descriptor
```

## Descriptor And Stub Agreement

If a native module has an NG stub, the checker should verify:

- Every exported native function in the stub exists in the descriptor.
- Every descriptor function used by imports has a visible NG type.
- Native opaque types in the stub match descriptor type names.
- Exported trait/impl evidence in descriptor and stub do not conflict.

## Acceptance Criteria

- A native module can be imported with normal `import`.
- A native-backed function has a checked NG signature and a C++ runtime handler.
- A native opaque type can be exported and used by source modules.
- Native-exported impl evidence participates in trait satisfaction.
- Descriptor/stub mismatches produce deterministic diagnostics.

## Implemented Coverage

- `NativeModuleDescriptor` registration in `ModuleRegistry`.
- Native descriptor metadata import in the type checker, including descriptor-only modules with
  no source file.
- STUPID import fallback for native-only artifacts.
- STUPID source-stub compatibility, including mixed NG helper functions plus native handlers.
- ORGASM external native imports for descriptor-backed modules.
- Native descriptors for `std.prelude` and `std.imgui`.

## Remaining Work

- Persisted native artifact metadata for `.ngo` module loading.
- Full duplicate trait impl diagnostics across native/source module boundaries.
- Richer descriptor/stub agreement diagnostics for trait and impl evidence mismatches.
