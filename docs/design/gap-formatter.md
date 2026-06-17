# Code Formatter (`ng fmt`)

> **Status:** Refined. Approach chosen (hand-written AST pretty-printer), algorithm detailed. INVEST: 5/5/4/5/5/5 after refinement.

## Order

Recommended implementation order: **4** (paired with LSP).

## Goal

Provide an automatic code formatter for NG, ensuring consistent code style.

## Implementation Approach: Hand-Written AST Pretty-Printer

**Chosen approach:** Walk the existing C++ AST and emit formatted source.
**Rationale:**
- Reuses the existing, tested parser — no grammar synchronization problem
- AST preserves comments via associated tokens (already supported)
- Guaranteed to format valid NG (input was parsed successfully)
- Simpler to implement than tree-sitter based approach (~1,500 lines vs ~3,000)

**Not chosen:** Tree-sitter based approach. Revisit if/when tree-sitter grammar exists for other reasons (LSP highlighting).

## Project Structure

```
src/formatter/
├── Formatter.cpp          # Main entry point
├── Formatter.h            # Public API
├── PrettyPrinter.cpp      # AST visitor that emits formatted text
├── PrettyPrinter.h        # PrettyPrinter class declaration
└── Config.cpp/h           # Formatting configuration (from ngfmt.toml)
```

## Public API (`include/formatter.hpp`)

```cpp
namespace NG::formatter {

struct FormatConfig {
    int indentSize = 4;
    bool useSpaces = true;
    int maxLineWidth = 100;
    bool trailingComma = true;
    bool spacesAroundBinaryOps = true;
    bool newlineAtEndOfFile = true;
};

// Format source code. Returns formatted source or error message.
// Input must be valid NG source (parseable).
auto format(const std::string &source, const FormatConfig &config = {}) 
    -> std::expected<std::string, std::string>;

// Check only — returns true if already formatted.
auto check(const std::string &source, const FormatConfig &config = {}) -> bool;

} // namespace NG::formatter
```

## PrettyPrinter Design

The `PrettyPrinter` extends `AstVisitor` and walks the AST, emitting formatted text:

### Core Algorithm

```
class PrettyPrinter : public AstVisitor {
    std::ostringstream out;
    int indentLevel = 0;
    FormatConfig config;
    
    // State tracking
    bool atLineStart = true;
    int column = 0;
    Vec<int> breakOpportunities;  // positions where line breaks are allowed
    
    void emit(std::string_view text);
    void emitLineBreak();
    void emitIndent();
    void emitSpace();
    void emitWithLineBreakProtection(std::string_view text);
    
    // Smart line breaking
    bool wouldExceedMaxWidth(std::string_view addition);
    void breakLineIfNeeded();
};
```

### Formatting Rules (Per AST Node)

**CompileUnit:**
```
[module_decl]\n
[imports] (grouped, sorted)\n\n
[top_level_definitions] (separated by \n\n)
```

**FunctionDef:**
```
fun name(params) -> ReturnType {\n
    [indented body]\n
}
```
Single-expression shorthand:
```
fun name(params) -> ReturnType => expr;
```
Break long parameter lists:
```
fun name(
    param1: Type1,
    param2: Type2,
    param3: Type3,
) -> ReturnType { ... }
```

**TypeDef:**
```
type Name {\n
    [indented fields]\n
}
```
Tagged unions:
```
type Result<T, E> = Ok(value: T) | Err(error: E);
```
Break long variants:
```
type Result<T, E>
    = Ok(value: T)
    | Err(error: E);
```

**TraitDef:**
```
trait Name: SuperTrait1 + SuperTrait2 {
    fun method(self: ref<Self>) -> Type;
}
```

**ImplDef:**
```
impl Trait for Type {
    fun method(self: ref<Self>) -> Type {
        [body]
    }
}
```

**IfStatement:**
```
if (condition) {
    [then_body]
} else if (condition2) {
    [else_if_body]
} else {
    [else_body]
}
```

**LoopStatement:**
```
loop i = 0 {
    if (i < n) {
        next i + 1;
    }
}
```

**SwitchStatement:**
```
switch (expr) {
    case Variant1(field) {
        [body]
    }
    case Variant2 {
        [body]
    }
    otherwise {
        [body]
    }
}
```

**Match arm (expression):**
```
case Pattern => expr;
```

**BinaryExpression:**
```
a + b        // short: no spaces if simple
a + b + c    // long: break before operator
```
Line breaking rules:
- If the full expression fits on one line: keep it
- If not: break before operators at the outermost level

**FunCallExpression:**
```
func(arg1, arg2, arg3)               // short
func(                                 // long
    arg1,
    arg2,
    arg3,
)
```

**ArrayLiteral:**
```
[1, 2, 3]                            // short
[                                    // long
    1,
    2,
    3,
]
```

**TupleLiteral:**
```
(a, b, c)
(
    a,
    b,
    c,
)
```

**Generic parameters:**
```
fun name<T, U>(...) -> ...
fun name<
    T: Constraint1,
    U: Constraint2,
>(...) -> ...
```

### Comment Preservation

Comments are preserved by associating them with the nearest following AST node:

```
// This comment is attached to the next definition
fun foo() { ... }
```

- Line comments (`//`) stay at their current line
- Comments between functions stay between functions
- Comments inside function bodies stay at their indentation level

### Token-Level Whitespace

The formatter operates on the **token stream** from the lexer, reconstructing output from tokens rather than reconstructing from the AST alone. This preserves:
- String literal contents exactly
- Numeric literal formatting (`0xFF` stays `0xFF`)
- Comments exactly as written

## CLI Interface: `ng fmt`

```
Usage: ng fmt [OPTIONS] [FILES]...

Options:
  --check          Check formatting only (exit 1 if not formatted)
  --diff           Show diff of changes
  --stdin          Read from stdin, write to stdout
  --config FILE    Use config file
  -w, --write      Format in-place (default)
  -q, --quiet      Suppress output

Examples:
  ng fmt file.ng              # Format file.ng in-place
  ng fmt --check file.ng      # Check only
  ng fmt src/                 # Format all .ng in src/
  echo "val x=1" | ng fmt --stdin
```

## Config File (`ngfmt.toml`)

```toml
[format]
indent_style = "space"       # "space" | "tab"
indent_size = 4              # 2 | 4 | 8
max_line_width = 100
trailing_comma = true        # Add trailing commas in multiline
spaces_around_binary_ops = true
newline_at_end_of_file = true
sort_imports = false         # Enable import sorting (future)
```

## Acceptance Criteria

1. **Idempotence**: `ng fmt` on an already-formatted file produces identical output
2. **Valid output**: Formatted files are syntactically valid (parse succeeds)
3. **Preservation**: Comments, string contents, and numeric literals are unchanged
4. **Diffs**: Formatted diff is minimal (changing one line doesn't reformat the whole file)
5. **All examples**: All `example/*.ng` files pass `ng fmt --check` after initial formatting
6. **Config**: Changing `indent_size` from 4 to 2 produces correct output
7. `--check` exits 0 for formatted, 1 for unformatted
8. `--stdin` reads and formats correctly

## Implementation Plan

| Week | Deliverable |
|---|---|
| 1 | PrettyPrinter skeleton, handles basic nodes (Expression, Statement, CompileUnit) |
| 2 | Type/Function/Trait definitions, indentation, line breaking |
| 3 | Control flow (if/loop/switch), comments preservation |
| 4 | CLI tool, config file, --check, --diff modes |
| 5 | Edge cases, fuzz testing, all example files verified |

## Effort Estimate

| Component | Lines | Effort |
|---|---|---|
| PrettyPrinter core | ~800 | 2 weeks |
| CLI + Config | ~300 | 1 week |
| Testing + edge cases | ~500 | 2 weeks |
| **Total** | **~1,600** | **5 weeks** |

## Dependencies

- Stable AST (already exists).
- [LSP Server](gap-lsp-ide.md) can use `ng fmt` as formatting backend (future).
- No dependencies on other gap proposals.

## Potential Challenges

- **Comment placement**: Comments between AST nodes must be preserved in the correct position. Solution: the lexer already provides position information — comments attach to the nearest following token.
- **Line breaking heuristics**: Finding the optimal break point is NP-hard for general case. Use a simple greedy algorithm: break at the outermost operator that would exceed the line limit.
- **Malformed input**: The formatter requires valid NG source. Consider adding a "best effort" mode for partially written code (deferred).