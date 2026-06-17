# Gap Proposals — Review Log

This document contains the review for each of the 15 gap proposals submitted to `docs/design/`.

---

## 1. gap-error-handling.md — Error Handling: Result, ?, try/catch

**Rating: ✅ Accept (with minor revisions)**

**Strengths:**
- Clear motivation — the absence of error handling is indeed the #1 blocker.
- Good split between language features (?, try/catch, throw) and type-level (Result<T,E>).
- Migration plan for existing stdlib functions is mentioned.

**Issues:**
- The `throw` statement and `try/catch` are somewhat redundant with `Result` + `?`. Rust proves you can build everything with `Result` alone. Recommend deferring `try/catch` to phase 2.
- No mention of interaction with `const if` / compile-time evaluation — `?` in const functions must be rejected at compile time.

**Recommendation:** Accept. Start with `Result<T,E>` and `?` operator only; add `try/catch` after real-world usage shows the need.

---

## 2. gap-stdlib-expansion.md — Standard Library Expansion

**Rating: ✅ Accept**

**Strengths:**
- Well-organized into tiers with clear priorities.
- Each module is scoped with specific API surface.
- Dependencies on other proposals are clearly noted.

**Issues:**
- `HashMap` needs a `Hash` trait — this is a significant type system addition not discussed. Recommend defining `Hash` as a companion trait to `Eq` in the prelude.
- `std.test` as a standard library module vs built-in syntax — the proposal treats it as library, which is the right call.
- No mention of `std.json` performance expectations (streaming vs DOM parsing).

**Recommendation:** Accept. Prioritize Tier 1 (collections, JSON, time, test, math). Define `Hash` trait in prelude alongside `Eq`.

---

## 3. gap-lsp-ide.md — LSP Server And IDE Support

**Rating: ✅ Accept**

**Strengths:**
- Comprehensive LSP feature table with priority ordering.
- Tree-sitter grammar is the right foundation.
- Architecture (LSP server ↔ editor) is standard and well-understood.

**Issues:**
- LSP server startup time requirement (< 1s) is aggressive for a C++ binary doing full type-checking. Recommend incremental compilation or a persistent daemon mode.
- Tree-sitter grammar must be kept in sync with the hand-written parser — no mechanism is proposed for this. Recommend adding a conformance test suite.
- The proposal assumes the type checker can produce file-level diagnostics. Currently, type checking processes the full AST — this needs refactoring.

**Recommendation:** Accept with notes. Tree-sitter grammar can be developed independently (no C++ dependencies). LSP server should use a build cache approach rather than re-parsing every keystroke.

---

## 4. gap-formatter.md — Code Formatter (ng fmt)

**Rating: ✅ Accept**

**Strengths:**
- Clear formatting rules that match the existing codebase style.
- `--check` and `--diff` modes are standard and correct.
- Tree-sitter based approach is recommended — good choice.

**Issues:**
- The proposal lists two implementation strategies but doesn't decide. Recommend committing to tree-sitter based approach in the document.
- Comment preservation is listed as a challenge but no solution is proposed. Tree-sitter handles this with `set` captures.
- No mention of `ng fmt` integration in CI (pre-commit hook, GitHub Actions).

**Recommendation:** Accept. Commit to tree-sitter approach and add a CI integration section.

---

## 5. gap-package-manager.md — Package Manager (ngpkg)

**Rating: 🟡 Conditional Accept**

**Strengths:**
- Good dependency resolution design (SemVer + lockfile).
- `ng.toml` format is clean and familiar (Rust/Cargo inspired).
- Registry API is minimal and pragmatic.

**Issues:**
- **Significant**: The proposal does not address how package resolution integrates with the existing module system. Currently, `import math;` resolves to `math.ng` on disk. With packages, it must also search package install directories. This requires a virtual module path layer.
- No discussion of name collision between packages and stdlib modules.
- Authentication and publishing workflow is hand-waved.
- The default registry URL is mentioned but there is no discussion of operational cost or maintenance.

**Recommendation:** Accept conditionally. A pre-MVP that only supports git and path dependencies (no registry) would deliver 80% of the value with 20% of the complexity.

---

## 6. gap-concurrency.md — Concurrency Model (Async/Await)

**Rating: 🟡 Conditional Accept**

**Strengths:**
- Async/await is the right choice over goroutines for NG's type system.
- `Send`/`Sync` auto trait integration is well-motivated (reuse existing infrastructure).
- Clear trade-off analysis between Option A (async) and Option B (goroutines).

**Issues:**
- **Critical**: The VM is currently single-threaded and has no suspension mechanism. This proposal substantially under-estimates the VM rework needed.
- GC must become thread-safe — no mention of this in the proposal.
- The `spawn` keyword introduces runtime behavior that doesn't exist (thread pools, work stealing).
- The scoping is too broad for a single implementation phase. Recommend splitting: Phase 1 = async/await on single thread. Phase 2 = multi-threaded executor. Phase 3 = Send/Sync checking.

**Recommendation:** Accept for design, but mark as P2 (not P1). The VM is not ready for concurrency — the GC, call frames, and native bridge all need significant rework first.

---

## 7. gap-debugger.md — Debugger (DAP Adapter)

**Rating: ✅ Accept**

**Strengths:**
- DAP is the correct protocol — supported by all major editors.
- Source map generation is the right approach.
- Breakpoint patching via `DEBUG_BREAK` opcode is practical.

**Issues:**
- The proposal assumes the VM can be suspended and inspected. Currently, the VM executes in a tight loop with no suspension points. This is a major refactoring.
- Variable inspection requires mapping bytecode slots back to source-level names — the compiler doesn't currently emit this mapping.
- `step into`/`step over` require line-level source maps that the compiler doesn't emit today.

**Recommendation:** Accept but mark as P2. Defer until the VM has a proper debugging interface. The source map compiler changes can be developed independently.

---

## 8. gap-c-ffi.md — C ABI / External FFI

**Rating: ✅ Accept**

**Strengths:**
- `extern fun` syntax is clean and familiar from Rust/Zig.
- `*T` raw pointer and `unsafe` block design is correct (mirrors Rust's approach).
- `ng-bindgen` is a practical addition.

**Issues:**
- String marshaling: C `char*` can be UTF-8, arbitrary bytes, or null-terminated. The proposal says `string` or `*u8` — but the choice must be made at binding-generation time, not call time.
- Struct layout must match platform ABI — the proposal mentions this as a challenge but doesn't propose a solution. Recommend starting with `opaque type` for structs and adding field access later.
- No mention of the FFI safety model for the GC: `*T` pointers into GC-managed memory must be handled very carefully (pin or root). Without this, GC collections can invalidate raw pointers.

**Recommendation:** Accept as a phase 1 design. Phase 1: `extern fun`, non-GC raw pointers (external C memory only). Phase 2: `ng-bindgen`, struct access. Phase 3: GC-safe pointers.

---

## 9. gap-docgen.md — Documentation Generator (ng doc)

**Rating: ✅ Accept**

**Strengths:**
- `///` doc comment syntax is the right choice (matches Rust).
- Markdown support with code blocks is sufficient.
- Doc tests (`--test`) are a killer feature.

**Issues:**
- Cross-referencing (`[type]` auto-linking) is non-trivial — requires type resolution during doc generation. The proposal doesn't specify whether this uses the type checker or a simpler symbol table.
- Search functionality is listed but no implementation approach is given. Recommend client-side Fuse.js for MVP.

**Recommendation:** Accept. Mark cross-referencing and search as P2; start with basic `///` → HTML rendering.

---

## 10. gap-syntax-ergonomics.md — Syntax Ergonomics

**Rating: ✅ Accept**

**Strengths:**
- Each feature (interpolation, lambdas, for, match) has a clear desugaring path.
- Operator overloading via traits is type-safe and well-understood (Rust model).
- Optional<T> sugar leverages existing tagged union infrastructure.

**Issues:**
- **Scope is too large for one release.** String interpolation + lambdas alone is a substantial parser change. Recommend splitting: Batch 1 = interpolation + for/while. Batch 2 = lambdas + match expressions. Batch 3 = operator overloading.
- Lambda capture semantics are not specified — by-ref, by-value, or by-move? Recommend by-value default with `move` keyword for explicit ownership transfer.
- `for` loop over arrays requires an `IntoIterator` trait or a built-in desugaring — the proposal says "desugar to loop" but doesn't specify how `for item in arr` maps to `loop i = 0 { item = arr[i]; ... }`.

**Recommendation:** Accept with scoping. Implement string interpolation and `for`/`while` first (parser-only changes). Leave lambdas, operator overloading, and `match` expressions for later phases.

---

## 11. gap-type-system-enhancements.md — Type System Enhancements

**Rating: 🟡 Conditional Accept**

**Strengths:**
- GATs are correctly motivated (Collection::Iter pattern).
- `impl Trait` is well-specified for return position.
- `never` type is a small but impactful addition.

**Issues:**
- **GATs are extremely complex to implement correctly.** Rust spent 6+ years stabilizing GATs. Recommend deferring GATs to a very late phase.
- Compile-time borrow checker: this is effectively re-implementing a significant subset of Rust's borrow checker. The proposal under-estimates the complexity by an order of magnitude.
- The borrow checker would reject many patterns that currently work with runtime move checking. Migration path for existing code is not discussed.
- `impl Trait` in argument position is omitted — this is the more useful form (like `impl Into<String>` for ergonomic APIs). Recommend including it.

**Recommendation:** Accept conditionally. Implement in this order: (1) `never` type (small, well-bounded), (2) `impl Trait` in return + argument position, (3) compile-time borrow checker (major effort), (4) GAT (major effort, defer to last).

---

## 12. gap-build-system.md — Build System & Project Config

**Rating: ✅ Accept**

**Strengths:**
- Integrates well with the package manager proposal.
- Build profiles (debug/release/minimal/test) are practical.
- Incremental compilation is correctly identified as critical.

**Issues:**
- The proposal assumes `ng build` handles multi-file projects automatically, but the dependency graph construction logic is not specified (simplest approach: topological sort of import graph).
- Cross-platform file paths are mentioned as a challenge but no solution is proposed. Recommend using `std::filesystem` (already in the codebase) for path normalization.
- Standalone binary embedding is in scope but no design for how the VM is linked/stub is provided.

**Recommendation:** Accept. Focus MVP on `ng build` with `ng.toml` that just sets module paths and entry point. Add build profiles and incremental compilation after the basic pipeline works.

---

## 13. gap-runtime-optimization.md — Runtime Optimization (AOT/WASM/Embed)

**Rating: 🟡 Conditional Accept**

**Strengths:**
- Clear four-feature structure (AOT, WASM, embedding, JIT).
- LLVM backend is the correct approach for native compilation.
- C embedding API is well-designed.

**Issues:**
- **AOT compilation with GC is extremely hard.** LLVM stack maps for precise GC are complex to emit correctly. The proposal says "no GC in MVP" but this limits what NG programs can be compiled (no `new`, no `ref<T>`, no trait objects).
- WASM target with GC is even harder without WasmGC being widely supported.
- JIT compilation is listed but has minimal design detail. Recommend removing JIT from this proposal and making it a separate design document.

**Recommendation:** Accept conditionally. Split into separate proposals: (1) C embedding API (independent, high value), (2) LLVM AOT (major effort, GC-optional MVP), (3) WASM (dependent on WasmGC maturity), (4) JIT (separate proposal). Remove JIT from this scope.

---

## 14. gap-test-framework.md — Testing Framework & Benchmarks

**Rating: ✅ Accept**

**Strengths:**
- Comprehensive: unit tests, benchmarks, property tests.
- `describe`/`test` blocks are library-based (not syntax changes).
- JUnit XML output for CI integration.
- Property-based testing with `forAll` is ambitious but well-scoped.

**Issues:**
- `test "name" { body }` syntax requires either: (a) a special AST node, or (b) a function that takes a closure/string/block. The proposal says "library function" but NG doesn't have closures yet (deferred to syntax-ergonomics proposal). This creates a circular dependency.
- Benchmark measurement: `measure { ... }` returning `Duration` requires high-resolution timers and isolation from GC pauses. No mention of warm-up iterations or statistical methods.
- Property-based test shrinking is listed as a challenge but no approach is given (Hedgehog-style integrated shrinking vs QuickCheck-style manual shrinkers).

**Recommendation:** Accept with notes. Phase 1: `test`/`expect` as special AST nodes (like `const if`). Phase 2: benchmarking. Phase 3: property testing. Resolve the closure dependency by making `test` and `describe` built-in syntax initially.

---

## 15. gap-community-infrastructure.md — Community Infrastructure

**Rating: ✅ Accept**

**Strengths:**
- Well-organized (website, playground, RFC, roadmap, channels, governance).
- RFC process design is mature (borrowing good ideas from Rust).
- Roadmap format is clear and chronological.

**Issues:**
- The playground section lists three implementation options but doesn't choose one. Recommend starting with server-side execution (simplest) and adding WASM later.
- Website maintenance is noted as ongoing work but there's no proposal for automation (CI/CD, automatic doc publishing from `ng doc`).
- No discussion of translation/internationalization. English-only is fine for MVP but should be acknowledged as a limitation.

**Recommendation:** Accept. Focus on (1) website with static docs, (2) RFC process, (3) GitHub Discussions for community. Defer playground to after WASM compilation is available.
