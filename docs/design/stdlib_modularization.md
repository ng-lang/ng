# Standard Library Modularization

## Order

Recommended Issue order: 9.
Module-system-local order: 4.

## Goal

Organize the standard library as normal source/native/bytecode modules with explicit imports and exports.

## Dependencies

Prerequisites:

- [Module Artifact And Typechecker Integration](module_artifact_typechecker.md)
- [Native Module Artifacts](native_module_artifacts.md)
- [Bytecode Module Loading](bytecode_module_loading.md), if shipping `.ngo` stdlib artifacts in the same milestone

Related:

- [Enhanced Tuple Types](enhanced_tuples.md), if tuple intrinsics are exposed through stdlib modules.
- [Auto Traits And Derive Traits](auto_derive_traits.md), if stdlib owns core marker traits.

## Scope

In scope:

- Split stdlib into explicit modules.
- Keep `std.prelude` as public facade.
- Move native-backed APIs behind NG stubs plus native descriptors.
- Ensure source-first and bytecode-first stdlib modes agree.
- Define entrypoint prelude import policy.

Out of scope:

- Initial artifact model.
- Initial native descriptor model.
- General bytecode format design.

## Layout

Recommended layout:

```text
lib/std/prelude.ng
lib/std/string.ng
lib/std/array.ng
lib/std/io.ng
lib/std/imgui.ng
native/std/prelude
native/std/imgui
```

Optional compiled layout:

```text
lib/std/prelude.ngo
lib/std/prelude.ng
```

## Rules

- Prelude is imported automatically only by entrypoint policy, not hardcoded into every module artifact.
- `std.prelude` re-exports stable common APIs.
- Native-backed standard modules use NG stubs plus native descriptors.
- Imported symbols are not re-exported by `exports *`; re-export is explicit.
- Standard library modules must be usable from STUPID and ORGASM.

## Native Module Shape

Implemented stdlib native modules:

- `std.prelude`: common language/runtime helpers.
- `std.imgui`: Dear ImGui wrapper backed by SDL3/GPU state owned by the module.

`std.imgui` exposes opaque NG native handle types for ImGui structures that are not
owned by NG:

```ng
type ImGuiContext = native;
type ImGuiIO = native;
type ImGuiStyle = native;
```

The wrapper keeps SDL window/device/event ownership internal. NG code uses ImGui-like
functions and handle accessors (`GetIO`, `GetStyle`, `ImGuiIO_*`, `ImGuiStyle_*`)
without directly importing SDL APIs.

Current wrapper coverage includes lifecycle/frame control, context/IO/style handles,
common flag constants, windows, child windows, cursor/layout, ID stack, disabled
regions, tooltips, style/color stacks, text, basic widgets, input widgets, combos,
selectables, tables, trees, tabs, menus, popups, and runtime queries. Mutable ImGui
pointer-parameter widgets return the updated value in NG, e.g. `InputText(...) ->
string`, `DragFloat(...) -> f32`, and `Checkbox(...) -> bool`.

## Acceptance Criteria

- `import std.prelude (*)` works from source modules.
- `import std.string (join, split)` works after string APIs are moved.
- Native-backed stdlib functions have NG-visible signatures.
- Implemented: `std.imgui` exposes NG-visible signatures for context, IO, style,
  flags, windows, layout, widgets, menus, popups, and runtime query wrappers.
- Examples pass with source-first stdlib resolution.
- Examples pass with bytecode-first stdlib resolution when `.ngo` artifacts are present.
- STUPID and ORGASM agree on exported stdlib symbols.
