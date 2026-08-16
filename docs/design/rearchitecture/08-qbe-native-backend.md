# QBE Native Backend — Decision, Workflow, and Artifact Design

> **Status:** proposed. The toolchain-integration slice (§9) is implemented and
> green; the FlowIR → QBE lowering described in §3–§6 is not yet implemented.
>
> **AI-assisted document:** drafted with AI assistance. Evidence was read
> directly from the vendored copy (`vendored/qbe-1.3/doc/*`) and the current
> sources.

## 1. Question and decision

Should NG generate native programs through the vendored
[QBE](http://c9x.me/compile/) (`vendored/qbe-1.3`) instead of adopting LLVM,
targeting macOS and Linux (amd64 + arm64) first and deferring Windows?

**Decision: adopt QBE. Do not adopt LLVM at this tier.** LLVM's integration
cost (build system, ~100x code size, IR ceremony) buys optimizations and
debug-info infrastructure that this backend tier does not need yet. Revisit
LLVM only if a later performance tier demands `-O2`-class optimization; the
backend boundary chosen here (FlowIR, §3) keeps that option open.

### Terminology: the runtime owns the std modules

- **Runtime:** one owned build — `libng` plus its standard module libraries:
  `lib/std/*.ng` interfaces together with their C++ implementations.
  Generated executables link the language-side part (`ngrt`).
- **Native module:** the C++ implementation behind a std module interface —
  e.g. `lib/std/imgui.ng` backed by `registerImguiNatives`. A std module is
  part of the runtime, not a separate frontend: the transitional `ngi_imgui`
  executable folds into the default `ngi` registration.
- **Embedding API:** the (future) C++ surface for third-party hosts; an
  implementation detail of native modules, not a runtime category of its
  own.

Native calls take one of two paths **by construction**: AOT code calls
natives through declared C ABI shims at link time (no boxing, §5); the VM
path exchanges values through the boxed `Value`/`ValueView` boundary
([doc 02 §7.1](02-runtime-module-ffi.md)) because VM frames are boxed by
design. `NG::Value` therefore survives only for the VM path — the naive
per-call by-name lookup is what gets deleted.

## 2. Evidence: QBE vs LLVM

Primary sources read from the vendored copy: `doc/llvm.txt` (the author's own
comparison), `doc/il.txt` (IL specification), `doc/abi.txt` (SysV AMD64 ABI
subset), `doc/rv64.txt`, `doc/win.txt`, `doc/native_win.txt`, `LICENSE`,
`Makefile`, `main.c`.

| Dimension | QBE 1.3 (vendored) | LLVM | What it means for NG |
|---|---|---|---|
| Scope | "first 70% of performance in 10% of the code" — SSA, copy propagation, GVN, coalescing ([doc/llvm.txt](vendored/qbe-1.3/doc/llvm.txt)) | industry-grade optimizer | We need a correctness-first native tier with the VM as differential oracle, not peak optimization |
| C ABI | lowered **inside** the backend per target (`abi.c`, `amd64/sysv.c`, `arm64/abi.c`; documented subset in [doc/abi.txt](vendored/qbe-1.3/doc/abi.txt), fuzz-tested upstream) | frontend must reimplement ABI classification itself ([doc/llvm.txt](vendored/qbe-1.3/doc/llvm.txt): "reimplement large chunks of the ABI in its frontend") | R9 `extern "C"` / `repr(C)` (B3) gets struct classification for free — the flagship QBE feature |
| IL shape | small, readable; non-SSA temps allowed and fixed up by QBE ([doc/il.txt](vendored/qbe-1.3/doc/il.txt) §Phi); few types (`w l s d`, sub-word `sb ub sh uh`, aggregates) | verbose typed IR with loads/stores and casts | a FlowIR → QBE emitter stays small; generated IL is debuggable by hand |
| Aggregates | first-class IL types, but passed by pointer at IL level; backend classifies for the C ABI | alloca/load/store by hand | NG owns layout math; QBE does register classification at NG↔C boundaries |
| Variadic | portable `vastart`/`vaarg` ([doc/il.txt](vendored/qbe-1.3/doc/il.txt) §Variadic; base types only) | `va_arg` intrinsics | C varargs on both targets without per-arch work |
| Closures | `env` parameter invisible to C callers ([doc/il.txt](vendored/qbe-1.3/doc/il.txt) §Functions) | explicit environment structs | natural lowering for future closures/callbacks (R9) |
| TLS | `thread` / `extern thread` data ([doc/il.txt](vendored/qbe-1.3/doc/il.txt) §Data) | full TLS support | available for R10 later |
| Targets | amd64 (sysv + apple; `amd64_win` experimental, see [doc/native_win.txt](vendored/qbe-1.3/doc/native_win.txt)), arm64 (incl. apple), rv64 (struct-with-float ABI gap, [doc/rv64.txt](vendored/qbe-1.3/doc/rv64.txt)) | everything | covers macOS/Linux amd64+arm64; deferring Windows matches QBE's own experimental status there |
| Output | GNU/Apple assembly text; needs external assembler + linker | object code, JIT | pipeline includes a `cc` assemble+link step; no JIT ambitions now |
| Debug info | none | DWARF | known gap; R11 must add line tables ourselves (§10) |
| Embedding | standalone binary; subprocess (pipe IL text) — the model harec uses | in-process C++ library | `qbe` is spawned from the driver; no libqbe exists upstream |
| Size/build | <10 kloc C99, no dependencies, compiles in seconds ([doc/llvm.txt](vendored/qbe-1.3/doc/llvm.txt)) | massive dependency | vendored sources build via CMake in-tree (§9); self-hosting story stays light |
| License | MIT ([LICENSE](vendored/qbe-1.3/LICENSE), © 2015-2026 Quentin Carbonneaux) | Apache-2.0 with LLVM exception | both permissive; no concern |
| Ecosystem | production user: the Hare language (`harec`); cproc; maintained upstream (QBE 1.3 covered by [LWN](https://lwn.net/Articles/1080519/)) | universal | proven for a real statically-typed language, actively maintained |

What we must build ourselves **either way** (LLVM would not remove): NG type
layout from `TypeDescriptor`s, deep-copy/drop lowering, trait-view dispatch,
the runtime library, native registration, and (mostly) debug info.

## 3. Where the backend plugs in

```text
syntax → HIR → type check (typed side tables + monomorphized instances)
       → FlowIR → [new] QBE lowering → QBE IL (.ssa)
       → qbe (subprocess) → .s → cc → executable / object
```

FlowIR is the designed boundary: `include/flowir.hpp` documents it as "a new
backend-neutral IR", and
[01-target-architecture-hir.md](01-target-architecture-hir.md) §11–12
mandates that bytecode/native/WASM lowerings consume FlowIR plus ABI
descriptors and never re-derive semantic facts. The bytecode backend
(`src/bytecode.cpp`) is the reference lowering to mirror.

Proposed new components:

- `include/native/qbe_ir.hpp` + `src/native/qbe_ir.cpp` — QBE IL emitter.
- `include/native/lowering.hpp` + `src/native/lowering.cpp` — FlowIR → QBE IL:
  - FlowIR block parameters → QBE `phi` instructions (QBE jumps carry no
    arguments); alternatively non-SSA temps, which QBE fixes up itself
    ([doc/il.txt](vendored/qbe-1.3/doc/il.txt) §Phi) — phi is preferred since
    FlowIR values are already single-assignment.
  - Terminators: `Return` → `ret`; `Jump`/`Branch` → `jmp`/`jnz`;
    `LoopBackedge` → `jmp`; `TailRecur` → self-loop re-materialization (the VM
    already models tail recursion as a loop — mirror that lowering).
  - Calls: direct (`Call`) via symbol; `CallTrait` via the vtable globals
    emitted from `typecheck::traitViewTables`; natives via declared C shims (§5).
  - Drop edges: typecheck already emits return/fallthrough/block drop calls;
    they lower to ordinary calls to the drop descriptor.
- `include/native/layout.hpp` + `src/native/layout.cpp` — `TypeDescriptor` →
  QBE `type :...` aggregate declarations and memory layout (offsets/alignment)
  shared with the runtime library.
- `src/native/runtime/*.cpp` — `ngrt`, the runtime library (C++23, exposed
  through `extern "C"` entry points for generated code).
- `src/native/driver.cpp` — orchestration: emit `.ssa`, spawn `qbe`, spawn
  `cc`, run (§6).

## 4. Value representation strategy (tiered)

Constraints from the current implementation (`include/value.hpp`,
`src/vm.cpp`, `src/vm/value_ops.cpp`, [04-language-decisions.md](04-language-decisions.md)):
copy-first deep-copy (D-015); `ref`/`ref mut` are scope-local and cannot
escape (the borrow checker rejects escapes); trait views are
(root cell, steps, trait id, concrete id) with per-(trait, concrete) vtables;
no GC (B1 heap domains deferred); drop calls are already in FlowIR.

**Tier 0 — boxed, semantic parity first.** Every FlowIR value has a uniform
representation (a tagged heap box) and every FlowIR instruction lowers to a
call into `ngrt` mirroring `vm::detail` semantics one-to-one. Goals: exact VM
parity and differential-testability; not fast. This proves the pipeline, the
toolchain, natives, and the harness before any layout sophistication.

**Tier 1 — typed unboxing.** Per monomorphized function, values use concrete
`TypeDescriptor` layouts:

| NG value | Native representation |
|---|---|
| `i8..i64`/`u8..u64`, `f32`/`f64` | QBE `w l` / `s d`; sub-word `sb ub sh uh` only at C boundaries |
| `bool`, `unit` | `w` (0/1, unused) — matches VM truthiness |
| struct / tuple / fixed array | QBE aggregate type by value (pointer-passed inside IL; QBE classifies at NG↔C boundaries) |
| enum | `{ tag: w, payload: union of variant payloads }`, max-variant sized; recursive payloads through pointers |
| `string` | `{ ptr, len }` heap buffer; deep copy = `memcpy` on bind (no refcounting needed under copy-first semantics) |
| dynamic `array<T>` | `{ ptr, len, cap }` owned buffer; append/slice → `ngrt` ops |
| `range` | `{ start, end }` pair |
| `ref` / `ref mut` | raw pointer into the local's **stack cell** — sound because refs cannot escape; any borrowed local becomes a stack slot |
| `ref<Trait>` (trait view) | `{ data ptr, vtable ptr }`; vtable = emitted global table; `CallTrait` → indirect call |
| opaque handle | `uintptr_t` token per its declared handle descriptor (doc 02 §5.3) |

**Internal NG calling convention:** reuse the target's C ABI through QBE with
NG-layout argument types. `extern "C"` on ABI-safe signatures then becomes
free (doc 02 §7.2), and `repr(C)` verification (B3) shares the layout pass.

## 5. Native/ABI: replacing the naive layer

The current native mechanism is deliberately naive, and the native backend is
the forcing function to replace it:

- `vm::NativeRegistry` is a name → `std::function<Value(vector<Value>,
  vector<TypeId>)>` map; the VM resolves natives **by name at every call**
  (`src/vm.cpp` `Call` path), deep-copies every argument into a `vector<Value>`
  per call, and signatures are recovered ad hoc from `localTypes`.
- Native placeholders in bytecode carry no declared signature
  (`nativeFunction` placeholders in `src/driver.cpp`).
- Opaque values are bare `uint64_t` tokens (`value.hpp`), and heap handles live
  in a process-global `shared_ptr<vector<optional<Value>>>` (`heapSlots` in
  `src/driver.cpp`) — violating invariants 7 and 11.
- `ConstNativeHost` duplicates the string natives with a second signature —
  two parallel registries that can drift.

**Verdict: yes — rewrite it as the declared-descriptor ABI of
[02-runtime-module-ffi.md](02-runtime-module-ffi.md) §7.1/§9 (R9), and do it
as a staged unification, not a flag-day deletion.** The VM remains an
execution engine (differential oracle, `--fuel` interactive mode, imgui
binding), so one declared registry must serve both backends:

| Concern | Naive today | Target |
|---|---|---|
| native call from VM | by-name lookup per call | declared registry, resolved at registration; typed `ValueView` adapter |
| native call from native code | impossible | `extern "C"` shim symbol from the descriptor; link-time resolution |
| signature | recovered from `localTypes` | `CallableDescriptor` / `FunctionTypeIds` authoritative |
| marshaling | `vector<Value>` + deepCopy per call | typed `ValueView` + descriptor copy/move/drop ops; unboxed in Tier 1 |
| opaque handles | `uint64_t` token + process-global slots | declared handle descriptors with ownership policy (doc 02 §5.3) |
| const natives | duplicate `ConstNativeHost` | one declared surface; a pure-capability flag admits const evaluation (A6) |
| user `extern "C"` | unsupported | direct lowering through QBE's C ABI (`abi.c`), gated by ABI-safe type rules (doc 02 §7.2) |
| heap/global state | process-global `heapSlots` | `RuntimeSession`-owned (R6) |

Order of work: (1) the declared-descriptor registry serving VM + const +
native paths (no behavior change for VM; migrate registrations
incrementally — string/io natives first, `imgui_natives`/`runNgi` last);
(2) native codegen consumes the same descriptors and emits the `extern "C"`
shims; (3) delete the by-name `std::function` path once differential tests
pass. What survives: `NG::Value` — but only as the **VM-path and
host-embedding exchange representation** (doc 02 §7.1), never on the AOT hot
path, where native calls resolve to link-time C ABI shims; the VM itself; and
the bytecode artifact format. `ngrt`'s boxed tier (Tier 0, §4) is a lowering
strategy, not a host interface.

**Std modules and opaque handles (decided):** `std.imgui` is a standard
library, not a separate frontend: `lib/std/imgui.ng` is the NG surface and
its C++ implementation is a native module built and registered as part of
the ng runtime (the transitional `ngi_imgui` executable is folded away).
Foreign C++ state (imgui contexts, device handles) appears in NG only as
abstract/opaque types — `type X = native;` today, declared handle
descriptors in R9 ([doc 02 §5.3/§8](02-runtime-module-ffi.md)): NG code can
pass them around and hand them to native APIs, but never inspects them. In
AOT code an opaque handle is a `uintptr_t` token passed through untouched.

## 6. Workflow and final generated results

Driver UX (proposed; VM remains the default):

```bash
ngi example/hello.ng                # default: VM (unchanged)
ngi --native example/hello.ng       # AOT: emit exe, assemble+link via cc
ngi --native --emit=ssa file.ng     # dump QBE IL (.ssa) only
ngi --native --emit=asm file.ng     # stop after qbe (.s)
ngi --native --emit=obj file.ng     # stop after cc -c (.o)
ngi --native --target arm64 ...     # qbe -t arm64 (cross-emit; linking needs a cross toolchain)
```

Stages:

1. **Front (unchanged):** resolve → check → monomorphize → FlowIR.
2. **QBE lowering:** FlowIR + type descriptors + vtables → `.ssa` text.
3. **`qbe` subprocess:** `./build/qbe` (via the `NG_QBE_PATH` compile
   definition, §9) → GNU/Apple assembly.
4. **`cc` subprocess:** assemble + link the runtime archive (`libngrt`) and
   the host runtime for natives → executable (Mach-O / ELF).
5. **Run** if requested; artifact caching arrives with R7.

Final generated results:

- **primary:** a native executable (Mach-O on macOS, ELF on Linux);
- **on demand:** `.ssa` (QBE IL), `.s` (assembly), `.o` (object) intermediates;
- **runtime:** `libngrt` (static archive from `src/native/runtime`, linked
  into every executable);
- **later:** exported-function/vtable tables as data sections for
  `ModuleArtifact`/`ModuleInstance` (R6/R7).

## 7. Testing strategy

- **Differential (delivered):** `test/native_differential_test.cpp` runs
  every `example/*.ng` through the VM and through `--native` and asserts the
  VM's main return value (low 8 bits) equals the native exit code; a hidden
  `[.benchmark]` case times fib under both tiers. The sweep caught and drove
  the fixes for: QBE's all-phis-first rule, shell signal collapse (the
  runner now fork/execs directly), escaping refs in recursive enums (cell
  locals), copy-first deep-copy-on-bind, and constant-index place steps
  (header-aware offsets).
- **Toolchain:** `qbe_smoke` CTest (implemented, §9); the upstream
  `tools/test.sh` suite as a manual gate for the vendored copy.
- **Unit:** one lowering test per FlowIR instruction; layout tests asserting
  offsets/alignment against compiled C fixtures (required before B3).
- **Natives:** the same descriptor-driven tests as the VM path, plus ABI
  round-trip tests through C once `extern "C"` lands.

## 8. Milestones

- **M0 (implemented this round):** vendored `qbe` built by CMake + toolchain
  smoke test (§9).
- **M1 (delivered):** QBE IL emitter (`include/native/lowering.hpp`,
  `src/native/lowering.cpp`) lowers FlowIR to `.ssa`; `ngi --emit=ssa`
  prints the module IL. Delivered scope: scalar literals (int/bool/float),
  prefix/binary arithmetic and comparisons with mixed int/float promotion,
  locals as stack slots, block parameters as phi instructions, plain local
  assignment, and return/branch/jump/loop-backedge/tail-recur terminators.
  Tests: `test/native_qbe_test.cpp`, incl. an end-to-end round trip
  (emit → `qbe` → `cc` → run, exit code checked). Unsupported constructs
  fail with `LoweringError`; calls, aggregates, refs, and trait dispatch
  arrive in M2/M3.
- **M2 (in progress — calls and Tier 0 aggregates delivered):** Tier 0
  lowering of the full FlowIR instruction set + `ngrt` core; differential
  tests on scalar/branch/loop examples. Delivered: direct calls with
  collision-free symbols (`<sanitized-name>_<defid>`; `main` keeps its
  C-runtime name), recursion, and NG `next f(...)` tail recursion lowered
  to a loop (the one-shot entry logic lives under `@start`, the loop jumps
  back to `@body0` — QBE forbids jumping to `@start`). Also delivered: Tier
  0 aggregates as 8-byte pointers into malloc'd objects — strings
  (`{ len, bytes }`, literals as QBE `data` items), arrays and tuples
  (`{ len, cap, 8-byte elements }`), ranges (`{ start, end }`); string
  concat/content equality, literal construction, bounds-checked indexing
  (`hlt` on violation), `<<` append, and range slicing. The ngrt helpers
  (`$ngrt_str_concat`, `$ngrt_str_eq`, `$ngrt_arr_get`,
  `$ngrt_arr_append`, `$ngrt_arr_slice`) are emitted as IL once per module
  on first use and call libc `malloc`/`memcpy`/`memcmp` through QBE's C
  ABI — no external runtime archive yet. Also delivered: structs (Tier 0
  field layout `offset = 8 * ordinal`, literals, member reads, in-place
  field mutation through static place paths) and scope-local `ref` /
  `ref mut` as `{ root-slot, count, steps[] }` objects with
  `$ngrt_ref_load`/`$ngrt_ref_addr` helpers mirroring the VM's (cell +
  path) view semantics — rebinding the root is observed and writes flow
  through the referenced place. Delivered in the M2 close-out: copy-first
  deep copy at bind/load sites (static type-driven: structs clone
  field-by-field, tuples unroll statically, arrays use per-element-kind
  helpers, strings use `$ngrt_str_clone`); borrowed locals become heap
  cells with slot → cell indirection, so BindLocal rebinds a fresh cell
  (VM shared-cell snapshot semantics) while `:=` mutates the current cell
  (outstanding refs observe it); constant indexes in place paths are
  normalized to header-aware offsets (FlowIR encodes them as Member steps,
  matching the VM's `walkStep`). Known gap:
  monomorphized instance bodies can carry the generic type-parameter id in
  value/local type tables (`*a` types as the reference's element, which
  stays `T` until `TypeInterner::specialize` substitutes it); the lowering
  falls back to `l` for type parameters — QBE rejects float-context misuse
  loudly, and the proper fix is substitution-aware typing in the
  typechecker. Also delivered: enums as `{ tag, payload-word }` objects —
  construction (single- and multi-field; the latter through tuple splicing,
  now supported with a static type-directed splice), tag loads, payload
  extraction, and variant switches (which lower to tag compares +
  extraction). Recursive `List<T>` style enums work end to end including
  `ref` payloads and switch-recursive walks; the `$ngrt_enum_list_len`/
  `$ngrt_enum_list_get` helpers are implemented for the VM-parity
  `EnumList*` instructions but currently unreachable from the typechecker.
  Also delivered: `ref<Trait>` dynamic dispatch — a trait view is
  `{ ref-object, trait-id, concrete-id }`, vtables become per-module `data`
  items (`{ l method-symbol, ... }`) with a linear key -> vtable lookup
  helper, and `CallTrait` performs an indirect QBE call passing the view's
  ref object as `Self ref` (so method bodies observe root rebinding like
  the VM). Polymorphic view arrays dispatch per element. Remaining: native
  shims, `ngrt` as a linked archive, unions, ref-rooted view/place paths,
  deep-copy-on-bind for aggregates.
- **M3 (delivered):** `ngi --native` compiles, links, and runs an
  executable end to end (emit → qbe → cc → run; per-process temp dir);
  `main` maps to the C entry point (unit returns become exit code 0). The
  `libngrt` archive is superseded: ngrt helpers stay as emitted IL calling
  libc directly, so executables need no host runtime until native shims
  (M5). 24/45 example programs run natively (folds, ref_places, traits,
  trait_objects, enums, recursion, …); the rest were blocked on native
  shims, unions, and opaque types at that point (all but two resolved by
  the M5 shim slice below).
  Delivered alongside: ref-rooted place paths (`(*self).field := ...`),
  halt traps (`$ngrt_panic`) for VM-rejected unresolved trait-slot calls in
  dead generic originals, and type-constructor fallbacks.
- **M4 (first slice delivered):** SSA locals — `let`-bound locals that are
  never borrowed, assigned, or a block parameter (plus unassigned function
  parameters) live directly as QBE temps instead of stack slots; QBE's
  non-SSA fixup handles rebinding and cross-block uses. Measured:
  fib(26) native executable ≈ 4 ms user time vs ≈ 0.8 s for the VM
  interpreter (≈ 200x); the `--native` wall time is dominated by the
  qbe+cc toolchain. Delivered alongside: string/float `main` results are
  printed and exit 0 (no meaningful exit code), and a hidden
  `[.benchmark]` suite (fib / loop / string concat / list walk) times the
  VM against the compiled executable directly — fib(22) ≈ 30x, loop
  200000 ≈ 17x (the native column includes ~0.3 ms fork/exec startup per
  run, so startup-dominated micro-programs show no gain). Delivered next:
  **structs by value** — every struct type gets a QBE aggregate type
  (`type :ngs_<id> = { l, ... }`, all members 8-byte words in this slice);
  literals and unborrowed locals live in stack slots (`alloc8`), calls and
  returns use the aggregate ABI (small structs pass in registers via QBE's
  classification), and copy-first semantics are field-wise copies — QBE's
  `blit` is unusable (its optimizer folds blitted data to an uninitialized
  sentinel, observed empirically). Borrowed structs stay heap-resident
  (cell locals hold object pointers), so escaping refs remain valid.
  Delivered next: **enums by value** — `type :nge_<id> = { l, l }`
  (tag + payload word), stack literals, aggregate-typed parameters
  (register passing), and heap-cloned aggregate returns (`l` signatures:
  callee stack slots must not outlive frames, and QBE's aggregate return
  areas interact poorly with recursive aggregate traffic — observed
  empirically). Cell/ref assignments clone aggregates onto the heap so
  loop-rebuilt stack literals cannot alias into cells.
  Remaining M4: sub-word/float member layout (f32 as `s`), real
  alignment-aware offsets.
- **M5 (first slice delivered):** AOT shims for the standard natives —
  `libngrt` (`src/native/ngrt_shims.c`, pure C99, layouts matching the
  Tier 0 representations) is linked into every `--native` executable, and
  the lowering emits per-call-site shim calls keyed by native name plus
  static signature: `print` (i64/u64/f64/bool/string), `assert`, the
  string ops (length/charAt/substring/trim/toUpper/toLower/contains/
  startsWith/endsWith/replace/split/join/regexMatch), seq ops
  (len/sum/arrayContains/reverse), memory handles
  (allocate/load/store/release/outstanding), file/line I/O, and cwd.
  Coverage: **44/45 examples run natively** — the only remaining one is
  the host-bound imgui binding (recorded decision: R9 std-module work).
  Union types are delivered as tagged `{ member-index, payload }` boxes
  (wrapping at production sites, equality/ordering comparisons; ordering
  compares raw payload words — a documented Tier 0 deviation from the VM's
  value-nature checks). regexMatch uses POSIX ERE, a documented deviation
  from the VM's std::regex. R9 first slice delivered: every `native fun`
  declaration's signature (parameter types + result) is derived from the
  checker's `FunctionTypeIds`, attached to the registry
  (`NativeRegistry::declare`), validated for arity at VM call time, passed
  as authoritative type guidance to handlers, and used by the native tier
  for declaration-driven shim selection (`print(u8)` selects the unsigned
  shim). Remaining M5: capability/ownership descriptors on signatures,
  and `extern "C"`/`repr(C)` (B3, gated on R4/R6/R7).- **Later:** cross-target linking, debug info (R11), Windows when QBE's
  `amd64_win` matures.

## 9. Build integration (implemented)

- `cmake/qbe.cmake` builds the `qbe` executable from `vendored/qbe-1.3` with
  upstream Makefile parity: the same source list (all three targets compiled
  into one binary, selected at runtime with `-t`), a generated `config.h`
  (`Deftgt` per host platform), and the same warning flags. The binary lands
  at the build root next to `ngi`. Missing vendored sources degrade to a
  status message (`NG_QBE_AVAILABLE=FALSE`).
- Root `CMakeLists.txt`: `include(cmake/qbe.cmake)`; the `ng` library gets
  `NG_QBE_PATH="$<TARGET_FILE:qbe>"` as a compile definition plus a build
  ordering dependency.
- CTest: `qbe_smoke` (`test/native/qbe_smoke.ssa` +
  `cmake/qbe_smoke.cmake`) runs the exact native pipeline — IL → `qbe` →
  `cc` → executable → verified output ("fib(10) = 55", which also exercises a
  C variadic call through QBE's ABI lowering).
- Verified locally: `./build/qbe -t '?'` reports `arm64_apple`; the full
  suite is green (509/509).

**Repository decision (made):** `vendored/qbe-1.3` is vendored as a plain
git tree — no submodule. Upstream: `git://c9x.me/qbe.git`, tag `1.3`
(GitHub mirror `8l/qbe`).

## 10. Risks, gaps, and open questions

- **No debug info from QBE:** native backtraces/line tables require our own
  emission — fold into R11; not a blocker for M1–M3.
- **rv64 ABI gap** ([doc/rv64.txt](vendored/qbe-1.3/doc/rv64.txt)): structs
  with floats — irrelevant until RISC-V support is claimed.
- **We own layout math** (QBE aggregates are pointer-passed at IL level):
  mitigated by the shared layout pass + C fixture tests.
- **Tail calls:** QBE has no tail-call guarantee. Self tail-recursion lowers
  to loops (as the VM models it); general tail calls become calls. This is a
  documented semantic difference from the VM's tail-recursion counters.
- **Checked arithmetic (decided and delivered):** QBE has no built-in
  overflow instructions, so the lowering emits checked helper calls
  (`$ngrt_checked_add/sub/mul/div/rem/neg/shl/shr` — compare-based overflow
  detection, division-by-zero and MIN/-1 guards, shift-count range checks,
  halting on violation) plus D-008 narrow-width range checks
  (`$ngrt_check_w_i8`...`u32`) after integer results — neither the VM nor
  QBE changes, and VM overflow errors remain the oracle semantics (both
  tiers fail loudly; the VM reports, the native tier halts). A
  `--release` wraparound mode remains a separate, later language decision.
- **Subprocess integration** is the sanctioned QBE model (no library
  interface upstream); pipe vs temp file is an implementation detail.
- **Decided (owner):** (1) VM remains the default `ngi` mode after the
  backend ships; (2) native `main` maps `argv` → `[string]` parameters and
  the process exit code comes from the return value; (3) the first
  milestone links `libng` (the C++ host runtime) for natives, with a later
  C-only shim layer to shrink the distribution footprint.
- **Remaining open:** (1) native-executable panic/error policy (abort vs
  exit code vs handler); (2) artifact caching integration with R7.
