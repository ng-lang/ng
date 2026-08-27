# Workstate

## Current focus

Executing the vNext increment plan (`docs/design/rearchitecture/09-increment-plan.md`) one
increment at a time: design → implement → verify (429-unit + conformance/PBT corpus, ASan
variant) → doc-sync → commit (feat + docs separately).

## Status

- INC-0 … INC-8, ARCH-1 … ARCH-4, PBT consolidation: committed.
- INC-A (prelude auto-loading): skipped by user decision (visibility-blind overload registration).
- INC-B (core traits → `prelude.ng`): committed `c1e733b`/`4040ecd`.
- INC-9 (error codes + did-you-mean): committed `e38e51c`/`55b815d`. Full suite green
  (489 cases / 2348 assertions, `build` + `build-asan`); ctest 490/490. ASan has one
  **pre-existing, unrelated** failure: imgui native link in `driver_test.cpp:178`.
- INC-10 + DEC-B (while/continue/break): implemented; full suite green (497 cases /
  3303 assertions); ctest 498/498. ASan same pre-existing imgui link failure only.

## Recent completed changes

- INC-B: Copy/Clone/Eq declared in `lib/std/prelude.ng` (`export trait …`); derive validator
  queries registered traits; `clone` keyword accepted as trait method name.
- INC-9: `code` field on error classes + driver emission (`<kind> error <code> at bytes […]`);
  9 specific codes (move/borrow/compare/unresolved-name); resolver Levenshtein≤2
  `did you mean` suggestions; INC-2 negative contracts assert a mandatory `code:` directive.

## Next steps

- INC-10 (with DEC-B): `while (cond)` desugars to loop+Branch; `continue` (while-only);
  `break` (loop/while); FlowIR LoopBreak/Jump-to-exit. DEC-B doc revision + counterexample
  tests (bare `next(args)` inside `while` must still resolve as a tail call).
- Do not chase the ASan imgui link failure in `driver_test.cpp:178` (pre-existing).

## Next steps

- INC-11 (derive(Show)): derive validator + synthesis for `show`; property tests and goldens.
- Follow INC-11 onward per `docs/design/rearchitecture/09-increment-plan.md`.

## References

- `.agents/worklog/2026-08-27.md` — INC-B / INC-A / INC-9 entries
- `docs/design/rearchitecture/09-increment-plan.md` — execution table + INC-10 spec
- `AGENTS.md` — pipeline/build/test/commit conventions