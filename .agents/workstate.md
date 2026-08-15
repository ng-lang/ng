# Workstate

## Current focus

Documentation sync only. The user asked to pause new tasks; align docs and record state via the worklog/align skills.

## Status

- vNext migration goal: **complete** (marked in goal tools; see `goal-2b0550d8-4094-4168-9cd3-06658ac747e5`).
- Full suite green: 1757 assertions / 437 test cases, ctest 100%; 45 examples run e2e (`test/examples_sweep_test.cpp`).
- Docs phase complete: guide rewritten, ref guides updated, legacy design/review docs archived (`docs/design/archive/`, untracked + gitignored), VitePress design content removed.

## Recent completed changes

- `fe8089f` list spreads into array literals (legacy 59 `[...items]`; EnumListLength/EnumListGet opcodes)
- `0c2032d` list collection literals (`let xs: List<i64> = [1,2,3];`)
- `8deca5a`/`7b977c9`/`422bc56` docs: guide/ref rewrites, archive + vitepress + gitignore

## Next steps (user-gated)

- Review rewritten `docs/guide` and the deferred-item plan.
- Candidate continuations per `07-post-cutover-plan.md`: R6/R9 prerequisites (heap domain, C ABI) or a repo-wide clang-format pass.
- Align: CHANGELOG + root README still describe the legacy pipeline (flagged for update).

## References

- `docs/design/rearchitecture/07-post-cutover-plan.md` — remaining work ladder
- `docs/design/rearchitecture/05-legacy-example-migration-matrix.md` — coverage matrix
- `AGENTS.md` — pipeline/build/test conventions
