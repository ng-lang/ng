# Workstate

## Current focus

Executing the vNext increment plan (`docs/design/rearchitecture/09-increment-plan.md`) one
increment at a time: design → implement → verify (unit + conformance/PBT corpus, ASan
variant) → doc-sync → commit (feat + docs separately).

## Status

- INC-0 … INC-8, ARCH-1 … ARCH-4, PBT consolidation: committed.
- INC-A (prelude auto-loading): skipped by user decision (visibility-blind overload registration).
- INC-B (core traits → `prelude.ng`): committed `c1e733b`/`4040ecd`.
- INC-9 (error codes + did-you-mean): committed `e38e51c`/`55b815d`.
- INC-10 + DEC-B (while/continue/break): committed; D-010 revised.
- INC-11 (derive(Show)): implemented + verified (500 cases / 3468 assertions, ctest 501/501);
  ASan 499/500 — the one failure is the **pre-existing, unrelated** imgui link in
  `driver_test.cpp:178`. Commits pending (feat + docs staged below).

## Recent completed changes

- INC-11: `export trait Show` in `lib/std/prelude.ng`; derive validator accepts
  string / `toString`-scalar (i64/i32/u64/f64/bool) / nested Show struct fields and records
  `toString` DefIds; infer `show` special-case; FlowIR `buildShowString` synthesizes
  `name{field: value, ...}` strings (same shape as derive(Eq) extension point). Local
  `trait Show` in prelude-importing sources renamed `RenderText`; guard ceiling 2075→2150.
  Tests: golden contracts + property PBT (24 random structs) + 4 negative contracts;
  `example/derive.ng` extended (exit 127→255).

## Next steps

- Commit INC-11 as two commits: `feat(vnext): INC-11 derive(Show) …` and
  `docs(vnext): INC-11 doc sync`.
- Then INC-12 onward per `docs/design/rearchitecture/09-increment-plan.md`.
- Do not chase the ASan imgui link failure in `driver_test.cpp:178` (pre-existing).

## References

- `.agents/worklog/2026-08-27.md` — INC-B / INC-A / INC-9 / INC-10 entries
- `.agents/worklog/2026-08-30.md` — INC-11 entry
- `docs/design/rearchitecture/09-increment-plan.md` — execution table + INC-12 spec
- `AGENTS.md` — pipeline/build/test/commit conventions