

# AGENTS.md

## Project Overview

**NG** is a statically-typed, multi-paradigm programming language implemented in modern C++ (C++23). The codebase is structured as a classic compiler pipeline:

**Lexer → Parser → AST → Type Checker → Interpreter**

**Key directories and file patterns:**
- `src/ast/` — AST nodes and visitors (`ast.cpp`, `AstVisitor.cpp`)
- `src/parsing/` — Lexer, parser, reserved tokens (`Lexer.cpp`, `ParserImpl.cpp`, `reserved.inc`)
- `src/runtime/` — StorageCell/slot-based NG runtime values and layout helpers (`NGArray.cpp`, `runtime_env.cpp`, `NGString.cpp`, `NGTuple.cpp`)
- `src/typecheck/` — Type info and checker (`PrimitiveType.cpp`, `FunctionType.cpp`, `typecheck.cpp`)
- `src/module/` — Module loading/registry; `src/stdlib/` — built-ins (e.g., `prelude.cpp`, `imgui.cpp`)
- `src/orgasm/` — ORGASM bytecode compiler and VM (`Compiler.cpp`, `VM.cpp`, `module.cpp`)
- `src/main.cpp` — Builds the `ngi` interpreter
- `include/` — Public headers mirror modules (e.g., `ast.hpp`, `parser.hpp`, `token.hpp`, `visitor.hpp`)
- `include/orgasm/` — ORGASM bytecode headers (`opcode.hpp`, `module.hpp`, `compiler.hpp`, `vm.hpp`, `native_bridge.hpp`)
- `example/*.ng` — Runnable language examples (e.g., `14.tuple.ng`)
- `test/` — Catch2 v3 tests grouped by `parsing/`, `runtime/`, `typecheck/`, `orgasm/` + helpers (`test.hpp`)
- `lib/` — Standard library in NG
- `docs/` — Language and internals documentation


## Architecture & Patterns
- **Lexer:** `src/parsing/Lexer.cpp` (`LexState` struct) — tokenizes source code
- **Parser:** `src/parsing/ParserImpl.cpp` (`ParserImpl` class, recursive descent) — builds AST nodes (`include/ast.hpp`)
- **AST:** Visitor pattern (`AstVisitor`), base class `ASTNode` (`include/ast.hpp`, `src/ast/`)
- **Type Checking:** `src/typecheck/` — traverses AST for type inference/validation
- **Interpreter:** `src/intp/` — executes AST directly (see `Interpreter` class)
- **ORGASM:** `src/orgasm/` — bytecode backend for typed AST
  - **Compiler:** `src/orgasm/Compiler.cpp` — lowers checked AST into `BytecodeModule`
  - **VM:** `src/orgasm/VM.cpp` — executes StorageCell-based bytecode
  - **Module model:** `include/orgasm/module.hpp` + `src/orgasm/module.cpp` — bytecode functions, types, imports, exports, and merge/remap logic
  - **Opcode set:** `include/orgasm/opcode.hpp` — the single active ORGASM opcode enum
- **Modules:** Each `.ng` file is a module. Use `export`/`import` for visibility (see `docs/guide/language_guide.md`)
- **Standard Library:** Minimal, in `lib/std.ng` and `lib/std/`
- **Native functions:** NG supports native (C++) functions via `= native;` in NG code. Register with the interpreter (`register_native_library`).
- **Memory management:** Runtime values are `StorageCell` slots managed with `std::shared_ptr`; heap values are cloned into managed heap cells and traced through slot references, `RuntimeEnv`, and call-frame roots.


## Build, Test, and Development Workflows
**Fetch dependencies:**

```bash
git submodule update --init --recursive
```
**Configure & build (Ninja):**

```bash
cmake -S . -B build -GNinja
cmake --build build -j
```
**Run tests (CTest/Catch2):**

```bash
ctest --test-dir build -j
./build/ng_test --list-tests
./build/ng_test "parser*"   # example filter
```
**Run interpreter:**

```bash
./build/ngi example/14.tuple.ng
```
**Coverage report:**

```bash
./utils/coverage_report.sh
# Output in build/reports/cov/
```
**Format and lint C++ code:**

```bash
clang-format -i src/**/*.cpp include/**/*.hpp
clang-tidy -p build src/<file>.cpp
```


## Coding Style & Naming Conventions
- **C++23**; prefer RAII, `const` correctness, and explicit ownership
- **Types/classes:** Use `PascalCase` (runtime often prefixed `NG*`)
- **Headers/sources:** Use `snake_case` (`token.hpp`, `typeinfo.cpp`)
- **Tests:** `test/<area>/<topic>_test.cpp`
- **NG code style:** Follow examples in `example/`
- **External dependencies:** Vendored in `build/_deps/` and `vendored/` (e.g., Catch2, SDL, imgui)
- **Do not modify** files in `vendored/` or `build/_deps/`
- **AI-generated code** must be marked as such (see `CONTRIBUTING.md`)


## Testing Guidelines
- **Framework:** Catch2 v3 (vendored). Include shared helpers from `test/test.hpp`
- **Add focused unit tests** near their domain; keep deterministic and isolated from FS/network
- **Test patterns:** All tests use Catch2; test files are in `test/` and follow the pattern `REQUIRE(...)`
- **CI reports coverage** (Codecov/Codacy). Maintain or improve coverage for modified areas

## Commit & Pull Request Guidelines
- **Prefer Conventional Commits** like history: `feat(tuple): ...`, `fix(parsing): ...`, `chore: ...`
- **Messages in imperative mood**; link issues (`resolve #14`)
- **PRs must include:** clear description, linked issues, tests, docs/examples updates where relevant (e.g., `docs/`, `example/`). Add screenshots for ImGui/UI changes
- **Disclose any AI‑generated code** per `CONTRIBUTING.md`

## Security & Configuration Tips
- **Local builds** default to `clang++` with `libc++`; CI toggled via `RUNNING_ON_GITHUB=1`
- **macOS/CI** enable coverage flags via CMake; no action needed unless reproducing CI locally

## References
- [Language Guide](../docs/guide/language_guide.md)
- [Internals](../docs/ref/Internals.md)
- [Contribution Guide](../CONTRIBUTING.md)
- [README](../README.md)

---
For any unclear conventions or missing documentation, consult the above references or ask in project discussions.
