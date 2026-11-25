# Getting Started with NG

NG compiles and runs through a single tool, `ngi`, which drives the full
pipeline (parse → resolve → typecheck → FlowIR → QBE IL → native executable)
for a file or an inline source unit.

## Requirements

- macOS or Linux with a C++23 compiler (the build pins `clang`/`clang++`
  with libc++ on macOS; Homebrew LLVM provides `clang-tidy`/`clang-format`).
- CMake ≥ 3.25 and Ninja.

## Build

```bash
cmake -S . -B build -GNinja
cmake --build build -j
```

This produces:

- `build/ngi` — the NG frontend.
- `build/ng_test` — the full Catch2 test suite (run `ctest --test-dir build`).

## Run your first program

Write `hello.ng`:

```ng
import prelude;

fun main() -> i64 {
    print("hello, NG");
    return 42;
}
```

Run it:

```bash
./build/ngi hello.ng
```

Output:

```text
compiled 49 vNext function(s)
hello, NG
native main exited with code 42
```

Every run compiles to a native executable through QBE and `cc`; there is no
separate interpreter or bytecode tier.

`main` may return nothing (`unit`), `i64`, `f64`, or `string`; string and
float returns are printed to stdout, and an integer return becomes the
process exit code reported by the driver.

## Inline source

`ngi --source` compiles and runs a source unit directly:

```bash
./build/ngi --source 'import prelude; fun main() { print(1 + 2); }'
```

```text
compiled 49 vNext function(s)
3
native main exited with code 0
```

`ngi --expr` parses a single expression. It reports the parsed span and does
not evaluate or print a value:

```bash
./build/ngi --expr '2 * 3 + 4'
```

```text
parsed vNext expression at bytes [0, 9)
```

## Compiling to an executable

`--output <path>` compiles to a native executable at `<path>` and stops
without running it. Executables link the NG runtime library (`libngrt`) and
run on their own:

```bash
./build/ngi --output hello hello.ng
```

```text
compiled 3 vNext function(s)
native executable written to hello
```

`--emit=ssa` lowers the module to QBE IL text instead of building and running
it:

```bash
./build/ngi --emit=ssa hello.ng
```

The full set of flags is:

```text
ngi --expr <expression>
ngi --source <source-unit>
ngi <source-file> [--emit=ssa] [--output <path>]
```

## Interactive programs

GUI/interactive programs run a frame loop (see
[ImGui Integration](/guide/imgui-integration)):

```bash
./build/ngi example/ng_ide.ng
```

## Tests and examples

- `./build/ng_test` runs the whole suite (4139 assertions across 662 test
  cases), including a sweep that runs every `example/*.ng` end to end.
- `example/` holds one runnable example per feature area — a great way to
  learn by reading.

## Next steps

- [Basic Syntax](/guide/basic-syntax)
- [Control Flow](/guide/control-flow)
- [Functions](/guide/functions)
