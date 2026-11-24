# Modules and Imports

NG uses a file-based module system. Every `.ng` file is a module; the
module name is the file name.

## Imports

```ng
import prelude;                 // wildcard: imports prelude's exported surface
import string (trim, split);    // selective: only these names
```

Imported names are used **unqualified**. There is no qualified module access:
writing `hello.greeting()` after `import hello;` fails, because the module name
is not a value or namespace expression:

```
resolution error E-name-not-visible: name `hello` is not visible in this module
```

Imports are transitive: an imported module's own wildcard imports
re-export through it, so `import prelude;` also brings the `string` and `io`
surface the prelude re-exports.

## Exports

`export` marks items importable from other modules; non-exported
functions stay private to the module:

```ng
export fun visible() -> i64 { return helper(); }

fun helper() -> i64 {   // module-private
    return 7;
}
```

After `import priv_helper;`, calling `visible()` works; calling `helper()`
directly is rejected:

```
resolution error E-name-not-visible: name `helper` is not visible in this module; did you mean `helper`?
```

## The standard library

`lib/std/*.ng` is the stdlib, importable by name from any module
(the loader searches `lib/std` next to the importing file and upward):

| Module | Surface |
|---|---|
| `prelude` | `print` (i64/i8/i16/i32/u8/u16/u32/u64/isize/usize/string/bool/f64/f32 overloads), `assert`, `not`, `toFloat`, `toSigned`, `toString`, `canParseInt`, `parseRawInt`, `parseInt`, `Option`, `Result`, `optMap`/`optAndThen`/`optOrElse`, `resultMap`/`resultAndThen`/`resultOrElse`, traits `Copy`/`Clone`/`Eq`/`Show`, auto traits `Fn`/`FnOnce`, and re-exports of io/string |
| `io` | `readLine`, `readFile`, `writeFile`, `currentExecutablePath` |
| `system` | `system(command)`, `systemOutput(command)` — process/system command bindings (`import system;`) |
| `string` | `split`, `join`, `trim`, `contains`, `replace`, `startsWith`, `endsWith`, `toUpper`, `toLower`, `length`, `charAt`, `substring`, `regexMatch` |
| `seq` | `len`, `sum`, `arrayContains`, `reverse`, `pushBack`, `map`, `filter`, `fold` |
| `list` | recursive-enum `List<T>` with `length`/`get`/`contains`; builders `listFrom`/`pushFront`/`append`/`reverseList`/`pushBack` |
| `memory` | generic `Box<T>` (`new`/`box`/`read`/`write`), `Cell<T>` (`newCell`/`cellGet`/`cellSet`), `outstanding` |
| `imgui` | the Dear ImGui binding (see [ImGui Integration](/guide/imgui-integration)) |

`import prelude;` is the usual first line of a program. Modules such as `seq`
and `list` must be imported by name to use their functions.

Note that the prelude exports a trait named `Show`; a module that imports the
prelude cannot also declare its own `Show`.

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

fun main() -> i64 {
    let text = greeting();
    if (text != "hello from module") { return 0; }
    return 1;
}
```

```bash
./build/ngi example/modules/imports_main.ng
```

## Module interfaces

Each module's public surface is its **interface artifact** (D-017): exported
functions plus the type declarations it introduces. A selective import
(`import m (a, b);`) may only name something on that surface — importing a name
that is private to the module is rejected at the boundary.

`--emit=artifact` prints the per-module interface summary and verifies the
import boundary without running the program (it never changes the lowered code;
plain compilation and `--emit=ssa` output are identical with or without it):

```bash
./build/ngi --emit=artifact example/modules/imports_main.ng
```

Next: [Generics](/guide/generics).
