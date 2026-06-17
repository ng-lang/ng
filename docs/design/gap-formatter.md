# Code Formatter (`ng fmt`)

## Order

Recommended implementation order: **4** (pairs with LSP for IDE integration).

## Goal

Provide an automatic code formatter for NG, ensuring consistent code style across projects without manual effort.

## Motivation

NG currently has **no code formatting tool**. The project's `AGENTS.md` mentions naming conventions but there's no way to automatically enforce them. This leads to:
- Inconsistent formatting in pull requests
- Developer time wasted on style discussions
- No easy path to enforce a style guide in CI

## Proposed Design

### Binary: `ng fmt`

```bash
ng fmt file.ng                  # Format file in-place
ng fmt --check file.ng          # Check only, exit 1 if not formatted
ng fmt --diff file.ng           # Show diff of changes
ng fmt src/                     # Format all .ng files in directory
ng fmt --stdin < file.ng        # Read from stdin, write to stdout
```

### Configuration File: `ngfmt.toml`

```toml
[format]
indent_style = "space"          # space | tab
indent_size = 4                 # 2 | 4 | 8
max_line_width = 100
trailing_comma = true           # Add trailing commas in tuples/objects
spaces_around_binary_ops = true
newline_at_end_of_file = true
```

### Formatting Rules (Initial)

| Rule | Description |
|---|---|
| Indentation | Consistent indentation for blocks, `case` branches, continuation lines |
| Spacing | Spaces around binary operators, after commas, inside braces |
| Line breaks | Braces on same line (OTBS / K&R style), blank lines between top-level decls |
| `fun` params | Break long parameter lists across lines, align types |
| `switch` cases | Indent `case` under `switch`, align `case` bodies |
| Imports | Group stdlib imports first, then local imports, sorted alphabetically |
| Trailing commas | Add after last element in multiline arrays, tuples, objects |
| Semicolons | Ensure all statements end with `;` |
| Redundant whitespace | Remove trailing whitespace, extraneous blank lines |

### Implementation Approach

Two strategies (choose one or combine):

#### A. Tree-Sitter Based (recommended)

- Reuse the [tree-sitter grammar](gap-lsp-ide.md) from the LSP proposal
- Implement formatting as AST traversal with pretty-printing rules
- More resilient to syntax errors (can format partial files)

#### B. Hand-Written Pretty-Printer

- Walk the existing C++ AST
- Output formatted source from the parsed AST
- Guaranteed consistency with the compiler's AST representation

### Minimum Viable Formatting

```ng
// Before (hand-written, inconsistent):
val   x=1; fun add(a:i32,b:i32)->i32{return a+b;}

// After (ng fmt):
val x = 1;
fun add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

## Dependencies

- Requires tree-sitter grammar OR stable AST representation.
- [LSP Server](gap-lsp-ide.md) can integrate `ng fmt` as the formatting backend.
- Unblocks: pre-commit hooks and CI formatting checks.

## Scope

**In scope:**
- `ng fmt` binary as a new CMake target
- Formatting all syntax constructs (functions, types, imports, loops, switch, etc.)
- `--check`, `--diff` modes
- Configuration file support
- CI integration guide

**Out of scope:**
- Auto-fixing of unused imports or variables (code actions — see LSP proposal)
- Import sorting and organization (future enhancement)
- Refactoring tools (rename, extract function — see LSP proposal)

## Acceptance Criteria

- `ng fmt` produces idempotent output (formatting an already-formatted file is a no-op)
- All `example/*.ng` files are idempotent after formatting
- `ng fmt --check` exits 0 for formatted files, 1 for unformatted
- A pre-commit hook template is included in the repository
- Formatter handles edge cases: empty files, deeply nested generics, complex tagged unions
- Formatter preserves comments

## Potential Challenges

- NG syntax is complex (tagged unions, generics, const blocks, HKT) — every construct must be handled.
- Comments must be preserved and re-associated with the correct AST nodes.
- Handling malformed input gracefully while formatting the valid parts.
- TOML config parser needs to be added as a dependency or implemented inline.