# Workstate

## Current focus

Post-cutover maintenance of the vNext pipeline: keeping the native (QBE) backend link path portable across macOS and Linux toolchains.

## Status

- Full suite green on both platforms: 455 test cases / 1940 assertions, verified locally (macOS arm64) and in a Docker replica of the GitHub ubuntu-24.04 + clang-20 runner.
- Linux CI failure in "compiles and links an imgui program without running it" is fixed: `libngrt.a` now links last so GNU ld's one-pass archive resolution sees `libngrt_imgui.a`'s callbacks.
- Driver imgui-link test now surfaces driver status/output/errors via Catch2 `INFO` on failure.

## Recent completed changes

- `src/driver.cpp` — native link order fix (`libngrt.a` after dependent archives) plus missing `<format>` include.
- `src/hir.cpp` — missing `<algorithm>` include (latent portability bug under libstdc++).
- `test/driver_test.cpp` — failure diagnostics for the imgui compile/link case.

## Next steps (user-gated)

- Consider a Linux pre-push check (container or second CI job/arch) to catch linker/platform-sensitive changes before push.
- Extend the `INFO(...)` diagnostics pattern to other driver tests that capture but never print `errors`.
- Continue 08/QBE backend open items (panic policy, artifact caching, remaining B3 slices); R6/R9 prerequisites per `07-post-cutover-plan.md`.

## References

- `.agents/worklog/2026-08-25.md` — CI fix entry and postmortem
- `docs/design/rearchitecture/08-qbe-native-backend.md` — QBE native backend status
- `docs/design/rearchitecture/07-post-cutover-plan.md` — remaining work ladder
- `AGENTS.md` — pipeline/build/test conventions
