# LSP Server And IDE Support

## Order

Recommended implementation order: **3** (developer tooling dramatically impacts adoption).

## Goal

Deliver a Language Server Protocol (LSP) implementation for NG, enabling rich IDE features in VS Code, Vim/Neovim, Emacs, JetBrains, and any LSP-compatible editor.

## Motivation

Currently, NG has **no IDE support** beyond a basic built-in NG IDE built on ImGui. Developers cannot:
- Get syntax highlighting in VS Code, Vim, or any standard editor
- See compile errors inline while editing
- Autocomplete identifiers or imports
- Jump to definition or find references
- See type information on hover
- Rename symbols across files

## Proposed Components

### 1. Tree-Sitter Grammar

A tree-sitter grammar for NG enables syntax highlighting, code folding, and structural navigation in all tree-sitter-compatible editors.

```
ng/
├── tree-sitter-ng/
│   ├── grammar.js          -- tree-sitter grammar definition
│   ├── highlights.scm      -- syntax highlighting rules
│   ├── folds.scm           -- folding rules
│   ├── indents.scm         -- indentation rules
│   └── queries/            -- additional query files
```

Key parsing constructs to cover:
- `fun`, `val`, `type`, `trait`, `impl`, `module`, `import`, `export`
- Tagged union variants: `Ok(value) | Err(msg)`
- Generic parameters: `<T, U>`
- References: `ref<T>`, `move`, `*`
- Operators: `|>`, `..`, `..=`, `<<`, `:=`, `is`, `as`
- Const blocks: `const if`, `const fun`

### 2. LSP Server

An LSP server implemented in C++ (reusing the existing parser and type checker) that provides:

```
ng-lsp/
├── src/
│   ├── main.cpp            -- LSP server entry point
│   ├── documents.cpp       -- Document synchronization (open/change/close)
│   ├── completion.cpp      -- Autocompletion engine
│   ├── hover.cpp           -- Type information on hover
│   ├── goto.cpp            -- Go to definition / find references
│   ├── diagnostics.cpp     -- Real-time error reporting
│   ├── formatting.cpp      -- Document formatting (ng fmt integration)
│   └── workspace.cpp       -- Workspace symbol search
```

### 3. VS Code Extension

```json
{
  "name": "ng-language",
  "contributes": {
    "languages": [{
      "id": "ng",
      "extensions": [".ng"],
      "configuration": "./language-configuration.json"
    }],
    "grammars": [{
      "language": "ng",
      "scopeName": "source.ng",
      "path": "./syntaxes/ng.tmLanguage.json"
    }]
  }
}
```

### LSP Features (Priority Order)

| Priority | Feature | LSP Method |
|---|---|---|
| P0 | Syntax highlighting | tree-sitter / TextMate grammar |
| P0 | Compile error diagnostics | `textDocument/publishDiagnostics` |
| P1 | Go to definition | `textDocument/definitions` |
| P1 | Hover type info | `textDocument/hover` |
| P1 | Autocomplete | `textDocument/completion` |
| P2 | Find references | `textDocument/references` |
| P2 | Document symbols | `textDocument/documentSymbol` |
| P2 | Workspace symbols | `workspace/symbol` |
| P3 | Rename symbol | `textDocument/rename` |
| P3 | Code actions | `textDocument/codeAction` |
| P3 | Format document | `textDocument/formatting` |
| P4 | Inlay hints | `textDocument/inlayHint` |
| P4 | Signature help | `textDocument/signatureHelp` |

## Incremental Diagnostics API

The LSP server connects to the type checker through a `DocumentCache`:

```cpp
class DocumentCache {
public:
    // Open/change/close a document
    void openDocument(const std::string &uri, const std::string &source);
    void changeDocument(const std::string &uri, const std::string &newSource);
    void closeDocument(const std::string &uri);

    // Get diagnostics for a document (re-checks if changed)
    // Returns empty vector if no errors
    auto getDiagnostics(const std::string &uri) -> std::vector< Diagnostic>;

    // Get type information for a position
    auto getTypeAtPosition(const std::string &uri, Position pos) -> std::optional<TypeInfo>;

    // Get completion items at a position
    auto getCompletions(const std::string &uri, Position pos) -> std::vector<CompletionItem>;

    // Get definition location for symbol at position
    auto getDefinition(const std::string &uri, Position pos) -> std::optional<Location>;

private:
    struct CachedDocument {
        std::string source;
        ASTRef<CompileUnit> ast;
        bool isDirty = true;
        std::vector<Diagnostic> diagnostics;
    };

    HashMap<std::string, CachedDocument> documents;
    ModuleRegistry moduleRegistry;
    TypeEnvironment typeEnv;
};
```

### Diagnostics Flow

1. User opens file → Editor sends `textDocument/didOpen`
2. LSP calls `DocumentCache::openDocument(uri, text)`
3. `openDocument` parses the file, runs type checking, stores diagnostics
4. LSP sends `textDocument/publishDiagnostics` with results
5. User edits → Editor sends `textDocument/didChange`
6. LSP calls `DocumentCache::changeDocument(uri, newText)`
7. `changeDocument` marks the file as dirty, re-checks on next `getDiagnostics` call

### Completion Engine

Completions are generated by:
1. Finding all in-scope symbols at the cursor position
2. Filtering by the prefix the user has typed
3. Sorting by relevance (local > imported > stdlib)
4. Adding type information and documentation snippets

Completion source order:
- Local variables (highest priority)
- Function parameters
- Local function/type definitions
- Imported symbols
- Standard library symbols (lowest priority)

## Tree-Sitter Grammar Conformance

The tree-sitter grammar must pass a conformance test suite:

```bash
# In CI: generate AST from tree-sitter and compare with ng parser
tree-sitter-ng/test/corpus/
  ├── 01-expressions.txt
  ├── 02-functions.txt
  ├── 03-types.txt
  ├── 04-traits.txt
  ├── 05-modules.txt
  ├── 06-generics.txt
  ├── 07-control-flow.txt
  └── 08-const.txt
```

Each `.txt` file contains:
```
=== Function Definition ===
fun add(x: i32, y: i32) -> i32 {
    return x + y;
}
---
(compile_unit
  (function_definition
    name: (identifier)
    parameters: (parameter_list ... )
    body: (block ...)))
```

## Dependencies

- Requires stable parser API (already exists).
- Requires type checker that can answer "what is the type of this expression?" incrementally (see `DocumentCache` above).
- Diagnostic streaming requires the type checker to report errors per-file without aborting.
- Unblocks: code formatter, documentation generator.

## Scope

**In scope:**
- tree-sitter grammar covering the full language syntax
- LSP server with P0–P2 features
- VS Code extension
- Build integration (CMake target for ng-lsp)

**Out of scope:**
- Debug Adapter Protocol (DAP) — see [Debugger](gap-debugger.md)
- IntelliJ / JetBrains plugin (community effort after LSP stabilizes)
- Language server written in NG itself (future bootstrapping goal)

## Acceptance Criteria

- `ng-lsp` starts and responds to `initialize` handshake
- Opening an `.ng` file shows syntax highlighting
- Diagnostics appear on save for type errors
- Ctrl+click navigates to definition of functions and types
- Hovering shows function signatures and variable types
- Autocomplete suggests local variables and imported symbols
- Find references works within a file
- The server handles 100+ files without noticeable memory growth
- Build time for ng-lsp adds < 5 seconds to the overall build

## Potential Challenges

- The current type checker processes the full AST per-file, not incrementally — incremental rebuilding is needed for real-time diagnostics.
- tree-sitter grammar must be maintained in sync with the hand-written parser.
- C++ LSP server startup time must be < 1 second for a good UX.
- Module import resolution requires file system access — need a virtual file system layer for LSP.