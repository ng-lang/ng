# Codebase Quality Audit: Does NG Need to Be Rewritten?

**Short answer: No. The codebase is solid and should NOT be rewritten.**

This document is the result of a systematic audit of all 28,804 lines of source code and 7,727 lines of headers.

---

## 1. Architecture Quality: Green

| Criterion | Finding | Verdict |
|---|---|---|
| Dependency direction | `ast ← parser ← typecheck ← compiler ← vm` No cycles. | ✅ |
| Design patterns | Visitor pattern throughout. RAII for memory. Factory-like constructors. | ✅ |
| Encapsulation | Components are separated by directory (`src/parsing/`, `src/typecheck/`, `src/orgasm/`). | ✅ |
| Single responsibility | Each directory has one job. Files are focused. | ✅ |
| Platform isolation | Platform-specific code isolated to `src/sysdep/process.cpp` (3 platforms). | ✅ |

> **A rewrite would destroy this architecture for zero gain.**

## 2. Code Quality: Green with Minor Warnings

| Indicator | Finding | Rating |
|---|---|---|
| Naming consistency | `snake_case` for files, `PascalCase` for types, `camelCase` for functions. Consistent. | ✅ |
| Modern C++ usage | C++23: `auto`, `constexpr`, `[[nodiscard]]`, `std::expected`-like patterns, smart pointers. | ✅ |
| Memory safety | `std::shared_ptr` for AST nodes. `NonCopyable` base class. No raw `new`/`delete`. | ✅ |
| Error handling | C++ exceptions for compiler errors. Runtime exceptions for VM errors. Appropriate. | 🟡 |
| Comments/TODOs | Only 1 TODO in the entire codebase. Doxygen in most headers. | ✅ |
| Code duplication | Some duplication between STUPID and Compiler (visitors for similar node types). Minor. | 🟡 |

### Specific Minor Issues (Not Worth a Rewrite)

```
Issue 1: Global mutable state in typecheck.cpp (10 inline static variables)
Impact: Makes parallel type checking impossible; tests must be careful about ordering.
Fix: Pass state through constructor instead of globals. (~1 day of refactoring.)
```

```
Issue 2: typecheck.cpp is 7,737 lines
Impact: Hard to navigate, but this is NORMAL for a type checker (Rust's type_check.rs is 15,000+ lines).
Fix: NOT a rewrite. Just extract helper functions to separate files if it becomes unwieldy.
```

```
Issue 3: VM opcode switch is ~800 lines (lines 640-1460)
Impact: One giant switch statement. Hard to navigate.
Fix: Extract each opcode handler to a named function. (~2 days of mechanical refactoring.)
```

```
Issue 4: 39x repetitive makecheck<PrimitiveType>(typeinfo_tag::XXX)
Impact: Boilerplate for each numeral type.
Fix: Template or macro. (~1 hour.)
```

## 3. Test Coverage: Green

| Metric | Value | Assessment |
|---|---|---|
| Test files | 46 `.cpp` files | ✅ Excellent |
| Test cases | 695 | ✅ Excellent |
| Assertions | 3,327 | ✅ Excellent |
| All passing | Yes | ✅ |
| Parsing tests | 16 files | ✅ |
| Type check tests | 15 files | ✅ |
| Runtime tests | 6 files | ✅ |
| ORGASM tests | 5 files | ✅ |
| Integration tests | Multiple | ✅ |

> **A rewrite would lose this test coverage. 695 tests passing is a huge asset.**

### One Gap

| Missing | Impact | Severity |
|---|---|---|
| STUPID interpreter has NO dedicated unit tests (only integration). | Bug fixes in STUPID require running all integration tests. | 🟡 Medium |

## 4. What Would a Rewrite Cost?

| Resource | Estimated | Notes |
|---|---|---|
| Time to rewrite from scratch | 12-18 months | Full lexer, parser, type checker, compiler, VM |
| Time to match existing test coverage | +6 months | Writing 695+ tests takes time |
| Risk of new bugs | Very high | Every rewrite introduces regressions |
| Lost documentation | Complete | All Doxygen, design docs, examples need rework |
| Developer months | ~24 person-months | Assuming 2 developers |

**vs. incremental improvement:**

| Resource | Estimated |
|---|---|
| Fix all minor issues (globals, switch extraction, duplication) | ~2-3 weeks |
| Add STUPID unit tests | ~1 week |
| Ship all 15 gap proposals | ~8 months |

## 5. Final Verdict

| Question | Answer |
|---|---|
| Does the codebase need to be rewritten? | **Absolutely not.** |
| Is the code quality acceptable? | **Yes.** Above average for a research language. |
| What's the biggest risk? | Not the code quality — it's the **monolithic typecheck.cpp** making team collaboration harder. But that's a maintenance concern, not a correctness concern. |
| What should be done instead? | **Ship features.** The code is good enough. The 695 passing tests prove it. Fix the global state issue if it becomes a problem. |

### If You Insist on Refactoring (Not Recommended)

The ONLY refactoring that would meaningfully improve the codebase:

1. **Extract `libng`** (4 weeks) — turns VM into a shared library. Unlocks embedding, testing, and build system. This is file movement, not rewriting.
2. **Replace `inline static` globals** in typecheck.cpp with constructor parameters (1-2 days).
3. **Extract opcode handlers** from the giant VM switch to individual functions (2 days).

**Total:** ~5 weeks of safe, incremental refactoring. **Not a rewrite.**

> **"Rewrite the codebase" is the siren song of every mature project. Don't listen. The code works, it's tested, and it's well-structured. Ship the features."**