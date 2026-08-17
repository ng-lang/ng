# Workstate

## Current focus

Continuing the QBE native backend (`docs/design/rearchitecture/08-qbe-native-backend.md`): native binary output (`--output`) and M5 ownership descriptors.

## Status

- vNext migration goal: **complete** (marked in goal tools; see `goal-2b0550d8-4094-4168-9cd3-06658ac747e5`).
- Full suite green: 561/561 ctest, including the native differential sweep, ownership-descriptor tests, and native `--output` tests.
- M5 (AOT native shims + declared native signatures) is complete: `DeclaredSignature` carries per-parameter `Ownership` (`Copy`/`Borrow`/`Move`) and result ownership; AOT lowering deep-copies `Copy` aggregate arguments before shim calls.
- `ngi --native --output <path>` now writes a native executable without running it.

## Recent completed changes

- `src/driver.cpp`, `test/driver_test.cpp`, `test/native_qbe_test.cpp` — added `--output` / `-o` native executable output.
- `docs/design/rearchitecture/08-qbe-native-backend.md` — documented `--output`, M5 delivered.
- `include/native.hpp`, `include/native/lowering.hpp`, `src/driver.cpp`, `src/native/lowering.cpp` — ownership descriptors added and wired into AOT shim argument lowering.
- `test/native_function_test.cpp`, `test/native_qbe_test.cpp` — ownership descriptor and deep-copy regression coverage.

## Next steps (user-gated)

- Continue 08/QBE backend open items (e.g. panic policy, artifact caching, remaining B3 slices).
- Per `07-post-cutover-plan.md`: R6/R9 prerequisites (heap domain, C ABI) or a repo-wide clang-format pass.
- Align: CHANGELOG + root README still describe the legacy pipeline (flagged for update).

## References

- `docs/design/rearchitecture/08-qbe-native-backend.md` — QBE native backend status and milestones
- `docs/design/rearchitecture/07-post-cutover-plan.md` — remaining work ladder
- `docs/design/rearchitecture/05-legacy-example-migration-matrix.md` — coverage matrix
- `AGENTS.md` — pipeline/build/test conventions
