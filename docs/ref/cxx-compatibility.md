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

Host code integrates through `include/` (`driver.hpp`, `ngrt.hpp`,
`value.hpp`, `native/lowering.hpp`, ...):

- `NG::runDriver(arguments, output, errors)` drives the whole pipeline:
  load → resolve → typecheck → FlowIR → QBE IL → native executable (or
  `--emit=ssa` for the IL text).
- `libngrt` (`include/ngrt.h`) is the single C implementation of the
  standard native surface; it is linked into every generated executable and
  called by the frontend through the thin wrappers in `ngrt.hpp` for
  compile-time evaluation of const-capable natives.
- `native fun`s lower to `ngrt_*` symbols (and `$ngrt_imgui*` for the SDL3
  GPU / Dear ImGui binding), so hosts are added by implementing those C
  symbols rather than registering per-name callbacks.
