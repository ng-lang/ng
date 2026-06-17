# Package Manager (`ngpkg`)

## Order

Recommended implementation order: **5** (needed once libraries exist to distribute).

## Goal

Design and implement a package manager for NG, enabling distribution, discovery, and dependency resolution of NG libraries.

## Motivation

Currently, NG has **no mechanism to distribute or consume third-party code**. All code must live in a single repository. There is no:
- Package registry or index
- Dependency declaration format
- Version resolution
- Lockfile for reproducible builds

## Proposed Design

### Manifest File: `ng.toml`

```toml
[package]
name = "my-project"
version = "0.1.0"
edition = "2026"
authors = ["Author Name"]

[dependencies]
std = ">=1.0"                       # built-in stdlib
json = { git = "https://github.com/user/json.ng", tag = "v1.2.0" }
http = { path = "../libs/http" }
regex = "0.2"                       # from default registry

[dev-dependencies]
test = "0.1"
```

### Commands

```bash
ng init                    # Create new ng.toml
ng add json                # Add dependency
ng remove json             # Remove dependency
ng build                   # Resolve deps, build project
ng run                     # Run main entry point
ng test                    # Run tests
ng publish                 # Publish to registry
ng update                  # Update dependencies to latest compatible
ng tree                    # Show dependency tree
```

### Registry

- Default registry URL (e.g., `https://pkg.ng-lang.org`)
- Simple HTTP API: `GET /api/v1/packages/{name}` returns metadata + tarball URL
- Packages are versioned using SemVer 2.0
- Authentication via API token (optional for publishing)

### Dependency Resolution

1. Read `ng.toml`
2. For each dependency, check the registry / git / path
3. Build a dependency graph
4. Resolve version constraints using SemVer compatibility
5. Produce a `ng.lock` file with pinned versions
6. Download and cache packages in `~/.ng/cache/`
7. Make packages available via `NG_MODULE_PATH`

### Integration with Module System

```ng
// A package's exports are accessed through import:
import json;

val data = json::parse(text);
```

The package manager maps package names to module paths automatically. When `json` is a dependency, `import json;` resolves to the installed package's entry module.

## Dependencies

- Requires module path resolution to support virtual paths (not just file system paths).
- [Standard Library Expansion](gap-stdlib-expansion.md) provides the HTTP client needed for registry access.
- Unblocks: large-scale project organization, CI workflows.

## Scope

**In scope:**
- `ng.toml` manifest format
- Dependency resolution with SemVer
- Git and path dependencies
- `ng.lock` lockfile
- Package cache in `~/.ng/cache/`
- Basic registry protocol (HTTP API)
- VM integration for module path resolution

**Out of scope:**
- Private registries / authentication (MVP uses public registry only)
- Workspaces / monorepo support
- Build scripts / custom build steps
- WASM distribution target
- Native (C++) package distribution

## Acceptance Criteria

- `ng init` creates a valid `ng.toml`
- `ng add json` installs the latest version and updates `ng.lock`
- `ng build` resolves all dependencies and runs the project
- Two projects with the same `ng.lock` produce identical dependency trees
- A published package can be installed by another project
- Offline builds work when all packages are cached
- Version conflicts produce clear error messages

## Potential Challenges

- Registry infrastructure (server, storage, moderation) is a significant operational cost.
- SemVer compliance relies on human discipline — automatic checking requires a stable API surface.
- Git dependencies are slow for large repositories — need shallow clone or sparse checkout.
- Dependency graph resolution is NP-hard in theory (though SemVer ranges keep it tractable).
- Security: no code signing or integrity verification in MVP.