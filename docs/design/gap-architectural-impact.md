# Architecture Redesign Analysis: What Actually Needs to Change?

This document answers: **Does the existing codebase need to be redesigned before implementing the gap proposals?**

## Executive Summary

**No — 12 of 15 proposals can be implemented without touching existing code architecture.**

The current architecture is clean enough for incremental feature additions. Only 3 proposals require codebase restructuring, and those restructurings are **independent of each other** and can be done incrementally.

---

## Detailed Analysis

### ✅ Can Be Implemented Without Any Refactoring (12 proposals)

| Proposal | Implementation Strategy | Changes Needed |
|---|---|---|
| **Error Handling** (`Result` + `?`) | `?` desugars to existing `switch` + `return`. No new opcodes. `Result<T,E>` reuses tagged unions. | Add `ErrorPropagationExpression` AST node + parser → desugar in compiler. ~150 lines new code. |
| **Stdlib Expansion** | Each module is a new `.ng` file + optional native C++ functions. Module system already supports this. | New files in `lib/std/`. New native registrations in `src/stdlib/`. |
| **Code Formatter** | Standalone tool. Walks existing AST. No dependency on type checker or VM. | New `src/formatter/` directory. No changes to existing code. |
| **Package Manager** | Shells out to `git`. Sets `NG_MODULE_PATH`. No core changes. | New `src/pkg/` directory. No changes to existing code. |
| **Concurrency** (spawn/wait) | Uses C++ `std::thread` + `std::future`. Each task runs in its own VM instance. No VM suspension needed. | New AST nodes + 2 new opcodes (SPAWN, AWAIT). Thread pool in new `src/runtime/thread_pool.cpp`. |
| **Doc Generator** | Standalone tool. Uses lexer for `///` scanning. Reuses parser. | New `src/docgen/` directory. Lexer: +6 lines for `///`. |
| **Build System** | Orchestrates existing parser + compiler. No new compiler passes. | New `src/pkg/` (shared with package manager). |
| **Testing Framework** | `test` as special AST node (like `const if`). Per-test VM instance. | New AST nodes. Parser recognizes `test "..." { }`. Test runner is new binary. |
| **Syntax Batch 1** (interpolation, for/while) | Desugars at parse time. `for` reuses existing `LoopBindingType::LOOP_IN`. | Parser-only changes. String interpolation: concatenation desugaring. |
| **`never` type** | New `PrimitiveType` variant. ~50 lines in `typecheck.cpp`. | Add `NEVER` to typeinfo. Add unification rule. |
| **`impl Trait`** | New `ImplTraitType` in `typeinfo.hpp`. Existential type in type checker. | Moderate: ~300 lines in typecheck.cpp. |
| **Community Infrastructure** | No code changes. | Process + website only. |

### ⚠️ Needs Targeted Codebase Refactoring (3 proposals)

#### 1. LSP Server — Needs `type_check()` Wrapper

**What must change:** The type checker currently processes a full `CompileUnit` at once. LSP needs incremental checking.

**Solution (not a redesign, just a wrapper):**

```cpp
// New: DocumentCache wraps the existing type_check() function
class DocumentCache {
    // ... caches parsed ASTs, calls type_check() on change
    // Returns: list of errors per file (already emitted by type_check())
    auto updateDocument(const std::string &path, const std::string &source) -> std::vector<Diagnostic>;
};
```

**Lines changed:** ~200 new code in a new file. **Zero changes to existing `typecheck.cpp`.**

#### 2. Embedding API — Needs VM Library Extraction

**What must change:** The `ngi` executable currently owns `main()`. To embed NG in other apps, the VM must become a library.

**Solution:** Move files, add CMake target:

```
Before: src/main.cpp → ngi executable
After:  src/libng/   → libng.{a,so}
        src/main.cpp → thin CLI calling libng API
```

**Lines changed:** Move ~20 files, add C API header. **Existing code logic unchanged.**

#### 3. Debugger Phase 2+ — Needs VM Suspension

**What must change:** The VM's `execute_slots()` is a tight loop with no suspension. Breakpoints need `DEBUG_BREAK` opcode that pauses execution.

**Solution:** Add `SUSPEND` opcode + `VM::debugInterface`:

```cpp
// Current: tight loop
while (ip < code.size()) { switch (opcode) { ... } }

// After: suspendable loop
while (ip < code.size()) {
    if (suspendRequested) { waitForCommand(); }
    switch (opcode) { ... }
}
```

**Lines changed:** ~100 lines in `VM.cpp`. **Existing opcodes unchanged.**

#### 4. Lambdas/Closures (Syntax Batch 2) — Needs Capture Support

**What must change:** Lambdas that capture variables need heap-allocated closure objects.

**Solution:** Add closure struct generation in the compiler. The `StorageCell` system already supports fields — closures are structurally identical to objects.

**Lines changed:** ~500 lines in compiler + type checker.

---

## What Does NOT Need to Change

| Proposed Change | Verdict | Reason |
|---|---|---|
| Split `typecheck.cpp` into smaller files | ❌ Not needed | Adding visit() methods is ~50 lines per feature. The file's size is manageable. |
| Rewrite VM to support async/await | ❌ **Deferred forever** | Replaced spawn/wait model instead. Symptom solved. |
| Add lifetime system for GAT | ❌ **Removed from scope** | GAT deferred indefinitely. |
| Add LLVM backend | ❌ **Deferred to post-MVP** | Bytecode VM is sufficient. |
| Rewrite GC for thread safety | ❌ **Deferred** | Spawn/wait uses separate VM instances. No shared heap. |

---

## What Should Be Done First: A Concrete Plan

### Month 1-2: Foundation (no codebase refactoring needed)

```
1. Error Handling (Result + ?)       → 1 week  ← unlocks Result-based APIs
2. Stdlib: Hash + HashMap + math     → 3 weeks ← unlocks data structures
3. String interpolation + for/while  → 1 week  ← big UX improvement
4. `never` type                      → 0.5 week
5. Code Formatter (ng fmt)           → 5 weeks ← parallel with 1-4
```

**Architecture changes:** None. All additive.

### Month 3-4: Tooling (requires one refactoring)

```
6. Embedding API (libng)             → 4 weeks  ← **refactoring: VM as library**
7. Package Manager (git MVP)         → 4 weeks  ← uses libng
8. Build System                      → 3 weeks  ← uses libng
9. Testing Framework                 → 4 weeks  ← uses libng for per-test VMs
```

**Architecture change:** Extract `libng` from `ngi` (file moves, new CMake target, no logic changes).

### Month 5-6: Ecosystem

```
10. Stdlib: JSON, time, test        → 4 weeks
11. LSP Server (diagnostics only)    → 4 weeks  ← wrapper around type_check()
12. Spawn/wait concurrency           → 2 weeks  ← thread pool, separate VMs
13. `impl Trait` + operator overload → 4 weeks
```

**Architecture change:** `DocumentCache` wrapper (new file, no existing code changes).

### Month 7-8: Advanced

```
14. Doc Generator                   → 4 weeks
15. C FFI (extern + libffi)         → 6 weeks
16. Lambda/closures                 → 3 weeks
17. Borrow checker (lexical only)   → 4 weeks
18. Debugger Phase 1 (source maps)  → 2 weeks
```

**Architecture change:** Closure runtime objects, VM suspension for debugger (Phase 2+).

---

## Conclusion

| Question | Answer |
|---|---|
| Does existing code need to be redesigned? | **No.** 12/15 proposals are additive. |
| Does any proposal require touching typecheck.cpp? | Minimal: new `visit()` methods (~50 lines each) |
| Does the VM need to be rewritten? | **No.** The spawn/wait model avoids VM suspension. |
| Is the GC thread-safe? | Not needed — spawn/wait uses separate VM instances. |
| What's the only mandatory refactoring? | **Extract libng** (file moves, no logic changes). Needed for embedding, tests, build system. |
| When should libng extraction happen? | Month 3-4, after the initial P0 features are shipped. |