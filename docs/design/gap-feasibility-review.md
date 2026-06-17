# Gap Proposals — Architecture & Feasibility Review

This document evaluates each proposal against the **existing codebase architecture**. Proposals are rated:
- ✅ **Feasible** — can be implemented with the current architecture
- ⚠️ **Challenging** — needs architectural changes but is doable
- 🔴 **High Risk** — fundamental architecture mismatch, needs redesign

---

## Foundation: Existing Architecture Summary

| Component | Size | Architecture | Extensibility |
|---|---|---|---|
| **Lexer** (`Lexer.cpp`) | 739 lines | State machine | ✅ Add new tokens easily |
| **Parser** (`ParserImpl.cpp`) | 2,497 lines | Recursive descent | ⚠️ Large switch/if-else chains, need careful disambiguation |
| **AST** (`ast.hpp`) | 1,400 lines | Node types + Visitor | ✅ Add new node types easily |
| **Type Checker** (`typecheck.cpp`) | 7,737 lines | Internal `TypeChecker` class + `type_check()` free function | ⚠️ Monolithic, hard to extend without touching 7K+ file |
| **Trait Registry** (`trait_registry.hpp`) | ~100 lines | Symbol table + resolution | ✅ Clean API, easy to add auto trait checks |
| **Compiler** (`Compiler.cpp`) | 2,776 lines | Visitor pattern (`Compiler : DummyVisitor`) | ✅ Easy to add new visit() methods |
| **VM** (`VM.cpp`) | 1,541 lines | Bytecode interpreter `execute_slots()` | ⚠️ Monolithic switch loop, no suspension |
| **GC** (`managed_heap.cpp`) | 151 lines | Simple mark-sweep | ✅ Simple, easy to add mutex |
| **STUPID** (`stupid.cpp`) | 3,542 lines | Internal visitors | ⚠️ Large file, but good visitor separation |
| **Module System** (`ModuleRegistry`) | ~200 lines | Path-based resolution | ✅ Clean API |

---

## Individual Proposal Feasibility

### 1. Error Handling (`Result<T,E>` + `?`) — ✅ Feasible (1-2 weeks)

**Architecture fit:**
- `Result<T,E>` reuses existing tagged union infrastructure (`TaggedUnionType`, `TAGGED_VALUE_EXPRESSION`)
- `?` operator needs a new AST node (`ErrorPropagationExpression`) plus parser change
- Type checker: add one `visit(ErrorPropagationExpression*)` method → changes to `typecheck.cpp` (~50 lines)
- Compiler: `?` desugars to existing `SWITCH_TAG` + `RETURN` — **no new opcodes needed**
- VM: no changes — desugared bytecode uses existing instructions

**Risk assessment:** 🟢 Low. All required infrastructure (tagged unions, switch, return) already exists.
**Key unknown:** How to integrate `Result<T,E>` return type with existing native C++ bridge (`prelude.cpp`).

### 2. Standard Library Expansion — ✅ Feasible (incremental, per-module)

**Architecture fit:**
- Each module is a separate `.ng` file + optional C++ native backing — the module system already supports this
- `Hash` trait: existing `TraitRegistry` + auto trait infrastructure can handle this
- `HashMap`: opaque native type exposing through `register_native()`
- `std.math`: pure native functions, simplest case

**Risk assessment:** 🟢 Low. Every module is independent. No core language changes needed except `Hash` trait.

**Key unknown for `HashMap`:** The `Hash` trait needs to map to `std::hash` in C++. The existing auto-derive system needs to support `derive(Hash)` for structural types.

### 3. LSP Server — ⚠️ Challenging (architectural changes needed)

**Architecture issues:**
1. **Type checker is all-or-nothing**: The `type_check()` function processes a full `CompileUnit`. There's no API to type-check a single expression or incrementally update. **A new incremental API must be built on top** — likely wrapping `type_check()` with a document cache.
2. **Parser is not incremental**: Each parse starts from scratch. For LSP responsiveness (<200ms), we need either a very fast parser (existing one is fast enough for single files) or incremental parsing.

**Mitigation:**
- Start with **full-file recheck** on each change (acceptable for files <1000 lines)
- The `DocumentCache` design in the proposal is correct — cache parsed AST, recheck on change
- For autocomplete, replace cursor with a sentinel token and parse to get completions

**Risk assessment:** 🟡 Medium. The LSP server is a new binary, so it doesn't risk breaking existing code. The main effort is wrapping existing APIs into an LSP-friendly form.

### 4. Code Formatter — ✅ Feasible (straightforward)

**Architecture fit:**
- The formatter is a standalone tool that walks the existing AST
- No interactions with type checker, compiler, or VM
- Can be built and tested independently

**Risk assessment:** 🟢 Low. Pure AST traversal + string output. No dependencies on other proposals.

### 5. Package Manager (Git + Path only) — ✅ Feasible

**Architecture fit:**
- `NG_MODULE_PATH` environment variable already exists and is read by `ModuleRegistry`
- The package manager just needs to clone repos and set the path
- No core language changes needed

**Risk assessment:** 🟢 Low. Shell-out to git, manage file cache, set environment variable.

### 6. Concurrency (Async/Await) — 🔴 High Risk (significant VM rework)

**Architecture issues:**
1. **VM execution is not suspendable**: The `execute_slots()` method is a tight loop with no suspension points. Adding `YIELD` requires refactoring the entire execution model to support **resumable frames**.
2. **State machine desugaring**: The compiler generates a state machine class for each `async fun`. This requires the compiler to generate new type definitions, methods, and fields — currently, the compiler only **lowers** existing AST nodes, it doesn't **generate new ones**.
3. **Single-threaded assumption**: The existing GC, symbol table, and runtime env all assume single-threaded access. Every shared structure needs auditing.

**Does the codebase support this?** Let's check what would break:

```cpp
// VM Frame currently has no resume capability:
struct Frame {
    const BytecodeModule *module = nullptr;
    const Function *function = nullptr;
    size_t ip;                    // ← This is the only state needed for resume!
    Vec<RuntimeRef<StorageCell>> locals;
};
```

Actually, a `Frame` DOES have an `ip` (instruction pointer). Adding suspension might be simpler than expected — save the frame and restore it later. But `execute_slots()` returns a value, it doesn't suspend mid-execution.

**Mitigation:**
- Phase 1 proposal is **too ambitious** for the current architecture
- **Replace async/await with callback-based concurrency** initially:

```ng
// Instead of async/await, start with:
spawn task1();
spawn task2();
waitAll();  // Block until all tasks complete
```

This uses thread pools and avoids the VM suspension problem entirely. Full async/await can be added later when the VM supports it.

**Revised recommendation:**
- **Defer async/await to post-MVP** (after embedding API exists)
- **Phase 1 alternative**: Add a `spawn` keyword that runs functions on a thread pool, with `wait()` / `waitAll()` for synchronization
- No VM changes needed for Phase 1 — tasks are just C++ threads calling `execute_slots()`

**Risk assessment:** 🔴 High. Proposal as written requires VM rework. Simplified alternative is 🟡 Medium.

### 7. Debugger (DAP) — ⚠️ Challenging (VM changes needed)

**Architecture issues:**
1. `DEBUG_BREAK` opcode requires the VM to **suspend** execution — same problem as async/await
2. Source maps: The compiler doesn't currently emit line-number-to-bytecode mappings
3. Variable inspection requires mapping bytecode slots to source-level names

**What exists:**
- The VM's `push_frame()` and `call_stack` provide stack trace information
- Source positions are tracked in AST nodes (for error messages)

**Mitigation:**
- Phase 1: **Source maps only** — emit mappings from compiler (usable for error reporting too)
- Phase 2: **VM pause/resume** — add `SUSPEND` opcode and debug interface (same mechanism as async)
- Phase 3: **Variable inspection** — emit local variable name→slot mappings

**Risk assessment:** 🟡 Medium. Phase 1 (source maps) is easy. Phases 2-3 depend on VM suspension infrastructure.

### 8. C FFI (`extern`, `*T`, `unsafe`) — ⚠️ Challenging (ABI bridge)

**Architecture issues:**
1. **Platform-specific assembly trampolines**: Calling C functions requires platform-specific ABI handling (x86-64 SysV vs Windows x64 vs ARM64). This code must be written in assembly or via `libffi`.
2. **GC safety**: `*T` raw pointers into GC-managed memory can be invalidated by GC collections. Must either pin objects or restrict `*T` to non-GC memory.
3. **Type marshaling**: The existing `StorageCell` representation must be converted to C ABI types.

**What exists:**
- `dlopen`/`dlsym` for dynamic loading (POSIX)
- The native bridge already marshals NG values to C++ types

**Mitigation:**
- Use `libffi` (vendored) instead of hand-written assembly trampolines — cross-platform, maintained
- Phase 1: `extern fun` only for C functions that take/return scalar types (no pointers to GC memory)
- Phase 2: `*T` raw pointers to non-GC memory (allocated by `malloc`)
- Phase 3: Safe `*T` with GC pinning

**Risk assessment:** 🟡 Medium. Phase 1 is feasible with `libffi`. The assembly trampolines are the hard part — using `libffi` avoids this.

### 9. Documentation Generator — ✅ Feasible

**Architecture fit:**
- New standalone tool, no dependencies on type checker or VM
- Lexer changes for `///` are minimal (6 lines in `Lexer.cpp`)
- Cross-reference resolution can use the existing parser (already parses files)
- Doc test execution: reuse existing `eval_const_function` or `execute_slots()`

**Risk assessment:** 🟢 Low. Straightforward implementation.

### 10. Syntax Ergonomics — ✅ Feasible (incremental)

**Batch 1 (String interpolation + for/while):**
- String interpolation: parser desugars to concatenation → no runtime changes
- `for` loop: uses existing `LoopBindingType::LOOP_IN` — just needs parser support
- `while` loop: desugars to existing `loop`
- `break`/`continue`: new AST nodes, simple VM support

**Batch 2 (Lambdas):**
- Closure capture requires new runtime objects (captured variable storage)
- The type checker needs to infer lambda types → new `FunctionType` construction
- **Most complex** of the three batches

**Batch 3 (Match expression + Operator overloading):**
- Match reuses existing `switch` infrastructure
- Operator overloading: trait-based, maps to existing method dispatch
- Moderate complexity

**Risk assessment:** 🟢 Low for Batch 1, 🟡 Medium for Batch 2 (closures), 🟢 Low for Batch 3.

### 11. Type System Enhancements — 🔴 High Risk (several areas)

**Sub-proposal A (`never` type):** ✅ Feasible
- Add `PrimitiveType` variant `NEVER`
- Type checker rule: `never` unifies with any type
- ~50 lines change in `typecheck.cpp`
- **Risk:** 🟢 Low

**Sub-proposal B (`impl Trait`):** ⚠️ Challenging
- Requires existential type support in the type checker
- The existing trait object system (`ref dyn Trait`) handles dynamic dispatch. `impl Trait` is a different concept (static dispatch, opaque).
- Need to add `ImplTraitType` to `typeinfo.hpp` and handle unification
- **Risk:** 🟡 Medium

**Sub-proposal C (Compile-time borrow checker):** 🔴 High Risk
- **Current state**: The type checker already tracks `movedBindings` with ancestor/descendant relationships and borrow conflicts. This is a **good foundation**.
- **What's missing**: Lifetime tracking (does a reference outlive its referent?), NLL (non-lexical lifetimes), two-phase borrows, reborrowing
- The existing `rejectBorrowConflict()` function shows the architecture works, but extending it to cover all Rust-level borrow rules is a **major research effort** (Rust spent 5+ years stabilizing)
- **Recommendation**: Keep runtime move checking as the primary mechanism. Add compile-time warnings for simple cases (lexical borrows only). Do NOT attempt full Rust-style borrow checker.

**Sub-proposal D (GAT):** 🔴 High Risk
- GATs require lifetime-generic associated types. NG currently has **no lifetime system at all**.
- HKT infrastructure exists (`GenericTypeDef::kindArity`), but it only supports type parameters, not lifetime parameters
- Adding lifetimes to the type system is a prerequisite — this is a 6-12 month effort on its own
- **Recommendation**: Defer indefinitely. GATs are not needed for any near-term use case.

**Revised recommendation:**
- ✅ `never` type: Implement (1-2 weeks)
- ✅ `impl Trait`: Implement (3-4 weeks)
- ⚠️ Borrow checker: **Limit to lexical borrow warnings** only. Keep runtime checking as safety net.
- ❌ GAT: **Remove from scope**. Not achievable without lifetime system.

### 12. Build System — ✅ Feasible

**Architecture fit:**
- The import graph can be constructed by parsing source files (parser already exists)
- Topological sort + cycle detection is standard algorithm
- Cache invalidation uses file hashes (simple)

**Risk assessment:** 🟢 Low. Well-understood problem, good existing infrastructure.

### 13. Runtime Optimization (Embedding/LLVM/WASM) — ⚠️ Challenging

**Sub-proposal A (C Embedding API):** ✅ Feasible
- Refactoring the VM into a library is straightforward (move files, add C API)
- The existing `VM` class already has a clean public interface
- **Risk:** 🟢 Low (estimated 4-6 weeks)

**Sub-proposal B (LLVM AOT):** 🔴 High Risk
- LLVM backend is a 3-6 month project
- GC integration (stack maps) is the hardest part
- **Recommendation:** Defer to post-MVP. The bytecode VM is fast enough for most use cases.

**Sub-proposal C (WASM):** 🔴 High Risk
- Approach A (compile VM to WASM via Emscripten): ✅ Feasible but produces large binary (~37MB)
- Approach B (direct NG→WASM): Cannot be done without LLVM backend first
- **Recommendation:** Implement Approach A for the online playground. Approach B is post-MVP.

**Revised recommendation:**
- ✅ Embedding API: Implement (highest value, lowest risk)
- ⚠️ WASM via Emscripten: Implement for playground (moderate effort)
- ❌ LLVM AOT: Defer. The bytecode VM serves well for now.
- ✅ Remove JIT from scope (correctly done in the refined proposal)

### 14. Testing Framework — ✅ Feasible

**Architecture fit:**
- `test "name" { body }` as a special AST form (like `const if`)
- The parser already handles custom statement forms
- Per-test VM isolation: create a new VM instance for each test
- Benchmark: use `std::chrono::high_resolution_clock`

**Key design decision:** `test` should be a **special AST node**, not a library function (since closures don't exist yet). This matches how `const if` works.

**Risk assessment:** 🟢 Low. Reuses established patterns (special AST forms, VM isolation).

### 15. Community Infrastructure — ✅ Feasible (process + content)

**No code changes needed.** Website, RFC process, and governance are organizational, not technical.

**Risk assessment:** 🟢 Low.

---

## Revised Implementation Roadmap

Based on this feasibility review:

| Quarter | Proposals | Feasibility |
|---|---|---|
| **Q3 2026** (Foundation) | ① Error Handling (Result + ?) | ✅ Feasible |
| | ② Stdlib: Hash, HashMap, math, JSON | ✅ Feasible |
| | ③ String interpolation + for/while | ✅ Feasible |
| | ④ `never` type + `impl Trait` | ✅ Feasible |
| **Q4 2026** (Tooling) | ⑤ Code Formatter (`ng fmt`) | ✅ Feasible |
| | ⑥ Testing Framework | ✅ Feasible |
| | ⑦ Package Manager (git/path) | ✅ Feasible |
| | ⑧ LSP Server (Phase 1: diagnostics) | ⚠️ Challenging |
| **Q1 2027** (Ecosystem) | ⑨ Documentation Generator | ✅ Feasible |
| | ⑩ Build System | ✅ Feasible |
| | ⑪ C Embedding API | ✅ Feasible |
| | ⑫ Lambdas + match + operator overloading | ⚠️ Medium |
| **Deferred** | Async/Await (needs VM rework) | 🔴 Redesign needed |
| | Borrow checker (compile-time) | 🔴 Scope to lexical warnings only |
| | LLVM AOT / WASM native | 🔴 Post-MVP |
| | GAT (needs lifetime system) | 🔴 Remove from scope |

## Proposals Needing Final Updates

Based on this review, the following proposals must be updated:

1. **gap-concurrency.md**: Replace async/await with spawn/wait model. Add explicit note that full async/await is deferred.
2. **gap-type-system-enhancements.md**: Remove GAT from Sub-D (mark as "indefinitely deferred"). Scope borrow checker to lexical warnings only.
3. **gap-debugger.md**: Clarify that source maps are Phase 1, VM suspension is Phase 2.
4. **gap-c-ffi.md**: Add `libffi` dependency note. Clarify GC safety for `*T`.
5. **gap-runtime-optimization.md**: Mark LLVM AOT as post-MVP. Keep embedding API and Emscripten WASM.
6. **gap-error-handling.md**: Add note that `?` can be implemented without new opcodes (desugaring approach).