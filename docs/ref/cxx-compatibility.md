# C++ Compatibility

The NG implementation uses a modern C++ toolchain.

## Toolchain

- **macOS**: the build pins `clang`/`clang++` with libc++
  (`-stdlib=libc++`), and emits an explicit `-isysroot` so tooling
  resolves the SDK headers.
- **CI** (`RUNNING_ON_GITHUB=1`): the platform compiler with the same
  baseline.

## Tooling

`clang-tidy` and `clang-format` come from the Homebrew LLVM suite at
`/opt/homebrew/opt/llvm` (symlinked into `/opt/homebrew/bin`):

```bash
clang-format -i src/**/*.cpp include/**/*.hpp
clang-tidy -p build src/<file>.cpp   # uses build/compile_commands.json
```

The build exports `build/compile_commands.json`
(`CMAKE_EXPORT_COMPILE_COMMANDS`), and `.clang-tidy` excludes
`modernize-use-trailing-return-type` (an LLVM 21 regression over libc++
macros).

## Embedding boundary

Host code integrates through `include/` (`driver.hpp`, `native.hpp`,
`value.hpp`, `bytecode.hpp`, ...):

- `NG::runDriver(arguments, output, errors)` drives the whole pipeline.
- `NG::runDriverWithNatives(..., NG::registerImguiNatives)` adds extra
  native registrations (the `ngi_imgui` frontend does this).
- `vm::NativeRegistry` registers hosts by name; hosts receive
  deep-copied `Value`s plus static parameter types and return `Value`s.
- The driver registers the const-capable host set for compile-time
  evaluation of pure natives.
