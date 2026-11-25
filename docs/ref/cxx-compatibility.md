# C++ Compatibility

The NG implementation builds with the C++23 standard.

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
- `native fun`s lower to `$`-prefixed symbols in the emitted IL, so hosts
  are added by implementing those C symbols rather than registering
  per-name callbacks. The `ngrt_*` spelling is the C side
  (`src/native/libngrt.c`); the `$` prefix is part of the QBE/IL symbol
  name. Observed call sites include `$ngrt_print_str`,
  `$ngrt_str_clone`, `$ngrt_canParseInt`, `$ngrt_alloc`,
  `$ngrt_trait_vtable`, and `$ngrt_imgui*` for the SDL3 GPU / Dear ImGui
  binding.

To see the exact symbols a program emits, dump the IL:

```bash
./build/ngi --emit=ssa /tmp/hello.ng
```

with `/tmp/hello.ng`:

```ng
import prelude;
fun main() -> unit {
    print("hi");
}
```

The emitted body shows the `$`-prefixed helper symbols directly:

```
export function l $main() {
@start
	%t2 =l copy $ngstr_0_0
	%t0 =l copy %t2
	%t3 =l call $ngrt_str_clone(l %t0)
	%t4 =l call $ngrt_print_str(l %t3)
	ret 0
}
data $ngstr_0_0 = { l 2, b "hi", b 0 }
```

Note that even the string literal and the entry point are `$` symbols
(`$ngstr_0_0`, `$main`); nothing in the IL uses a bare `ngrt_*` name.
