# Workstate

## Current focus

Executing the vNext increment plan (`docs/design/rearchitecture/09-increment-plan.md`) one
increment at a time: design → implement → verify (unit + conformance/PBT corpus, ASan
variant) → doc-sync → commit (feat + docs separately).

Maintainer swap decision (2026-08-30): **INC-14 minimal closures moved before
INC-12/13**, executed in S1–S6 slices because the combinators/law PBT need transform
arguments that NG cannot express without first-class functions.

## Status

- INC-0 … INC-8, ARCH-1 … ARCH-4, PBT consolidation: committed.
- INC-A (prelude auto-loading): skipped by user decision (visibility-blind overload registration).
- INC-B (core traits → `prelude.ng`): committed `c1e733b`/`4040ecd`.
- INC-9 (error codes + did-you-mean): committed `e38e51c`/`55b815d`.
- INC-10 + DEC-B (while/continue/break): committed; D-010 revised.
- INC-11 (derive(Show)): committed `fe8ee4d`/`75be8c5`; ASan imgui link fix committed
  `120eb50` (imgui archives kept uninstrumented).
- INC-14 **S1** (minimal closures — syntax, explicit param types, direct calls): committed
  `ffae039`/`bc4729a` (512 cases / 3511 assertions, ctest 513/513, ASan 512/512).
- INC-14 **S2a+S2b** (zero-param `||` heads, expected-type-driven inference, `|T| -> R`
  type literal, tail-return latent fix): committed `777651d`/`ed2e18c`/`2a0d879`/`002849e`
  (515 cases / 3525 assertions, ctest 516/516, ASan 515/515).
- INC-14 **S2c** (higher-order closures + std.seq `map<T,U>`): implemented, suite green,
  commit pending (525 cases / 3553 assertions, ctest 526/526, ASan 525/525).

## Recent completed changes

- INC-14 S2c: closure-typed function parameters act semi-generic — every concrete
  closure argument (literal or closure-typed local) creates a closure-specialized
  instance via `instantiateFunction` (`closureTargets`, key `|cl{index}#{defId}`),
  parameter local bound to the literal's code DefId via `closureLocals_`; the original
  higher-order function is marked `placeholderFunctions_` (driver lowers a stub Return,
  body compiles only in instances). Deferred template-body path: calls through a param
  local of a placeholder are typed from the closure descriptor without recording a
  call target. `analyzeCallSpread` and `inferExpected`'s generic branch handle closure
  args (shared `Checker::inferCallClosureArgument`); `TypeInterner::specialize` and
  `unifyGuarded` gained Closure cases. `lib/std/seq.ng` now has `export fun map<T, U>`.
  Parser fix: `ModuleParser::parseTypeUntil` pipe-depth nesting so `|i64, i64|` isn't
  truncated at the parameter comma in function signatures. Tests `test/closure_ho_test.cpp`
  (10 cases, closure suites 15→25); `example/closures.ng` +6 groups.
- INC-14 S2a+S2b: `parsePrefix` routes `OrOr` to `parseClosure` as an empty head
  (`|| expr`); new `ClosureTypeSyntax` (`|T| -> R`) → `hir::TypeKind::Closure` →
  `internClosure`; closure parameter annotations may be omitted and are inferred
  from the expected closure type (`inferExpected` Closure branch, factored
  `Checker::inferClosure`, records the closure expression type — previously an
  `unordered_map::at` crash). No-expected-type untyped params → clean error;
  result mismatch → clean error. FlowIR now wires the function-body tail value
  into the fallthrough Return (latent bugfix: bare `ret` previously returned a
  leftover QBE register; `|| 7` exposed it). Guard ceilings raised: flowir
  2177→2185, ast.hpp 938→955 (justified). Tests 12→15 closure cases (+2 S2
  runtime +3 negative/S2); `example/closures.ng` extended.
- INC-14 S1: `ClosureExpression` AST; resolver closure params/body; `TypeKind::Closure`;
  per-literal synthesized code function (`__closureN`, body cloned + param-local
  remapped, re-checked via `instances_`); calls reuse `callTargets`; FlowIR direct-call
  lowering with the callee local operand skipped; closure literal lowers to an inert
  zero word; `qtypeOf` Closure → Long. S1 contracts: explicit param types required,
  captures rejected (negative contract until S3). Also fixed
  `BlockParser::parseExpressionUntilAny` missing its `braceDepth` guard on the primary
  terminator break (truncated block-bodied closure let initializers). Guard ceilings
  raised in `test/guard_test.cpp`: flowir 2149→2177, ast.hpp 925→938 (justified).
  Tests: `test/closure_test.cpp` (12 cases) + `example/closures.ng`.
- INC-11: `export trait Show` in `lib/std/prelude.ng`; derive validator + `toString`
  native dispatch; FlowIR `buildShowString` field-level synthesis; guard 2075→2150.
- ASan imgui link fix: `-fno-sanitize=address,undefined` on `ngrt_imgui` + vendored
  `imgui` (plain `cc` links generated executables without the sanitizer runtime).

## Next steps

- Commit INC-14 S2c (feat/docs/chore).
- INC-14 **S3** (copy/move captures — enabling closures over enclosing state).
- The plan also lists differential contract seeding (map vs map-spread) — carries into S6.
- Thereafter INC-12 (combinators + law PBT), then INC-13 `?` desugar per plan order.

## References

- `.agents/worklog/2026-08-27.md` — INC-B / INC-A / INC-9 / INC-10 entries
- `.agents/worklog/2026-08-30.md` — INC-11 entry
- `.agents/worklog/2026-08-31.md` — INC-14 S1 entry + S2a+S2b entry + S2c entry
- `docs/design/rearchitecture/09-increment-plan.md` — execution table + INC-14 slices
- `AGENTS.md` — pipeline/build/test/commit conventions