# Build System — Dependency Graph & Incremental Compilation

> **Status:** Refined with algorithm details. INVEST: 5/5/4/4/4/4 after refinement.

## Dependency Graph Construction Algorithm

The build system constructs a **directed acyclic graph** (DAG) of module dependencies:

```
Input: Project source files (*.ng)
Output: Topologically sorted compilation order

Algorithm:
1. Parse all source files to extract `import` statements
2. Build a graph: file → [files it imports]
3. Resolve module paths (search NG_MODULE_PATH, dependency paths)
4. Detect cycles → error with cycle path
5. Topological sort → compilation order
6. Validate that all imported symbols resolve correctly
```

### Cycle Detection

```cpp
struct BuildGraph {
    struct Node {
        fs::path path;
        Vec<size_t> imports;    // Indices of imported modules
        bool visiting = false;  // For cycle detection
        bool visited = false;
    };
    
    Vec<Node> nodes;
    
    // Returns the compilation order (topological sort)
    auto resolveBuildOrder() -> std::expected<Vec<size_t>, CycleError>;
    
    struct CycleError {
        Vec<size_t> cycle;  // The nodes forming the cycle
    };
};
```

### Cache Invalidation

The build system maintains a **build cache** that tracks:

```toml
# .ngcache/module_hashes.toml
[hashes]
"src/main.ng" = "sha256:abc123..."
"src/utils.ng" = "sha256:def456..."
"lib/json.ng" = "sha256:789ghi..."
```

**Invalidation rules:**
1. If a source file's hash changes → recompile that file + all files that import it (transitively)
2. If a dependency's hash changes → same as #1
3. If the compiler version changes → full rebuild
4. If build flags change → full rebuild

**Hash file format**: SHA-256 of the normalized source content (LF line endings, no trailing whitespace).

### Incremental Build Algorithm

```
Input: Requested build target
Output: Minimum set of files to recompile

1. Load cached hashes (.ngcache/module_hashes.toml)
2. For each source file:
   a. Compute current hash
   b. If hash != cached hash → mark as dirty
3. For each dirty file, mark all its transitive dependents as dirty
4. Compile all dirty files in topological order
5. Update cache with new hashes

Optimization:
- Skip type checking if source hasn't changed (but dependencies may have)
- Use file modification time as a fast check, hash as accurate check
```

### Build Artifact Layout

```
build/
├── .ngcache/
│   ├── module_hashes.toml      # Source file hashes
│   └── dep_graph.json          # Serialized dependency graph
├── obj/
│   ├── main.ngo                # Compiled bytecode
│   ├── utils.ngo
│   └── json.ngo
└── output.ngo                  # Merged bytecode (for distribution)
```

### Commands

```bash
ng build                   # Build in debug mode
ng build --release         # Optimized build
ng build --clean           # Full rebuild (clear cache)
ng build --check           # Check for errors only (no output)
ng build --graph           # Print dependency graph (for debugging)
```

### `ng.toml` Schema

```toml
[project]
name = "my-app"
version = "0.1.0"
edition = "2026"

[build]
entry = "src/main.ng"
module-path = ["src", "lib"]
emit-bytecode = true
output = "build/output.ngo"

[profile.debug]
emit-debug-info = true

[profile.release]
optimize = true
strip-debug = true

[scripts]
serve = "ng run src/server.ng"
test = "ng run tests/run_all.ng"
```

### Acceptance Criteria

- `ng build` compiles all files in the correct order
- Changing one file only recompiles that file and its direct importers
- `ng build --clean` recompiles everything
- A circular import produces a clear error showing the cycle
- The build cache reduces second build time by >50%
- `ng build --release` produces optimized bytecode (if optimizer exists)

### Effort Estimate

| Component | Effort |
|---|---|
| Manifest parsing | 1 week |
| Import graph construction | 1 week |
| Topological sort + cycle detection | 0.5 week |
| Cache format + invalidation | 1 week |
| CLI interface | 1 week |
| Integration tests | 1 week |
| **Total** | **5.5 weeks** |