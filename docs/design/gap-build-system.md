# Build System and Project Configuration

## Order

Recommended implementation order: **12**.

## Goal

Provide a native build system for NG projects, handling compilation, dependency resolution, asset management, and cross-platform builds without requiring CMake or external build tools.

## Motivation

Currently, NG projects have **no build configuration file**. Developers must:
- Use CMake to build the interpreter itself
- Manually set `NG_MODULE_PATH` for multi-file projects
- Have no way to specify project metadata, entry points, or build flags
- Have no way to define scripts or custom build steps

This makes NG unsuitable for anything beyond single-file scripts and examples.

## Proposed Design

### Project Manifest: `ng.toml`

```toml
[project]
name = "my-app"
version = "0.1.0"
edition = "2026"

[build]
entry = "src/main.ng"
module-path = ["src", "lib"]
emit-bytecode = true

[profile.release]
optimize = true          # Enable VM optimizations
strip-debug = true        # Remove debug symbols from bytecode

[profile.debug]
optimize = false
debug-info = true

[scripts]
serve = "ng run src/server.ng"
test = "ng run tests/run_all.ng"

[dependencies]
json = { git = "https://...", tag = "v1.0" }
```

### `ng build` Command

```bash
ng build                   # Build in debug mode
ng build --release         # Build with optimizations
ng build --target wasm     # Cross-compile to WASM (future)
ng build --emit-ngo        # Emit bytecode artifact
ng build --emit-binary     # Emit standalone binary (with embedded VM)
```

### `ng run` Command

```bash
ng run                     # Run the project's entry point
ng run src/server.ng       # Run a specific file
ng run --release           # Run optimized build
```

### Multi-Target Builds

```toml
[build.targets]
linux-x64 = { type = "binary", entry = "src/main.ng" }
wasm = { type = "wasm", entry = "src/main.ng" }
```

### Build Profiles

| Profile | Optimization | Debug Info | Use Case |
|---|---|---|---|
| `debug` | None | Full | Development |
| `release` | Full | None | Production |
| `minimal` | Size | None | Embedded/WASM |
| `test` | None | Full + coverage | Testing |

### Build Artifacts

- `.ngo` bytecode modules (one per source file)
- `.ngb` build cache (avoid recompiling unchanged files)
- Optional: standalone executable embedding the VM + bytecode
- Build ID and source hash for cache invalidation

## Dependencies

- Requires module path resolution (already exists).
- Requires [Package Manager](gap-package-manager.md) for dependency integration.
- Unblocks: project scaffolding, CI pipelines, multi-file application development.

## Scope

**In scope:**
- `ng.toml` manifest format
- `ng build`, `ng run` commands
- Debug/release build profiles
- Incremental compilation (only recompile changed files)
- Bytecode cache
- Module path resolution from manifest
- Cross-platform build (Unix, macOS, Windows)

**Out of scope:**
- WASM target (future — depends on WASM runtime)
- AOT native compilation (see [Runtime & Native Compilation](gap-runtime-optimization.md))
- IDE integration (deferred to LSP)
- Package publishing (deferred to package manager)

## Acceptance Criteria

- `ng build` compiles all project source files to bytecode
- `ng run` executes the entry point
- Incremental build: touching one file only recompiles that file and its dependents
- `--release` mode produces optimized bytecode
- Project with dependencies loads correctly from `ng.toml` module paths
- Cross-platform: same project builds on Linux, macOS, and Windows
- Build cache reduces second-build time by >50%

## Potential Challenges

- Dependency graph construction must handle circular imports gracefully.
- Incremental compilation requires tracking source file hashes and module dependencies.
- Build cache invalidation is tricky: a dependency change must invalidate all transitive dependents.
- Cross-platform paths: Windows uses `\`, Unix uses `/` — must be handled consistently.
- Standalone binary embedding requires linking the VM as a library.