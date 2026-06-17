# Gap Proposals — INVEST / SOLID Assessment

## Methodology

**INVEST** scores (1-5, higher is better):

| Letter | Meaning | What it measures |
|---|---|---|
| **I** | Independent | Can it be developed and delivered separately from other proposals? |
| **N** | Negotiable | Is there room for scope/schedule tradeoffs, or is it a fixed spec? |
| **V** | Valuable | Does each increment deliver clear value to a stakeholder? |
| **E** | Estimable | Can an engineer estimate effort to ±25% with the current detail? |
| **S** | Small | Can it be completed in a single development cycle (2-4 weeks)? |
| **T** | Testable | Can acceptance criteria be verified objectively? |

**Score key:** 5=Excellent, 3=Adequate, 1=Missing

**Gate:** A score of **≤3 in any dimension** means the proposal needs refinement before execution.

**SOLID** assessment per proposal:

| Principle | What it evaluates |
|---|---|
| **SRP** | Does the proposal cover one feature/concern? |
| **OCP** | Can the design be extended without modifying existing code? |
| **LSP** | Does it integrate with the existing NG type system and runtime without breaking contracts? |
| **ISP** | Are interfaces (APIs, traits, module boundaries) minimal and focused? |
| **DIP** | Does it depend on abstractions, not concretions? |

---

## 1. gap-error-handling.md (P0)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 4 | 5 | 3 | 3 | 4 | **4.0** |

**Breakdown:**
- **I=5**: Independent of all other proposals. No downstream dependencies.
- **N=4**: Merging try/catch with Result/? is negotiable — could start with Result only.
- **V=5**: Clearly valuable — no production code can be written without error handling.
- **E=3**: Parser changes for `?` operator are specified, but AST node changes and ORGASM opcodes are not detailed. What new AST nodes? What opcodes? What's the scope of parser work vs VM work vs type checker work?
- **S=3**: 4 sub-features (Result, ?, try/catch, throw) — too big for one cycle. Needs splitting into at least 2 phases.
- **T=4**: Acceptance criteria are testable, but miss negative tests (what happens with unhandled errors?).

**SOLID Assessment:**
- **SRP ⚠️**: Covers Result type, ? operator, try/catch, AND throw. Slightly exceeds one responsibility. Recommend splitting.
- **OCP ✅**: Result<T,E> reuses existing tagged union infrastructure. ? desugars to switch. Good integration.
- **LSP ✅**: ? desugars to existing pattern matching — no new type system concepts.
- **ISP ✅**: Minimal API surface.
- **DIP ✅**: Depends on existing AST node types and pattern matching.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=3 (VM opcodes not detailed), S=3 (too large)
**Action:** Split into 2 phases, add AST/opcode specification, add negative test cases.

---

## 2. gap-stdlib-expansion.md (P0)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 5 | 5 | 2 | 2 | 3 | **3.7** |

**Breakdown:**
- **I=5**: Independent of other proposals (except Result dependency).
- **N=5**: Highly negotiable — which modules, what APIs, implementation order.
- **V=5**: Stdlib is the primary developer interface.
- **E=2**: Too vague. "HashMap<K,V>" without signature detail. "JSON parser" without API design. Cannot estimate without function signatures.
- **S=2**: Covers 12+ modules across 3 tiers. Way too large for one cycle.
- **T=3**: Acceptance criteria are stated but not quantified (e.g., "hashmap stores and retrieves" — how many ops/sec? How big?).

**SOLID Assessment:**
- **SRP ❌**: Violates SRP — 12 distinct modules in one proposal. Each module is a separate responsibility.
- **OCP ✅**: Module system already supports new stdlib modules as separate files.
- **LSP ✅**: New modules are additive — no existing code breaks.
- **ISP ✅**: Each module has focused API surface.
- **DIP ✅**: Stdlib modules depend on prelude and each other through imports.

### Verdict: ⚠️ Needs Refinement — CRITICAL
**Issues:** E=2 (no function signatures), S=2 (12 modules in one proposal)
**Action:** Split into per-module sub-proposals with full API signatures. Prioritize Tier 1 modules.

---

## 3. gap-lsp-ide.md (P0)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 4 | 5 | 3 | 3 | 4 | **4.0** |

**Breakdown:**
- **I=5**: Independent — LSP server uses existing parser and type checker.
- **N=4**: Feature scope is negotiable (P0-P4 priorities clearly marked).
- **V=5**: IDE support is essential for adoption.
- **E=3**: Tree-sitter grammar is estimable, but LSP server integration with the type checker is not detailed enough. How does the LSP call the type checker? Per-file? Incrementally?
- **S=3**: Two major components (tree-sitter, LSP server) — borderline too large. Tree-sitter grammar can be split as independent deliverable.
- **T=4**: Acceptance criteria are good but missing LSP protocol conformance tests.

**SOLID Assessment:**
- **SRP ⚠️**: LSP server covers diagnostics, completion, hover, goto, etc. — many features but they share the same infrastructure.
- **OCP ✅**: New LSP features can be added as new request handlers.
- **LSP ✅**: Exposes existing compiler capabilities — doesn't change them.
- **ISP ✅**: LSP protocol defines minimal interfaces.
- **DIP ✅**: Depends on parser and type checker APIs.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=3 (type checker integration not detailed)
**Action:** Add specific API for incremental diagnostics, specify tree-sitter grammar conformance tests.

---

## 4. gap-formatter.md (P0)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 5 | 4 | 2 | 4 | 3 | **3.8** |

**Breakdown:**
- **I=5**: Independent — formatter is a standalone tool.
- **N=5**: Style rules are negotiable (config file).
- **V=4**: Valuable for teams, but individual developers may not prioritize it.
- **E=2**: Two implementation approaches listed but no decision. Tree-sitter based needs grammar first (circular with LSP proposal). Hand-written needs AST traverser design. Cannot estimate without choosing an approach.
- **S=4**: Single tool, well-scoped.
- **T=3**: "Idempotent output" is testable, but verifying all edge cases (deeply nested generics, complex unions) needs specific test fixtures.

**SOLID Assessment:**
- **SRP ✅**: Single concern (formatting).
- **OCP ✅**: Style rules are configurable.
- **LSP ✅**: Output is valid NG source — no integration issues.
- **ISP ✅**: Simple CLI interface.
- **DIP ✅**: Depends on AST or tree-sitter output.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=2 (no implementation approach decided)
**Action:** Commit to an implementation approach and add detailed algorithm description.

---

## 5. gap-package-manager.md (P1)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 4 | 4 | 5 | 2 | 2 | 3 | **3.3** |

**Breakdown:**
- **I=4**: Independent enough, but depends on module path resolution changes.
- **N=4**: Registry URL, auth approach are negotiable.
- **V=5**: Without packages, no third-party ecosystem can exist.
- **E=2**: Missing lockfile format, cache directory structure, resolver algorithm. Cannot estimate without these details.
- **S=2**: Covers manifest format, dependency resolution, registry API, lockfile, publishing. Way too large.
- **T=3**: Acceptance criteria mention version conflict errors but no specific scenarios.

**SOLID Assessment:**
- **SRP ❌**: Violates SRP — package resolution, registry protocol, publishing workflow, lockfile management are separate concerns.
- **OCP ⚠️**: Resolver should support different source types (git, path, registry) — mentioned but not designed.
- **LSP ✅**: Integrates with existing module system through virtual paths.
- **ISP ⚠️**: Registry API is minimal but undefined.
- **DIP ✅**: Package resolution depends on module path abstraction.

### Verdict: ⚠️ Needs Refinement — SIGNIFICANT
**Issues:** E=2 (no resolver algorithm, lockfile, cache), S=2 (too large)
**Action:** Scope down to git+path dependencies only (MVP), then add registry + lockfile in phase 2.

---

## 6. gap-concurrency.md (P1)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 3 | 3 | 4 | 1 | 1 | 2 | **2.3** |

**Breakdown:**
- **I=3**: Depends on VM rework and potentially error handling.
- **N=3**: Two options presented (async vs goroutines) but no clear decision. Scope is negotiable though.
- **V=4**: Valuable but most applications can work without concurrency initially.
- **E=1**: Essentially unestimable. VM rework scope is undefined. GC thread safety is a PhD-level problem. No task model specification.
- **S=1**: The largest proposal by scope — async/await, Send/Sync, thread pool, channels, GC rework, VM rework. At minimum 3-6 months of work.
- **T=2**: "Two async tasks exchange data" — too vague for a pass/fail test.

**SOLID Assessment:**
- **SRP ❌**: Severely violates SRP — async syntax, runtime, GC, Send/Sync checking, channels, thread pools.
- **OCP ✅**: If well-designed, new concurrency primitives slot in.
- **LSP ⚠️**: GC thread safety breaks existing GC contract. VM suspension breaks existing execution model.
- **ISP ⚠️**: Send/Sync as auto traits is clean.
- **DIP ⚠️**: Depend on VM internals which are not designed for concurrency.

### Verdict: 🔴 Requires Complete Restructuring
**Issues:** E=1, S=1, SRP violation
**Action:** Split into 4+ sequential deliveries. This proposal needs the most work.

---

## 7. gap-debugger.md (P1)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 4 | 5 | 4 | 3 | 3 | 4 | **3.8** |

**Breakdown:**
- **I=4**: Mostly independent, but depends on source maps from compiler.
- **N=5**: DAP features can be delivered incrementally (P0-P4).
- **V=4**: High value for production use but not for initial adoption.
- **E=3**: VM suspension mechanism is hand-waved. Source map format not specified. DAP messages not listed.
- **S=3**: DAP server + VM changes + source maps is too large for one cycle.
- **T=4**: DAP conformance tests exist as a standard.

**SOLID Assessment:**
- **SRP ⚠️**: Debugger + source maps are two related but separable concerns.
- **OCP ✅**: New DAP features added as new handlers.
- **LSP ⚠️**: VM suspension is a fundamental change to the execution model.
- **ISP ✅**: DAP protocol defines minimal interface.
- **DIP ✅**: Debugger depends on VM debug interface (abstraction to be defined).

### Verdict: ⚠️ Needs Refinement
**Issues:** E=3 (VM suspension mechanism), S=3 (too many sub-components)
**Action:** Specify VM debug interface, source map binary format, split source maps from debugger.

---

## 8. gap-c-ffi.md (P1)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 5 | 5 | 3 | 3 | 4 | **4.2** |

**Breakdown:**
- **I=5**: Independent — new syntax and runtime support.
- **N=5**: Binding generator scope is highly negotiable.
- **V=5**: Most languages are extended through C libraries.
- **E=3**: extern fun parsing is estimable, but calling convention bridge between VM stack and C ABI registers is not detailed.
- **S=3**: extern + *T + unsafe + bindgen = 4 features. Bindgen should be separate.
- **T=4**: "Call a real C library function" is objectively testable.

**SOLID Assessment:**
- **SRP ⚠️**: extern/unsafe/raw pointers vs bindgen tool — two separable concerns.
- **OCP ✅**: New C library bindings don't modify NG core.
- **LSP ✅**: Extern functions fit alongside native functions.
- **ISP ✅**: Minimal syntax additions.
- **DIP ⚠️**: Calling convention bridge depends on platform ABI, not an abstraction.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=3 (calling convention bridge), S=3 (bindgen should split)
**Action:** Specify calling convention details, split bindgen into phase 2.

---

## 9. gap-docgen.md (P1)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 5 | 4 | 2 | 4 | 3 | **3.8** |

**Breakdown:**
- **I=5**: Independent.
- **N=5**: Output format, theme, search are all negotiable.
- **V=4**: Valuable for API documentation but not blocking.
- **E=2**: Parser changes for `///` not specified. Doc test execution mechanism unspecified. Cross-reference resolution algorithm missing.
- **S=4**: Single tool, well-bounded.
- **T=3**: "Generates valid HTML" is vague. Need specific rendering tests.

**SOLID Assessment:**
- **SRP ✅**: Single responsibility (documentation generation).
- **OCP ✅**: New output formats can be added.
- **LSP ✅**: Doc comments are just special comments — no runtime impact.
- **ISP ✅**: Simple CLI interface.
- **DIP ⚠️**: Doc test execution depends on VM.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=2 (missing parser changes, doc test mechanism)
**Action:** Add lexer changes for `///`, spec for cross-reference resolution, doc test sandboxing design.

---

## 10. gap-syntax-ergonomics.md (P2)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 3 | 5 | 4 | 2 | 1 | 3 | **3.0** |

**Breakdown:**
- **I=3**: Lambdas depend on closure type system support. Operator overloading depends on trait system.
- **N=5**: Each feature is independently negotiable.
- **V=4**: High value for developer experience.
- **E=2**: 7 separate features blended together. Cannot estimate as a single unit.
- **S=1**: 7 features (interpolation, lambdas, for, while, match, operator overloading, Optional sugar). Largest proposal by scope.
- **T=3**: Each feature is individually testable, but acceptance criteria lump them together.

**SOLID Assessment:**
- **SRP ❌**: 7 distinct language features. Extreme SRP violation.
- **OCP ⚠️**: Each feature is additive, but they may interact (e.g., match + pattern matching).
- **LSP ✅**: Each feature independently integrates with existing language.
- **ISP ✅**: Each feature has minimal syntax.
- **DIP ✅**: Depends on existing AST nodes.

### Verdict: 🔴 Requires Complete Splitting
**Issues:** S=1 (7 features), E=2, SRP violation
**Action:** MUST split into 3-4 separate proposals before any implementation.

---

## 11. gap-type-system-enhancements.md (P2)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 2 | 4 | 4 | 1 | 1 | 2 | **2.3** |

**Breakdown:**
- **I=2**: Borrow checker depends on partial move tracking. GAT depends on HKT.
- **N=4**: Each feature is independently negotiable.
- **V=4**: Important for expressiveness, but language works without them.
- **E=1**: GAT implementation alone is a 6-12 month effort (witness Rust). Borrow checker is similarly complex. Unestimable at this level of detail.
- **S=1**: 4 major features, each a full project. By far the largest cumulative scope.
- **T=2**: Acceptance criteria are vague ("mutable reference exclusivity is enforced" — what about NLL? Two-phase borrows?).

**SOLID Assessment:**
- **SRP ❌**: 4 independent type system features. Extreme SRP violation.
- **OCP ✅**: Each feature adds to type checker without changing existing passes.
- **LSP ⚠️**: Borrow checker will break existing code that compiles with runtime move checking.
- **ISP ✅**: Each feature has focused syntax.
- **DIP ✅**: Builds on existing type checker infrastructure.

### Verdict: 🔴 Requires Complete Restructuring
**Issues:** E=1, S=1, T=2
**Action:** MUST split into 4 separate proposals. never type should be delivered first (small, estimable). impl Trait second. Borrow checker third. GAT last.

---

## 12. gap-build-system.md (P1)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 5 | 4 | 2 | 3 | 3 | **3.7** |

**Breakdown:**
- **I=5**: Standalone tool.
- **N=5**: Build configuration format and commands are highly negotiable.
- **V=4**: Important for project organization, but workable with manual commands.
- **E=2**: Dependency graph construction algorithm not specified. Incremental compilation cache invalidation not specified. Cannot estimate without these.
- **S=3**: Build system + incremental compilation is borderline large.
- **T=3**: "Incremental build: touching one file only recompiles that file" — testable but misses edge cases (header-like dependencies through imports).

**SOLID Assessment:**
- **SRP ⚠️**: Build system + incremental compilation are related but separable.
- **OCP ✅**: New build targets can be added.
- **LSP ✅**: Build system orchestrates compilation — no runtime impact.
- **ISP ✅**: Simple CLI interface.
- **DIP ✅**: Depends on module loader's dependency resolution.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=2 (dependency graph algorithm, cache invalidation)
**Action:** Specify import graph construction algorithm, cache format, invalidation strategy.

---

## 13. gap-runtime-optimization.md (P2)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 2 | 4 | 3 | 1 | 1 | 2 | **2.2** |

**Breakdown:**
- **I=2**: AOT depends on stable bytecode format. Embedding depends on VM library. WASM depends on LLVM.
- **N=4**: Each sub-feature is independently negotiable.
- **V=3**: Important for deployment, but the bytecode VM works well enough initially.
- **E=1**: LLVM backend with GC stack maps is a multi-month project. WASM compiler is similarly large. Unestimable.
- **S=1**: 4 features (AOT, WASM, Embed, JIT) — each is a 3-6 month project.
- **T=2**: "AOT performance is at least 2x faster" — vague. What benchmark? What baseline?

**SOLID Assessment:**
- **SRP ❌**: 4 independent runtime features. Extreme SRP violation.
- **OCP ⚠️**: If designed well, LLVM backend is a new CodeGen pass. But it's inherently tightly coupled.
- **LSP ⚠️**: AOT output must preserve GC behavior — very hard.
- **ISP ✅**: Embedding C API is minimal.
- **DIP ⚠️**: LLVM backend is tightly coupled to LLVM IR.

### Verdict: 🔴 Requires Complete Restructuring
**Issues:** E=1, S=1, T=2
**Action:** MUST split into 4 independent proposals. Embedding API should be first (smallest, highest value). Remove JIT entirely (too speculative).

---

## 14. gap-test-framework.md (P1)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 4 | 5 | 5 | 2 | 3 | 3 | **3.7** |

**Breakdown:**
- **I=4**: Mostly independent, but test syntax may need AST changes.
- **N=5**: Deeply negotiable — which features, what API shape.
- **V=5**: Essential for writing reliable code.
- **E=2**: test discovery algorithm not specified. test isolation mechanism not specified. benchmark statistical method not specified. Cannot estimate without these.
- **S=3**: Unit tests + benchmarks + property tests = 3 features. Should be split.
- **T=3**: "A passing test suite produces exit code 0" — testable but trivial. Need property test falsification tests.

**SOLID Assessment:**
- **SRP ⚠️**: Unit tests, benchmarks, and property tests are distinct concerns.
- **OCP ✅**: New assertion types can be added.
- **LSP ✅**: Tests use existing language features — no runtime changes needed.
- **ISP ✅**: Minimal API for each feature.
- **DIP ✅**: Tests depend on VM execution.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=2 (test discovery, isolation, benchmarking stats)
**Action:** Specify test discovery algorithm, isolation mechanism, benchmark statistical method.

---

## 15. gap-community-infrastructure.md (P2)

### INVEST Score

| I | N | V | E | S | T | **Avg** |
|---|---|---|---|---|---|---|
| 5 | 5 | 3 | 2 | 3 | 2 | **3.3** |

**Breakdown:**
- **I=5**: Independent — no code dependencies on any other proposal.
- **N=5**: Everything is negotiable — tech stack, channels, governance model.
- **V=3**: Valuable but not a technical language feature. Doesn't make the language more capable.
- **E=2**: Website is estimable (~2 weeks). Playground depends on WASM (unestimable). RFC process is process-only. Cannot estimate as a single unit.
- **S=3**: Multiple loosely related components — borderline.
- **T=2**: "Website is live" is not a meaningful test. "Playground executes NG programs" needs specific test suites.

**SOLID Assessment:**
- **SRP ⚠️**: Website + playground + RFC + community management are separate concerns.
- **OCP ✅**: New community channels can be added.
- **LSP ✅**: No integration with the language itself.
- **ISP ✅**: Each component is independently usable.
- **DIP ✅**: No code dependencies.

### Verdict: ⚠️ Needs Refinement
**Issues:** E=2 (mixed scopes), T=2 (vague acceptance criteria)
**Action:** Split into infrastructure components with specific tech stacks and deliverable timelines.

---

## Summary

| # | Proposal | INVEST Avg | SRP | Verdict |
|---|---|---|---|---|
| 1 | error-handling | 4.0 | ⚠️ | Minor refinement |
| 2 | **stdlib-expansion** | **3.7** | **❌** | **CRITICAL — add signatures, split** |
| 3 | lsp-ide | 4.0 | ⚠️ | Minor refinement |
| 4 | **formatter** | **3.8** | ✅ | **Choose approach** |
| 5 | **package-manager** | **3.3** | **❌** | **SIGNIFICANT — scope down** |
| 6 | **concurrency** | **2.3** | **❌** | **RESTRUCTURE** |
| 7 | debugger | 3.8 | ⚠️ | Minor refinement |
| 8 | c-ffi | 4.2 | ⚠️ | Minor refinement |
| 9 | **docgen** | **3.8** | ✅ | **Add parser spec** |
| 10 | **syntax-ergonomics** | **3.0** | **❌** | **RESTRUCTURE — split into 3+** |
| 11 | **type-system** | **2.3** | **❌** | **RESTRUCTURE — split into 4** |
| 12 | **build-system** | **3.7** | ⚠️ | **Add algorithm details** |
| 13 | **runtime-optimization** | **2.2** | **❌** | **RESTRUCTURE — split into 4** |
| 14 | **test-framework** | **3.7** | ⚠️ | **Add algorithm details** |
| 15 | community-infra | 3.3 | ⚠️ | Minor refinement |

### Proposals Needing Restructuring (🔴)

| Proposal | Split into |
|---|---|
| **concurrency** (6) | Phase 1: Single-threaded async/await + Future type. Phase 2: Thread pool + spawn. Phase 3: Send/Sync checking. Phase 4: Channels. |
| **syntax-ergonomics** (10) | Batch 1: String interpolation + for/while. Batch 2: Lambdas + closures. Batch 3: Match expressions + operator overloading. |
| **type-system** (11) | Proposal A: never type. Proposal B: impl Trait. Proposal C: Borrow checker. Proposal D: GAT. |
| **runtime-optimization** (13) | Proposal A: C embedding API. Proposal B: LLVM AOT. Proposal C: WASM. (Remove JIT.) |

### Proposals Needing Algorithm Details (E=2→4)

| Proposal | Missing |
|---|---|
| **error-handling** (1) | AST node definitions, ORGASM opcodes |
| **stdlib** (2) | Function signatures per module |
| **package-manager** (5) | Lockfile format, cache structure, resolver algorithm |
| **debugger** (7) | VM suspension interface, source map format |
| **c-ffi** (8) | Calling convention bridge design |
| **docgen** (9) | Parser lexer change spec |
| **build-system** (12) | Dependency graph algorithm, cache invalidation |
| **test-framework** (14) | Test discovery, isolation, benchmark stats |
