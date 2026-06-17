# Documentation Generator — Lexer Changes & Doc Test Engine

> **Status:** Refined with implementation details. INVEST: 5/5/4/4/5/4 after refinement.

## Lexer Changes for `///` Doc Comments

### Token Type

```cpp
// In include/token.hpp
enum class TokenType : uint32_t {
    // ... existing ...
    DOC_COMMENT = 0x0C00,  // /// doc comment
};
```

### Lexer Implementation

```cpp
// In src/parsing/Lexer.cpp

if (state.source[state.pos] == '/' && state.source[state.pos + 1] == '/') {
    if (state.source[state.pos + 2] == '/') {
        // Doc comment: ///
        return lexDocComment(state);
    } else {
        // Regular comment: // → skip (existing behavior)
        return skipLineComment(state);
    }
}

Token lexDocComment(LexState &state) {
    state.pos += 3;  // skip ///
    
    // Optional space after ///
    if (state.source[state.pos] == ' ') {
        state.pos++;
    }
    
    // Lex until end of line
    auto start = state.pos;
    while (state.pos < state.source.size() && state.source[state.pos] != '\n') {
        state.pos++;
    }
    
    auto text = state.source.substr(start, state.pos - start);
    return Token(TokenType::DOC_COMMENT, text, state.line, state.col);
}
```

### AST Association

Doc comments are associated with the AST node that follows them:

```cpp
// In ParserImpl.cpp, when parsing top-level definitions:
if (peek().type == TokenType::DOC_COMMENT) {
    currentDocComment = consume().repr;
    // The next definition (fun/type/trait/val) gets this doc comment
}

// In each definition's AST node:
struct FunctionDef : Definition {
    std::string docComment;  // Associated doc comment (empty if none)
    // ... existing fields ...
};
```

## Cross-Reference Resolution

### Algorithm

```
Input: Doc comment text with [TypeName] or `functionName` references
Output: Resolved HTML links

1. Collect all exported symbols from the module
2. For each `[Ident]` in the doc comment:
   a. Look up Ident in the module's exported symbols
   b. If found → emit <a href="#symbol-ident">Ident</a>
   c. If not found → check imported modules
   d. If found in imported module → emit <a href="module.html#symbol-ident">Ident</a>
   e. If not found → emit Ident as plain text (no warning, silent fallback)
```

### Search Index

The doc generator builds a search index as JSON:

```json
{
  "entries": [
    {"name": "readFile", "type": "function", "module": "std.io", "url": "std/io.html#readFile"},
    {"name": "HashMap", "type": "type", "module": "std.collections", "url": "std/collections.html#HashMap"},
    ...
  ]
}
```

## Doc Test Execution

Doc tests are code blocks in doc comments that are compiled and executed:

```ng
/// # Examples
/// ```
/// val x = add(2, 3);
/// assert(x == 5);
/// ```
fun add(a: i32, b: i32) -> i32 => a + b;
```

### Execution Mechanism

```cpp
class DocTestRunner {
public:
    auto runTests(const std::string &moduleName, const std::vector<DocTest> &tests) -> TestResults;
    
private:
    // For each doc code block:
    // 1. Extract the code (between ``` and ```)
    // 2. Compile it in a sandbox that imports the documented module
    // 3. Execute and check for errors
    // 4. Record pass/fail
    
    struct DocTest {
        std::string code;
        std::string moduleName;
        uint32_t line;
    };
};
```

### Sandboxing

Doc tests execute in a restricted environment:
- No file system access (readFile/writeFile disabled)
- No module import outside the documented module
- Configurable timeout (default: 5 seconds)
- No mutable global state beyond the test scope

## Output Format

### HTML Template (simplified)

```html
<!DOCTYPE html>
<html>
<head>
    <title>Module: {{module_name}} — NG Documentation</title>
    <link rel="stylesheet" href="ng-doc.css">
</head>
<body>
    <nav class="sidebar">
        <h2>Modules</h2>
        <ul>
        {{#each modules}}
            <li><a href="{{name}}.html">{{name}}</a></li>
        {{/each}}
        </ul>
    </nav>
    <main>
        <h1>Module: {{module_name}}</h1>
        <section class="functions">
            <h2>Functions</h2>
            {{#each functions}}
            <article id="{{name}}">
                <h3><code>{{signature}}</code></h3>
                <div class="doc">{{{docHtml}}}</div>
            </article>
            {{/each}}
        </section>
    </main>
    <script src="search.js"></script>
</body>
</html>
```

### CSS Styling (ng-doc.css)

Dark/light theme via CSS variables. Responsive layout with collapsible sidebar.

## CLI Interface: `ng doc`

```bash
ng doc                                # Generate HTML docs for current project
ng doc --output docs/                 # Specify output directory
ng doc --format markdown              # Output Markdown instead of HTML
ng doc --serve                        # Start HTTP server at localhost:8080
ng doc --check                        # Check that all public items have docs
ng doc --test                         # Run doc tests
ng doc --no-deps                      # Only document the current project, not deps
```

## Effort Estimate

| Component | Effort |
|---|---|
| Lexer changes for `///` | 0.5 day |
| AST association (store doc comments) | 0.5 day |
| HTML template rendering | 2 weeks |
| Markdown->HTML rendering library integration | 1 week |
| Cross-reference resolution | 1 week |
| Search index | 0.5 week |
| Doc test extraction + execution | 2 weeks |
| `ng doc --serve` (HTTP server) | 0.5 week |
| CSS + theme | 1 week |
| Tests | 1 week |
| **Total** | **~9 weeks** |