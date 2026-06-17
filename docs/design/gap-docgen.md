# Documentation Generator (`ng doc`)

## Order

Recommended implementation order: **9**.

## Goal

Provide a documentation generator for NG, producing HTML/Markdown documentation from source code with doc comments.

## Motivation

NG has **no way to generate documentation** from source code. Developers must manually write and maintain separate documentation files. There is no:
- Document comment syntax (`///` or `/** */`)
- Documentation generator tool
- Documentation hosting workflow

## Proposed Design

### Doc Comment Syntax

```ng
/// A 2D point in screen space.
///
/// # Examples
/// ```
/// val p = Point { x: 10, y: 20 };
/// print(p.distanceToOrigin());
/// ```
type Point: derive(Copy) {
    /// The x-coordinate.
    x: i32;
    /// The y-coordinate.
    y: i32;
}

impl Point {
    /// Calculate the distance from origin.
    ///
    /// Uses the Pythagorean theorem: `sqrt(x² + y²)`.
    /// Returns `f64` for precision.
    fun distanceToOrigin(self: ref<Self>) -> f64 {
        return sqrt((self.x * self.x + self.y * self.y) as f64);
    }
}
```

### Markdown Support

Doc comments support Markdown with extensions:
- `# Examples` sections with code blocks
- `[link](target)` cross-references to other docs
- `inline code` formatting
- Lists, tables, headings
- `type`, `fun`, `trait` names automatically link to their documentation

### `ng doc` Command

```bash
ng doc                          # Generate HTML docs for current project
ng doc --output docs/           # Specify output directory
ng doc --format markdown        # Output Markdown instead of HTML
ng doc --serve                  # Start HTTP server for live preview
ng doc --check                  # Verify all public items have docs
```

### Generated Output

HTML pages include:
- Module index with all exported items
- Per-type documentation with methods
- Per-function documentation with signature and examples
- Search functionality
- Source code links
- Dark/light theme toggle

### Documentation Tests

Code blocks in doc comments are automatically tested:

```bash
ng doc --test                   # Run all doc tests
```

This ensures examples in documentation stay correct as the code evolves.

## Dependencies

- Requires stable tokenizer for comment parsing.
- Requires module-level export resolution to know which items to document.
- Unblocks: project onboarding, API reference.

## Scope

**In scope:**
- `///` doc comment syntax
- Markdown rendering
- HTML and Markdown output formats
- Cross-referencing types and functions by name
- Module index pages
- Search functionality
- `ng doc --serve` for live preview
- `ng doc --check` for missing docs
- Documentation tests (`--test`)
- Style: dark/light theme

**Out of scope:**
- `/** */` block doc comments (use only `///` for consistency)
- Private item documentation
- C++ native function documentation from C++ source
- Automated API change detection (future)
- Integrated hosting platform

## Acceptance Criteria

- `ng doc` generates a valid HTML site from any project
- `///` comments appear in the generated output
- Code blocks in docs render with syntax highlighting
- Clicking a type name navigates to its documentation
- `ng doc --serve` opens a browser with live documentation
- `ng doc --check` reports undocumented public items
- Doc tests run and pass
- All `example/*.ng` files can be documented

## Potential Challenges

- Markdown rendering is non-trivial — consider using an existing C/C++ Markdown library (e.g., MD4C, cmark).
- Cross-module references require resolving the module graph.
- Search requires indexing all documented items — could use a simple client-side JavaScript implementation.
- Documentation tests need evaluation in the VM — must be sandboxed to avoid side effects.
- Maintaining doc comment position accuracy through lexical analysis and AST transformations.