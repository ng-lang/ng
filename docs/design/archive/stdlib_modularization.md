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

- [Enhanced Tuple Types](../enhanced_tuples.md), if tuple intrinsics are exposed through stdlib modules.
- [Auto Traits And Derive Traits](../auto_derive_traits.md), if stdlib owns core marker traits.

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
- Re-exporting imported modules uses definition-prefix export syntax:
  `export import std.string (*);`.
- Native-backed standard modules use NG stubs plus native descriptors.
- Imported symbols are not re-exported by `exports *`; re-export is explicit.
- Imported symbols retain source module provenance. Re-importing the same symbol
  from the same source module is idempotent; importing the same short name from
  different modules is a conflict unless the caller keeps the modules qualified.
- Module-only imports remain the conflict-avoidance path:
  `import some.module as m; m.symbol(...)`.
- Future short-name aliasing for individual imported symbols is tracked in
  [Symbol Import Aliases](../symbol_import_aliases.md).
- Standard library modules must be usable from STUPID and ORGASM.

## Native Module Shape

Implemented stdlib native modules:

- `std.prelude`: common language/runtime helpers.
- `std.io`: input/output and process-adjacent helpers (`readLine`,
  `readFile`, `writeFile`, `currentExecutablePath`, `runNgi`).
- `std.string`: string helpers and regex predicate support (`split`, `join`,
  `trim`, `contains`, `replace`, `startsWith`, `endsWith`, `toUpper`,
  `toLower`, `regexMatch`).
- `std.array`: array-oriented helpers (`reverse`). Range, slice, and fold are
  language syntax rather than stdlib helper APIs.
- `std.seq`: `Sequence<T>` trait protocol for sequential containers (`size`
  and `get` in the current baseline).
- `std.list`: NG-authored linked list implementation built on tagged unions,
  references, inherent `List<T>` methods (`pushBack`, `append`, `get`), and a
  `Sequence<T>` impl. `listof<T>(args: T...)` provides homogeneous varargs
  construction; the free `pushBack`/`append`/`get` functions remain available
  as explicit ref-passing APIs.
- `std.memory`: native allocation/profiler helpers (`UniquePtr`,
  `nativeMalloc`, `nativeFree`, `nativeOutstandingAllocations`, `gcFree`).
- `std.imgui`: Dear ImGui wrapper backed by SDL3/GPU state owned by the module.

`std.prelude` is now a facade over the smaller std modules. It uses
`export import std.tuple (*)`, `export import std.io (*)`,
`export import std.string (*)`, `export import std.array (*)`,
`export import std.seq (*)`, and `export import std.memory (*)` to explicitly re-export stable symbols that
should remain available through entrypoint prelude loading. Direct module imports such as
`import std.string (trim, split)` and `import std.array (reverse)` are
supported in both STUPID and ORGASM.

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
- Implemented: `import std.string (join, split)` works after string APIs are moved.
- Implemented: `import std.io (...)`, `import std.array (...)`, and
  `import std.memory (...)` resolve as source modules backed by native handlers.
- Native-backed stdlib functions have NG-visible signatures.
- Implemented: `std.imgui` exposes NG-visible signatures for context, IO, style,
  flags, windows, layout, widgets, menus, popups, and runtime query wrappers.
- Examples pass with source-first stdlib resolution.
- Examples pass with bytecode-first stdlib resolution when `.ngo` artifacts are present.
- STUPID and ORGASM agree on exported stdlib symbols.
