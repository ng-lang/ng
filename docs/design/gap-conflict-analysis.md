# Gap Proposals — Conflict & Compatibility Analysis

This document analyzes **cross-proposal conflicts** and **compatibility with existing implementation** for all 15 gap proposals.

---

## 🔴 Critical Conflicts (Must Resolve Before Implementation)

### C1. `for` Loop vs Existing `impl Trait for Type` Syntax

**Proposal:** [gap-syntax-ergonomics Batch 1](gap-syntax-ergonomics.md) introduces `for i in 0..10` syntax.
**Existing:** `KEYWORD_FOR` (line 58 in `token.hpp`) is already consumed by the parser for `impl Trait for Type` (lines 992, 1029 in `ParserImpl.cpp`).
**Conflict Type:** 🔴 **Parsing Ambiguity**

```
// Existing usage (consumes KEYWORD_FOR):
impl Show for Point { ... }

// Proposed usage (same token):
for i in 0..10 { ... }
```

**Resolution:**
- The parser already has `LoopBindingType::LOOP_IN = 1` defined but **never used** (in `ast.hpp` line 475).
- `for` statement parsing happens in a different context than `impl` (statement-level vs declaration-level).
- **Recommendation:** Instead of creating a new `ForStatement` AST node, implement `LoopBindingType::LOOP_IN` in the existing `loop` parsing path. `for i in range` desugars to `loop i = range.iter()` using existing infrastructure.
- The syntax proposal must be updated to specify **reuse of existing `LoopStatement` + `LOOP_IN`** rather than creating a new node type.

### C2. `?` Operator vs Existing `QUERY` / `UNDEFINED` Tokens

**Proposal:** [gap-error-handling](gap-error-handling.md) introduces `?` as a postfix error propagation operator.
**Existing:** `QUERY = 0x0B00` (`?`, line 148 in `token.hpp`) and `UNDEFINED` (`???`, line 149) already exist.
**Conflict Type:** 🔴 **Token Reuse**

The `?` token already exists but its exact usage is undefined. The proposal assumes no existing token. The lexer needs to distinguish:
- `?` as postfix → error propagation (`readFile(path)?`)
- `?` as ternary → `condition ? expr1 : expr2` (if future ternary)
- `???` as undefined/null → future `Option<T>` syntax

**Resolution:**
- The proposal correctly specified `QUESTION = 0x0B00`, but `QUERY` at that hex value already exists. Use `QUERY` token instead.
- Postfix `?` vs ternary `?` is disambiguated by position: expression-token-`?` is postfix; `?`-expression-`:` is ternary.
- **Recommendation:** Update proposal to use existing `QUERY` token, add disambiguation rules.

### C3. `yield` vs Existing YIELD_STATEMENT AST Node

**Proposal:** [gap-concurrency](gap-concurrency.md) Phase 1 mentions `yield` for task suspension.
**Existing:** `YIELD_STATEMENT = 0x504` (line 90 in `ast.hpp`) already exists as an AST node type, but no `KEYWORD_YIELD` exists in the token enum.
**Conflict Type:** 🟡 **Naming/Naming Convention**

The `yield` keyword from the concurrency proposal should reuse the existing `YIELD_STATEMENT` node rather than creating a new one.

**Resolution:**
- Add `KEYWORD_YIELD` to token enum.
- Reuse existing `YIELD_STATEMENT` AST node.
- Implement parser support for `yield` statement.
- **Recommendation:** Update concurrency proposal to reference existing infrastructure.

---

## 🟡 Significant Compatibility Concerns

### D1. `readFile` Return Type Migration

**Proposal:** [gap-error-handling](gap-error-handling.md) and [gap-stdlib-expansion](gap-stdlib-expansion.md) change `readFile`/`writeFile` to return `Result<T, E>`.
**Existing:** `lib/std/io.ng` line 5: `fun readFile(path: string) -> string = native;`
**Compatibility Issue:** 🟡 **Breaking Change**

```ng
// Existing code (breaks):
val content = readFile("data.txt");    // ERROR: now returns Result
print(content);                         // ERROR: content is Result, not string

// New code (requires ? operator):
val content = readFile("data.txt")?;   // OK
```

**Impact:** Every existing NG program that uses `readFile`/`writeFile` breaks. This includes:
- `lib/std/prelude.ng` — uses `readFile` indirectly
- `example/18.stdlib_basics.ng` — uses `readFile`
- `example/56.stdlib_modules.ng` — uses `readFile`
- `ng_ide.ng` — uses `readFile`

**Mitigation Options:**
1. **Deprecation cycle**: Keep old functions as `readFileUnsafe`, add new `readFile` returning `Result`
2. **Dual API**: Both `readFile: string -> string` (crash on error) and `readFileSafe: string -> Result<string, IOError>`
3. **Once-and-for-all**: Break everything, fix all examples (only ~5 files)

**Recommendation:** Option 3 (break once, fix all). The project is young enough that full migration is feasible. Update error-handling proposal to include explicit migration path with all file changes listed.

### D2. Loop Infrastructure: `LOOP_IN` Already Exists But Unused

**Proposal:** [gap-syntax-ergonomics Batch 1](gap-syntax-ergonomics.md) defines a new `ForStatement` AST node.
**Existing:** `LoopBindingType::LOOP_IN = 1` (line 475 in `ast.hpp`) is defined but never parsed. `LoopStatement` already has a `bindings` vector that supports `LOOP_IN`.
**Compatibility Issue:** 🟡 **Design Redundancy**

The `for i in range` proposal creates redundant infrastructure. The existing `LoopStatement` already supports in-bindings:

```cpp
struct LoopStatement : Statement {
    Vec<LoopBinding> bindings{};  // Can hold LOOP_IN bindings
};
```

**Resolution:** Update the syntax-ergonomics proposal to:
- Implement `KEYWORD_FOR` in statement parsing position (after semicolons, not inside `impl` blocks)
- Create `LOOP_IN` bindings in the existing `LoopStatement`
- `for i in 0..10 { body }` → `LoopStatement { bindings: [{name: "i", type: LOOP_IN, target: RangeExpr(0, 10)}], body: body }`

### D3. Auto Trait `Send`/`Sync` vs Existing Auto Trait Infrastructure

**Proposal:** [gap-concurrency](gap-concurrency.md) Phase 3 introduces `Send`/`Sync` checking.
**Existing:** `include/typecheck/trait_registry.hpp` has `isAutoTrait()`, `typeCanDeriveTrait()`, auto trait structural satisfaction checking. 
**Compatibility:** ✅ **Good Fit**

The existing auto trait infrastructure can be reused:
1. Declare `auto trait Send;` in prelude (already works, see `example/55.auto_derive_traits.ng`)
2. The type checker's `satisfiesAutoTrait()` function checks field-level satisfaction
3. Opt-out syntax (`impl !Send for Type`) needs to be added

**Recommendation:** Update concurrency proposal to reference `trait_registry.hpp` API explicitly.

### D4. `Hash` Trait vs Derive System

**Proposal:** [gap-stdlib-expansion](gap-stdlib-expansion.md) introduces `trait Hash` with manual impls for primitives.
**Existing:** The derive system supports `derive(Copy + Clone)`. 
**Compatibility Issue:** 🟡 **Interaction with derive**

Many types that derive `Copy` or `Clone` would also want `derive(Hash)`. The proposal doesn't mention this. 

**Resolution:** Add `derive(Hash)` support alongside manual impl. The derive system can auto-generate `Hash` for structural types where all fields are `Hash`.

### D5. GC Thread-Safety Gap

**Proposal:** [gap-concurrency](gap-concurrency.md) Phase 2 assumes GC becomes thread-safe.
**Existing:** `src/runtime/managed_heap.cpp` implements a single-threaded tracing GC. No mutexes, no atomic operations, no concurrent marking.

**Impact:** This is a **foundational rework** of the GC — not a small change. The concurrency proposal severely underestimates this.

**Resolution:** Add explicit GC thread-safety design to the concurrency proposal:
- Phase 2a: Add GC mutex (stop-the-world, simple)
- Phase 2b: Concurrent marking (complex, optional)

---

## 🔵 Minor Issues

### E1. Test Name Confusion: `ng test` vs `ng_test`

**Proposal:** [gap-test-framework](gap-test-framework.md) uses `ng test` command.
**Existing:** `build/ng_test` is the C++ Catch2 test binary.

**Risk:** Users may confuse `ng test` (runs NG-level tests) with `./build/ng_test` (runs C++ compiler-level tests).

**Mitigation:** Document the distinction clearly in CLI help. Consider `ng test` vs `ng run-tests` or `ng check`.

### E2. `never` Type vs Existing `unit` Type

**Proposal:** [gap-type-system-enhancements Sub-A](gap-type-system-enhancements.md) adds `never` type.
**Existing:** `unit` type represents absence of value. `never` must be distinct.

**Risk:** New developers may confuse `never` (bottom type, never produces a value) with `unit` (produces a value that is always `unit`).

**Resolution:** The type checker must:
- `never` unifies with ANY type (it's a subtype of all types)
- `unit` unifies only with `unit`
- Functions returning `never` cannot complete normally
- Functions returning `unit` always complete with `unit`

### E3. `impl Trait` vs Existing Trait Object Syntax

**Proposal:** [gap-type-system-enhancements Sub-B](gap-type-system-enhancements.md) adds `impl Trait`.
**Existing:** `ref dyn Trait` for trait objects.

**Distinction:**
| Syntax | Semantics | Dispatch |
|---|---|---|
| `ref dyn Trait` | Heap-allocated, type-erased | Dynamic |
| `impl Trait` | Stack-allocated, concrete type known | Static (monomorphized) |

**Risk:** Users may confuse the two. `impl Trait` is generics; `ref dyn Trait` is dynamic dispatch.

**Resolution:** No code change needed, but documentation must clearly distinguish.

### E4. `package-manager` Module Path Integration

**Proposal:** [gap-package-manager](gap-package-manager.md) sets `NG_MODULE_PATH` to include dependency paths.
**Existing:** Module resolution in `src/module/ModuleLoader.cpp` and `src/module/ModuleRegistry.cpp` already uses a search path.
**Compatibility:** ✅ **Good Fit**

The existing `ModuleRegistry::resolveModule()` searches through registered paths. Setting `NG_MODULE_PATH` environment variable or calling the registry API directly both work without changes.

### E5. Operator Overloading Trait Names vs Existing Operators

**Proposal:** [gap-syntax-ergonomics Batch 3](gap-syntax-ergonomics.md) adds `trait Add`, `trait Sub`, etc.
**Existing:** Binary arithmetic operators are hardcoded in the type checker.

**Risk:** Custom operator implementations for built-in types (e.g., `impl Add for i32`) could conflict with hardcoded type-checker behavior.

**Resolution:** The type checker should check for inherent/trait operator implementations BEFORE falling back to hardcoded behavior. Built-in operators should be treated as default implementations that user code can override.

---

## Summary Table

| ID | Proposals | Issue | Severity | Resolution |
|---|---|---|---|---|
| C1 | syntax-ergonomics Batch 1 | `for` loop vs `impl ... for` | 🔴 **Critical** | Reuse existing `LOOP_IN` binding |
| C2 | error-handling | `?` vs existing `QUERY` token | 🔴 **Critical** | Use existing `QUERY` token |
| C3 | concurrency | `yield` vs `YIELD_STATEMENT` | 🟡 **Significant** | Reuse existing AST node |
| D1 | error-handling + stdlib | `readFile` return type breakage | 🟡 **Significant** | Plan migration, fix 5 files |
| D2 | syntax-ergonomics Batch 1 | Redundant `ForStatement` node | 🟡 **Significant** | Reuse `LoopStatement` + `LOOP_IN` |
| D3 | concurrency Phase 3 | Send/Sync vs existing auto trait infra | ✅ **Good fit** | Reference existing API |
| D4 | stdlib-expansion | `derive(Hash)` not mentioned | 🟡 **Significant** | Add derive support |
| D5 | concurrency Phase 2 | GC thread-safety | 🟡 **Significant** | Add explicit GC design |
| E1 | test-framework | `ng test` vs `ng_test` naming | 🔵 **Minor** | Document distinction |
| E2 | type-system Sub-A | `never` vs `unit` confusion | 🔵 **Minor** | Clear type rules |
| E3 | type-system Sub-B | `impl Trait` vs `ref dyn Trait` | 🔵 **Minor** | Documentation |
| E4 | package-manager | Module path integration | ✅ **Good fit** | Already compatible |
| E5 | syntax-ergonomics Batch 3 | Custom op overloading vs hardcoded | 🔵 **Minor** | Check traits before builtins |

---

## Required Proposal Updates

| Proposal | Changes Needed |
|---|---|
| **gap-error-handling.md** | Use existing `QUERY` token (not new `QUESTION`). Add explicit migration path for readFile/writeFile. |
| **gap-syntax-ergonomics.md** | Replace `ForStatement` with `LoopStatement` + `LOOP_IN` binding. Remove `KEYWORD_FOR` from new tokens (exists). Add `KEYWORD_WHILE`. Mention `break`/`continue` tokens already exist. |
| **gap-concurrency.md** | Reference existing `YIELD_STATEMENT` AST node. Add GC thread-safety design section. Add `KEYWORD_YIELD` to token changes. |
| **gap-stdlib-expansion.md** | Add `derive(Hash)` section. Reference existing `SpanType`/`RangeType` for collection operations. |
| **gap-test-framework.md** | Add note about `ng test` vs `ng_test` naming. |
| **gap-type-system-enhancements.md** | Add `never` vs `unit` distinction rules. Clarify `impl Trait` vs `ref dyn Trait`. |
| **gap-c-ffi.md** | Check if `KEYWORD_AS` exists for `as` cast syntax. |
| **gap-build-system.md** | Reference existing `ModuleRegistry::resolveModule()` API. |
| **gap-package-manager.md** | Reference existing `ModuleRegistry::resolveModule()` API. |