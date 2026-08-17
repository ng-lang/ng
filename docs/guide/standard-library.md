# Standard Library

The standard library lives in `lib/std/` and is imported by module name.
It is deliberately redesigned (not a port): value-typed sequences, a
recursive-enum `List`, and a GC-free memory module.

## `prelude`

```ng
import prelude;
```

`print` (i64/string/bool/f64/f32 overloads), `assert`, `not`, and
`system(command) -> i64` and `systemOutput(command) -> string` — process/system command bindings
inside the running program and returns its captured output (plus
diagnostics with a trailing `[exit N]` on failure). The prelude
re-exports the io and string surfaces.

## `io`

`readLine()`, `readFile(path)`, `writeFile(path, contents)`,
`currentExecutablePath()`.

## `string`

`length`, `charAt`, `substring`, `trim`, `split`, `join`, `contains`,
`replace`, `startsWith`, `endsWith`, `toUpper`, `toLower`, `regexMatch`
(invalid patterns are runtime/compile-time errors).

```ng
let parts = split("  alpha,beta  ", ",");
assert(join(parts, "|") == " alpha|beta ");
assert(regexMatch("a1b2", "a.b."));
```

## `seq`

Array helpers as free functions: `len(array<T>)` (generic), `sum`,
`arrayContains`, `reverse`.

## `list`

The recursive-enum sequence:

```ng
enum List<T> {
    Cons(head: T, tail: ref<List<T>>),
    Nil,
}
```

Traversal: `length`, `get`, `contains` (all take `ref<List<T>>`).
Immutable builders: `listFrom(array<T>)`, `pushFront`, `append`,
`reverseList`:

```ng
let made = listFrom([1, 2, 3]);
let grown = append(ref made, 7);
let front = pushFront(ref made, 5);
let reversed = reverseList(ref made);
```

In-place mutating builders remain deferred to the runtime-session heap
work.

## `memory`

GC-free native handles:

```ng
let mut cell = box(7);
write(ref cell, 9);
let value = read(ref cell);
assert(outstanding() == 1);
```

`Box` is a concrete handle with `impl Drop` release (`example/heap_box.ng`
demonstrates the scope-driven free). Generic `Box<T>`/`Gc`/`Arc` are
deferred.

## `imgui`

The Dear ImGui binding (`lib/std/imgui.ng`, lowered to `$ngrt_imgui*`) — see
[ImGui Integration](/guide/imgui-integration).

## Umbrella

`lib/std.ng` re-exports the prelude surface; individual modules are the
usual import path (`import string;`, `import seq;`, ...).

Next: [Memory Management](/guide/memory-management).
