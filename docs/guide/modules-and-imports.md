# Modules and Imports

NG uses a file-based module system. Every `.ng` file is a module; the
module name is the file name.

## Imports

```ng
import prelude;            // wildcard: imports prelude's exported surface
import string (trim, split);  // selective: only these names
```

Imports are transitive: an imported module's own wildcard imports
re-export through it, so `import prelude;` also brings the string/io
utilities the prelude re-exports.

## Exports

`export` marks items importable from other modules; non-exported
functions stay private to the module:

```ng
export fun greeting() -> string {
    return "hello from module";
}

fun helper() -> i64 {   // module-private
    return 1;
}
```

## The standard library

`lib/std/*.ng` is the stdlib, importable by name from any module
(the loader searches `lib/std` next to the importing file and upward):

| Module | Surface |
|---|---|
| `prelude` | `print`/`assert`, `not`, and re-exports of io/string |
| `io` | `readLine`, `readFile`, `writeFile`, `currentExecutablePath` |
| `system` | `system(command)`, `systemOutput(command)` — process/system command bindings (`import system;`) |
| `string` | `length`, `charAt`, `substring`, `trim`, `split`, `join`, `contains`, `replace`, `startsWith`, `endsWith`, `toUpper`, `toLower`, `regexMatch` |
| `seq` | `len`, `sum`, `arrayContains`, `reverse` |
| `list` | recursive-enum `List<T>` with `length`/`get`/`contains` and builders `listFrom`/`pushFront`/`append`/`reverseList` |
| `memory` | GC-free native handles: `box`, `read`, `write`, `outstanding` with `impl Drop` release |
| `imgui` | the Dear ImGui binding (see [ImGui Integration](/guide/imgui-integration)) |

`import prelude;` is the usual first line of a program.

## Example

`example/modules/hello.ng`:

```ng
export fun greeting() -> string {
    return "hello from module";
}
```

`example/modules/imports_main.ng`:

```ng
import hello;

fun main() -> unit {
    print(hello.greeting());
}
```

```bash
./build/ngi example/modules/imports_main.ng
```

Next: [Generics](/guide/generics).
