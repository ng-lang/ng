# NG Internals

The NG implementation is a single clean pipeline:

**Lexer → Parser (syntax AST) → Resolver (HIR) → Type Checker → FlowIR →
Bytecode → VM**

All source lives in `src/` with public headers in `include/`; the
`ngi`/`ngi_imgui` frontends drive the pipeline from `src/driver.cpp`.

## 1. Syntax — `src/syntax/`

- `parser.cpp` lexes the full source into spanned tokens (strings are
  unescaped at lex time, numeric suffixes preserved) and parses
  expressions; `module_parser.cpp` handles module items (`fun`, `struct`,
  `enum`, `trait`, `impl`, `const`, `type`, `import`, `export`, `native`);
  `block_parser.cpp` handles statements; `type_parser.cpp` handles type
  syntax; `const_expr.cpp` handles const-expression parsing.
- The syntax AST (`include/syntax/ast.hpp`) is immutable and carries
  source spans everywhere; semantic facts never live on syntax nodes.

## 2. Resolver — `src/hir.cpp`

`hir::Resolver` turns a syntax unit into an immutable `hir::Module`:
functions, structs, enums, traits, impls, const declarations, and opaque
types, with local ids and name scopes. Module loading
(`src/module_loader.cpp`) walks the transitive import graph, computes
per-module visible-name sets (exports, selective imports, transitive
re-export), and resolves `lib/std` imports.

## 3. Type checker — `src/typecheck.cpp` + `src/type_interner.cpp`

`typecheck::TypeChecker{}.check(module)` returns a `TypeCheckResult` of
side tables keyed by HIR node pointers — no AST mutation:

- expression types / expected types (`infer` / `inferExpected`)
- call targets, fold/spread positions, trait-view calls and coercions
- monomorphized instances (generic functions cloned, renumbered, and
  re-checked per concrete argument set, deduplicated by key)
- ownership: affine move/clone tracking, field-aware partial moves, drop
  edges, and borrow loans with non-lexical release
- trait/impl resolution, generic impl pattern matching, view tables
- const evaluation via the const interpreter and const-capable hosts

Types are interned `TypeId`s over descriptors (`TypeInterner`), which also
handles two-phase recursive enum instantiation and type constructors.

## 4. FlowIR — `src/flowir.cpp`

The lowerer produces per-function CFGs with block parameters:
fold/map-comprehension loops, drop calls on scope exits, trait-view
construction/dispatch, switch lowering (variant dispatch and literal
equality chains), and array spreads/appends. `flowir::Verifier` checks
each block (terminators, operand/result types).

## 5. Bytecode — `src/bytecode.cpp` + `src/bytecode/artifact.cpp`

`bytecode::ModuleCompiler` lowers verified FlowIR into a `Module` of
functions with local type metadata; one opcode descriptor table drives
encoding, decoding, verification, and disassembly. `ArtifactCodec`
serializes modules (versioned magic, string constants, type descriptors,
vtables) to bytes and back.

## 6. VM — `src/vm.cpp` + `src/vm/value_ops.cpp`

The interpreter executes decoded bytecode with a per-run instruction
budget (`--fuel 0` lifts it). Values (`NG::Value`) are
int64/double/string/array/tuple/struct/enum/reference/trait-view/opaque/
range variants; aggregate copies are deep. Native dispatch looks up
registered hosts by function name; `runNgi` re-enters the whole pipeline
from a running program.

## 7. Driver and hosts — `src/driver.cpp`

The driver parses CLI arguments (`--expr`, `--source`, file mode,
`--fuel`), loads/compiles the module, registers the core natives
(string/io/seq/memory, `runNgi`, `regexMatch`), and runs `main`. The
const-capable host set (pure string ops) is registered for compile-time
evaluation. `ngi_imgui` adds the imgui binding
(`src/imgui_natives.cpp`, SDL3 GPU backend).

## 8. Testing

`test/*.cpp` are Catch2 suites per feature area; `test/examples_sweep_test.cpp`
runs every example end to end. `./build/ng_test` must be green before
each commit.
