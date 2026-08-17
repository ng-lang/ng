# Getting Started with NG

NG compiles and runs through a single tool, `ngi`, which drives the full
pipeline (parse → resolve → typecheck → FlowIR → bytecode → VM) for a file
or an inline source unit.

## Requirements

- macOS or Linux with a modern C++ compiler (the build pins `clang`/`clang++`
  with libc++ on macOS; Homebrew LLVM provides `clang-tidy`/`clang-format`).
- CMake ≥ 3.25 and Ninja.

## Build

```bash
cmake -S . -B build -GNinja
cmake --build build -j
```

This produces:

- `build/ngi` — the NG frontend (headless).
- `build/ngi_imgui` — the same frontend plus the imgui binding.
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
hello, NG
compiled 26 vNext function(s); main returned after 10 instruction(s) with value 42
```

`main` may return nothing (`unit`), `i64`, `f64`, or `string`; command-line
arguments are passed to typed `main` parameters:

```ng
fun main(name: string) -> unit {
    print("hello, " + name);
}
```

```bash
./build/ngi hello.ng Ada
```

## Inline source

`ngi --source` compiles and runs a source unit directly, and `ngi --expr`
parses a single expression:

```bash
./build/ngi --source 'import prelude; fun main() { print(1 + 2); }'
./build/ngi --expr '2 * 3 + 4'
```

## Native compilation

NG can compile programs to native executables using the `--native` flag.
The `--output` flag specifies the output file path:

```bash
./build/ngi --native --output hello hello.ng   # compile to native executable
./build/ngi --native hello.ng                  # compile and run immediately
```

Native executables are self-contained and don't require the NG runtime at
execution time. See [Language Guide: Why NG?](/guide/language_guide#why-ng)
for binary size comparisons with other languages.

## Interactive programs

GUI/interactive programs run a frame loop; lift the per-run instruction
budget with `--fuel 0`:

```bash
./build/ngi_imgui example/ng_ide.ng --fuel 0
```

## Tests and examples

- `./build/ng_test` runs the whole suite (1700+ assertions across 430+
  cases), including a sweep that runs every `example/*.ng` end to end.
- `example/` holds one runnable example per feature area — a great way to
  learn by reading.

## Next steps

- [Basic Syntax](/guide/basic-syntax)
- [Control Flow](/guide/control-flow)
- [Functions](/guide/functions)
