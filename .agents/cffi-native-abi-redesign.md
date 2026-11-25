# CFFI and native ABI redesign

Status: design draft. This document is an implementation plan based on the current vNext compiler. It separates observed behavior from proposed decisions and implementation stages. It does not change the language or implementation by itself. Confirm the open decisions below before treating the proposals as language contracts.

## Goal and proposed boundary

NG owns NG values and their semantics. A direct native call crosses a C ABI boundary using only explicitly ABI-safe scalar values, pointers/opaque handles, and explicitly C-layout records. A native implementation may be written in C or C++ with `extern "C"`; it must not inspect or return an NG-managed string, array, tuple, closure, trait view, or ordinary NG struct/enum.

The intended declaration forms are:

```ng
type Handle = native "void*";

// Generated symbol: ng_<module-id>_malloc
export native fun malloc(size: usize) -> Handle;

// Exact existing linker symbol: malloc
native "malloc" fun malloc(size: usize) -> Handle;
```

The first form uses a deterministic, documented C-compatible symbol encoding of the module's logical identity and function name. The second form uses the literal link name unchanged. Neither form creates a C++ wrapper. `export` continues to mean visible to importing NG modules; it does not change the C linker symbol's visibility.

For the first implementation, direct native declarations should be non-generic and non-overloaded. Explicit link names should also be unique within an NG module's callable declarations. `extern "C"` remains the syntax for declaring arbitrary foreign symbols and may share the same ABI checker and lowering path. Do not remove it as incidental cleanup.

`native "void*"` is a nominal opaque pointer representation, not an arbitrary C declarator parser. QBE represents a data pointer as `l`. Start with a small canonical set of native representations needed by real APIs (`void*`, and if needed a const byte pointer), and reject unsupported strings. C record-by-value declarations continue to use `repr(C) struct` with a compiler-known field layout. A textual `"struct Foo"` alone is insufficient to describe a QBE aggregate.

## QBE and linker signature checking

**Finding:** QBE does not provide cross-object C prototype checking. In the vendored QBE 1.3 source, call argument/result classes are carried on each call instruction (`parse.c`, `parserefl` and call parsing); an unresolved global reference has a symbol name and relocation, not a C prototype. Function parameter classes are recorded on the defined function separately. The external C object is not available to QBE as typed declaration metadata. Aggregate call operands can name a QBE `type`, but this still only describes the caller's chosen ABI layout.

The linker resolves symbol names and relocations. It generally cannot report that a declaration of `malloc(size_t) -> void*` was mistakenly written as `malloc(double) -> i64`, because C object symbol tables do not carry an enforced C function type. The linker can report a missing symbol (or some platform-specific symbol/architecture errors), not a reliable signature mismatch.

Consequences:

- NG typechecking can guarantee that NG call sites agree with their selected NG declaration and that the declaration belongs to the supported ABI subset.
- QBE verifies the IL is well-formed and lowers the ABI classes written at each call site; it cannot verify the real foreign prototype.
- Link failure is the expected outcome for a missing explicit link name, but must not be described as signature validation.
- Signature agreement with a foreign implementation is an FFI contract. Test repository-owned implementations with matching C/C++ prototypes and runtime ABI fixtures. Do not claim the compiler detects a mismatch in third-party C libraries.
- Do not add an NG-side registry keyed only by symbol name as a substitute for external prototype checking. It can catch inconsistent NG declarations in one compilation, but cannot prove the linked implementation matches.

Evidence: `vendored/qbe-1.3/parse.c` parses call arguments with classes at the call site; `vendored/qbe-1.3/all.h` stores the defined function return type on `Fn`; the driver invokes QBE to produce assembly, then invokes `c++` to link in `src/driver.cpp`. QBE does not receive the C headers used to compile a library.

## Current implementation inventory

### FFI syntax, checking, and lowering

- `include/syntax/ast.hpp`: `FunctionDeclaration` has mutually exclusive `nativeFunction` and `externC` flags. `OpaqueTypeDeclaration` currently records only `abstract`, generic parameters, and pack parameter; it has no native representation string.
- `src/syntax/module_parser.cpp`: recognizes `native fun`, `extern "C" fun`, C import blocks, and `type T = native;`. Native fun declarations have no body. The parser must add optional link-name parsing both for standalone and `export native` forms, and a native representation on opaque declarations.
- `src/hir.cpp`, `include/hir.hpp`: syntax declarations become HIR functions and `hir::OpaqueType`; both need to preserve link name / representation. Imported items receive `originModule` indexes, but the loader does not currently preserve a stable logical module ID on HIR declarations.
- `src/module_loader.cpp`: canonical paths are used for deduplication, while `originModule` is a traversal index. Traversal indexes must not become ABI names because import order can change them. Add a stable logical module identity derived from the import identity (and define an entry-module identity), or choose an explicit module declaration before implementation. Never emit canonical absolute paths into generated symbols.
- `src/typecheck_register.cpp`: `validateExternSignature` / `abiSafetyIssue` accept fixed-width integers, floats, bool, callbacks of scalar types, nullable native opaque handles, and `repr(C)` structs. Native fun currently goes through more permissive checking and accepts NG-owned values. Introduce one explicit native ABI classifier and use it for all direct native declarations; preserve any intentionally wider existing `extern "C"` features only if their lowering remains sound.
- `src/native/lowering.cpp`, `src/native/lowering_call.cpp`: native calls currently target special `$ngrt_*` names, with type-specific exceptions and aggregate copy shims. `extern "C"` calls use the source function name and C argument/result classes. Unify the direct ABI call path around the stored link name or generated native symbol, declared parameter types, result type, ownership, and variadic metadata. Keep internal NG function symbol naming independent.
- `src/native/function_lowerer.hpp`: ordinary NG functions are named using sanitized source name plus DefId; `main` is `$main`. Native default symbol generation must use stable module identity plus source name, not DefId or loader order.
- `src/driver.cpp`: links the generated assembly with `libngrt_helpers.a`, `libngrt.a`, and optional ImGui archives. It currently always requires libngrt; after migration this should become dependency-driven, and a program using only C `malloc/free` plus generated code should not need the legacy stdlib C runtime archive.
- `docs/ref/language-decisions.md` D-004 currently says `native fun` is the NG runtime-native surface and `extern "C"` is foreign. Update only once the implementation contract is accepted and implemented.

### Type and aggregate representations

- `TypeKind::Opaque` in `include/typecheck.hpp` distinguishes abstract types from opaque native handles with an `abstractType` bit. Extend the native case with a canonical representation kind and ABI class. Do not make the parser's raw string the backend's permanent type system.
- `qtypeOf` in `src/native/function_lowerer.hpp` currently maps `Opaque`, nullable, arrays, tuples, ranges, refs, trait refs, closures, and ordinary aggregates to `l`; ordinary structs/enums are pointer-backed NG values. This is not the same as a C aggregate passed by value.
- `src/native/lowering.cpp` emits QBE aggregate declarations for struct/enum `TypeDescriptor`s. Current NG aggregate declarations use NG layouts (ordinary structs' fields become QBE field classes and enums are `{ l, l }`); ordinary aggregate values are generally represented as heap pointers in NG storage operations. `repr(C)` and `extern "C"` already have a special aggregate-by-value path in `src/native/lowering_call.cpp`.
- Preserve the distinction between (1) NG-owned aggregate values represented according to language semantics, (2) C ABI records described by `repr(C)`, and (3) opaque native pointers. Never infer C layout for an ordinary NG struct or enum.
- Rework QBE type emission so each C ABI record has a single stable QBE type declaration with exact field widths, ordering, padding/alignment rules, and a C-compatible name. Ensure the call site's `:type` and the emitted data layout refer to the same descriptor. Check nested `repr(C)` fields recursively and reject unsupported fields, generics, enums, references, arrays, and zero-sized records until their ABI is specified.
- QBE's `l` means a machine word in the supported current 64-bit targets; pointer and `usize/isize` ABI support should state supported targets. If 32-bit targets are later enabled, the ABI classifier must become target-specific.
- Ordinary NG struct/enum copy, move, drop, equality, and field access remain language operations. A C record does not automatically gain NG `Clone`, `Drop`, or heap ownership semantics.

### Runtime and standard-library dependencies

`src/native/libngrt.c` currently combines several unrelated concerns: allocator/arena and errors; string layout and all string helpers; stdlib printing/conversion/parsing; arrays/sequence helpers; opaque memory handle table; IO/filesystem/process helpers; test C fixtures. `include/ngrt.h` declares that surface. `include/ngrt.hpp` / `src/ngrt.cpp` adapt between `std::string`, `std::vector<Value>`, and runtime values for const evaluation / host execution. `src/native/helpers/*.ssa` is a separate QBE helper archive for string, array, ref, span, and checked arithmetic. `src/native/imgui_ngrt.cpp` is a C++ host binding with C-linkage entry points and currently converts the NG length-prefixed string representation.

The implementation must not equate “remove wrappers” with deleting every helper. Separate these dependencies:

1. **NG value operations:** move string/array/generic Box and Cell behavior into NG plus compiler-generated QBE operations. These must not depend on `NG::Value`, `std::string`, or `std::vector<Value>`.
2. **OS / libc boundary:** retain only narrow operations that cannot reasonably be implemented in portable NG (allocation, byte IO, process launch, GUI), with C ABI scalar/pointer/length signatures and explicit buffer ownership.
3. **QBE helpers:** keep or gradually inline pure QBE helpers such as checked arithmetic and low-level array address/bounds routines. Rename only if there is a real ABI or ownership reason; the `ngrt_` prefix is internal today.
4. **Compile-time evaluation:** const-native host implementations are a separate compile-time capability. Migrate or remove them explicitly; do not accidentally call runtime C APIs through `std::string` wrappers after runtime FFI has changed.
5. **Driver linkage:** link only archives needed by actual direct native symbols/runtime features. A missing explicitly named symbol should reach the linker and produce an understandable link error.

Stdlib migration inventory:

| Module | Current native surface | Proposed implementation |
| --- | --- | --- |
| `lib/std/prelude.ng` | print overloads, assert, numeric conversions, `toString`, integer parsing are native; `Option`, `Result`, combinators, `not`, traits are NG | Keep language combinators. Implement `assert`, integer conversions/parsing, and formatting in NG where feasible. Provide tiny raw output/format libc bindings only with scalar or pointer+length ABI. Never expose NG `string` to a C implementation. |
| `lib/std/string.ng` | Every string operation is native and implemented in `libngrt.c` | Implement utilities in NG over the new string/byte representation. Establish UTF-8 semantics before migration: valid-UTF-8 invariant, byte length vs scalar length, indexing/slicing units, invalid input behavior, and whether case conversion is ASCII-only or Unicode. Avoid claiming grapheme operations or normalization in the first slice. |
| `lib/std/io.ng` | Native `readLine/readFile/writeFile/currentExecutablePath` accept/return NG strings through `libngrt` | Keep OS interaction behind raw byte buffer + length APIs. NG converts path/text values to/from UTF-8 buffers. Define allocation ownership, error reporting, embedded NUL handling, and partial-read behavior. |
| `lib/std/seq.ng` | `len<T>`, primitive helpers and generic `pushBack` are native; map/filter/fold are already NG | Move array length, reverse, sum/contains, and push behavior into NG/compiler-generated operations. Preserve copy-on-write/unique mutation semantics. `map/filter/fold` stay NG. |
| `lib/std/memory.ng` | `Handle = native` token plus generic C `allocate<T>(value)`, `load<T>`, `store<T>`, `release` cell table; `Box<T>`/`Cell<T>` wrappers are NG | Replace the fake generic native ABI with opaque pointer allocation/deallocation primitives (e.g. `malloc/free`) and NG/compiler-generated typed load/store/copy/drop. Resolve generic size/alignment and indirect aggregate value representation before implementing. Keep Box/Cell and Drop impls in NG. Remove live-token table once no longer needed; retain `outstanding` only if made a deliberate debug API. |
| `lib/std/system.ng` | Native command-string and output-string APIs implemented by shell helpers in `libngrt.c` | Decide whether shell execution is part of supported stdlib. If retained, convert strings to explicitly terminated UTF-8 C buffers and copy returned bytes into NG strings; consider direct process APIs and explicit errors. Do not pass the NG string object. |
| `lib/std/imgui.ng` | Native functions accept/return strings; `imgui_ngrt.cpp` interprets current NG string layout and returns `char*` | Preserve C++/ImGui as an optional host library, but make its ABI take pointer+length or C-owned buffers, never an NG object. Input-text results need caller-provided output buffer or explicit allocated-buffer ownership. Link only when imported/used. |

Also audit `src/native/lowering_expr.cpp`, `lowering_aggregate.cpp`, `lowering_scaffold.cpp`, and `src/native/helpers/{str,arr,ref,span}.ssa`: current string clone/equality/concat, array allocation/copy, reference envelopes, trait views, and Box cloning call runtime helpers. These are language representation dependencies even when no stdlib declaration mentions them.

## Implementation stages

Each stage should land with the focused tests and example listed in that stage. Keep the full suite green at each integration point. Do not edit `vendored/qbe-1.3` to add signature checking.

### Stage 0 — Lock contracts and establish migration inventory

1. Agree the syntax `native "<link-name>" fun ...;` and `type T = native "<representation>";` while retaining shorthand `native fun` for generated symbols and `type T = native;` only if compatibility needs it.
2. Specify module identity: imported logical module path, reserved entry module identity, identifier encoding, symbol prefix, collision behavior, and ABI stability promise. Do not use module traversal index, DefId, function signature hash, machine path, or absolute path.
3. Specify no external overloads/generics in v1; decide whether two different NG declarations may bind the same symbol (recommended: permitted only if their normalized ABI signatures are identical, otherwise type error within one compilation).
4. Specify native type string vocabulary, pointer constness policy, null representation, ownership rules, target set, and `repr(C)` record restrictions.
5. Write UTF-8 and native buffer ownership semantics before rewriting string/io functions.

Tests/examples: add parser-only cases for each accepted spelling and precise rejections; add one short `example/native_abi_basics.ng` illustrating generated vs explicit names and a pointer type. This stage defines syntax only; it need not link.

### Stage 1 — Preserve link and representation metadata

1. Extend syntax AST, HIR, module artifact/type descriptors, and clone/specialization paths with optional native link name, logical module ID, and canonical native representation kind.
2. Have `ModuleLoader` preserve logical module identity independently from `originModule` visibility index. Detect duplicate logical IDs if the chosen scheme allows collisions.
3. Keep source spans for diagnostics. Ensure module names survive imported and selectively imported declarations.
4. Do not assign external symbol identity to regular NG function DefIds.

Tests/examples: `module_parser_test`, `module_import_test`, and `native_function_test` cover omitted/present link names, quoted-name escapes/invalid names, imported modules with same function names, reordered imports producing identical emitted symbols, and root entry identity. Example: `example/native_abi_import.ng` imports two modules with the same source function name and checks distinct generated symbols in emitted SSA.

### Stage 2 — Typecheck the direct C ABI subset

1. Add a canonical ABI type classifier shared by native and foreign declarations: C integer widths/signs, bool policy, f32/f64, supported native pointer/opaque kinds, nullable pointer, callback if retained, and `repr(C)` records.
2. Reject native declarations that accept/return `string`, arrays, tuple, ordinary struct/enum, trait values/views, refs/spans, closure values except explicitly supported C callback function pointers, generic native functions, variadic generated-native functions, or unsupported type constructors.
3. Continue allowing an opaque pointer as a value without permitting dereference/arithmetic. Enforce nullability only for pointer-represented native types.
4. Validate `repr(C)` fields recursively and disallow generic/packed/zero-size/unsupported member layouts. Keep ordinary NG record types separate.
5. Enforce external overload/link-name collisions and compare duplicate direct declarations' normalized ABI signatures when declarations are visible in one compilation.
6. Keep ownership annotations only where their behavior is precise. C borrowed pointer parameters borrow for the duration of the call; `owning` transfers only an explicitly owned native pointer. Do not apply NG deep-copy semantics to C aggregate records.

Tests/examples: extend `opaque_type_test.cpp`, `native_qbe_test.cpp`, `cabi_callback_test.cpp`, and add `native_abi_typecheck_test.cpp` to cover every accepted ABI kind, all rejected NG object kinds, nullable pointers, native type parameterization policy, incompatible duplicate symbol declarations, and existing repr(C) checks. Example: `example/native_abi_rejections.ng` is a compile-fail corpus only if examples infrastructure supports expected diagnostics; otherwise keep these checks in Catch2 source strings.

### Stage 3 — Direct symbol lowering and explicit link names

1. Route native declarations and `extern "C"` declarations through one ABI call lowerer, fed by the declared signature and classified ABI representation.
2. With explicit `native "symbol"`, emit exactly `$symbol` in QBE IL. With unnamed native, emit the stable module/function encoding. Do not add `$ngrt_` or any generated forwarding wrapper.
3. Avoid source-name-based special cases (`print`, `toFloat`, `pushBack`, etc.); built-in compiler intrinsics should be an explicit separate mechanism if still needed.
4. Ensure QBE aggregate operand/result classes are emitted from the C ABI record descriptor, and preserve existing C callback/variadic behavior only where declared and tested.
5. For an absent symbol, let QBE emit a relocation and let link fail with the exact symbol. Improve driver diagnostics to say link failed and include linker output; do not call this a signature check.

Tests/examples: extend `native_qbe_test.cpp` to assert explicit symbol passthrough and deterministic generated symbols; a C/C++ fixture declared `extern "C"` tests a scalar, pointer, and repr(C) value at runtime. Add a deliberately absent symbol test that asserts link failure and the missing symbol. Add an example importing a generated native C shim fixture only in test harness; repository examples should remain runnable without test-only linker inputs.

### Stage 4 — Replace generic memory handle ABI

1. Introduce exact raw allocation declarations (`malloc(usize) -> Handle`, `free(Handle) -> unit`) or an internal allocator ABI with the same exact signature. Verify `usize` lowering matches supported target `size_t`.
2. Make compiler-known `sizeOf<T>`/`alignOf<T>` or a layout query available to generic lowering. Do not pass T through C. Reject generic Box allocation until size/alignment and value storage are correct.
3. Implement Box/Cell storage operations in NG/compiler-generated code. Define how values are copied into and out of cells, how nested affine values are moved/dropped, and how aggregate values stored indirectly are cloned. `Drop` must free exactly once and run T's drop glue if required.
4. Delete `ngrt_allocate/load/store/release` and the live token table only after tests prove no other lowering path uses them. Keep a test-only outstanding-allocation counter outside the stable stdlib API if needed.

Tests/examples: extend `box_test.cpp` with primitive and aggregate value round trips, copy independence, mutation, move/drop exactly once, zero-size handling, alignment, and allocation failure policy. Keep `Cell<T>` alias/mutation tests. Add `example/native_memory.ng` using `Handle = native "void*"` and scalar allocation as the direct ABI smoke example; standard `Box<T>` example must remain generic and pure NG at its public API.

### Stage 5 — Establish NG-owned string/UTF-8 representation

1. Choose and document a string value layout for QBE, preferably explicit bytes pointer + byte length + capacity (plus any cached scalar count only if measured as necessary). Specify empty-string and literal storage, ownership, mutation, and clone behavior.
2. Make all compiler-generated string operations consume that representation without asking C to inspect NG objects. Replace `ngrt_str_clone`, concat/equality/order, string-specific array clone, and string literal layout paths in `lowering_expr.cpp`, `lowering_aggregate.cpp`, and helper SSA.
3. Define UTF-8 invariant and error policy. Recommended v1: strings contain valid UTF-8; `length` counts Unicode scalar values; `charAt` and `substring` use scalar indexes; byte length is exposed separately if needed; malformed byte input is rejected at OS/C ingress. Comparisons remain lexicographic UTF-8 bytes (equivalent to scalar order for valid UTF-8), with no normalization/grapheme behavior. Case conversion must be explicitly ASCII-only or deferred until Unicode data support is chosen.
4. Replace `std::string` returning runtime APIs with byte-buffer ingress/egress primitives and explicit ownership. For C `char*` APIs, create a temporary NUL-terminated copy and reject embedded NUL where C APIs require it.

Tests/examples: new `string_utf8_test.cpp` covers ASCII and 2/3/4-byte scalars, empty strings, embedded NUL in NG strings, scalar length/index/slice, invalid UTF-8 ingress, boundary errors, equality/order, concat, clone and array-of-string operations. Add `example/utf8_string.ng` demonstrating scalar indexing and byte/scalar length distinction.

### Stage 6 — Migrate stdlib surfaces and host bindings

Migrate modules in dependency order: `string` core first; `prelude` formatting/parsing/printing next; `seq` after array operations; `memory` after layout/drop glue; `io` and `system` using byte buffers; `imgui` last because it is optional and owns a substantial C++ host API.

1. `string`: replace every current native implementation (`split`, `join`, trim, find/replace, case, length, indexing/slice, regex). Regex can remain an optional C primitive over a temporary byte buffer if there is no NG regex engine; its interface must be bytes+length and must define NUL/UTF-8 semantics.
2. `prelude`: move conversions and integer parsing into NG where implementable. Keep a tiny scalar formatter/output host ABI only where necessary; print strings as `(pointer, byte_length)`.
3. `seq`: move generic and array operations to NG/compiler intrinsics; preserve COW uniqueness semantics. Do not keep a C function that accepts `array<T>`.
4. `memory`: keep only pointer allocator/deallocator primitives. No generic C memory-value functions.
5. `io`: provide raw byte read/write/allocate functions with explicit length and ownership. `readLine`, `readFile`, and current path become NG functions composing these primitives. Propagate IO errors as NG `Result`, not a C++ exception hidden in a wrapper, if the language's current error surface can express it.
6. `system`: make shell process APIs optional and explicit; serialize command to a temporary NUL-terminated UTF-8 buffer. Return output bytes with an explicit buffer result and release path. Do not return a `char*` pretending to be NG `string`.
7. `imgui`: change C++ functions to consume byte views and return output through caller-owned buffers or a documented native allocation. Keep declarations in `lib/std/imgui.ng`; compile/link tests must exercise the exact exported C names.

Tests/examples: retain and update stdlib behavior tests and the corpus sweep. Examples: `example/io_bytes.ng`, `example/seq_ng.ng`, and the existing `example/ng_ide.ng` should continue to run. Add a headless ImGui test for input/output text buffers and one that verifies unused programs do not link the ImGui archive.

### Stage 7 — Remove obsolete runtime coupling and finish integration

1. Remove migrated functions and obsolete layouts from `include/ngrt.h`, `src/native/libngrt.c`, `include/ngrt.hpp`, `src/ngrt.cpp`, and `src/native/imgui_ngrt.cpp` only when no runtime or const-eval call site remains.
2. Separate compile-time native registrations from runtime direct ABI declarations. Preserve deterministic const evaluation behavior and diagnostics.
3. Make the driver link only required runtime/host components. Ensure a minimal program with no OS or stdlib host imports does not depend on libngrt; arithmetic/array/string programs may still depend on the QBE helper archive until those helpers are moved or linked by feature.
4. Update CMake targets and driver toolchain tests. Do not delete allocation/error/panic support merely because it shares the old C file.
5. Update implemented-state docs and D-004 only after test and example coverage passes.

Tests/examples: run focused FFI, repr(C), callback/varargs (if retained), native type, memory, string, stdlib, module, driver, qbe smoke, and examples sweep tests, then full `ctest --test-dir build -j`. Add `example/native_abi_minimal.ng` that compiles and links with only an explicitly linked C symbol and no `libngrt` requirement, and `example/native_abi_module_symbol.ng` that demonstrates the generated name convention in SSA.

## Test matrix

| Layer | Required assertions |
| --- | --- |
| Lexer/parser | Both native function spellings; explicit link-name literal validation; native representation syntax; old shorthand compatibility or deliberate diagnostic; malformed/escaped string rejection. |
| Module/HIR | Stable logical module identity independent of import traversal; same function names in different modules; selective imports/re-exports preserve identity; no absolute path in emitted symbols. |
| Typechecker | Accept supported scalar/pointer/repr(C); reject NG-owned objects, unsupported raw pointers, generic/overloaded native, invalid repr(C) members, invalid nullability, incompatible duplicate explicit-link declarations. |
| QBE emission | Exact explicit symbol, deterministic generated symbol, correct C argument/result classes, C aggregate type/layout reuse, no `$ngrt_<native-name>` shim or deep clone of forbidden object arguments. |
| Linker/driver | Correct C fixture links and runs; absent symbol fails at link and names the unresolved symbol; no claim that wrong external prototype is caught; archive selection matches imports. |
| Ownership/memory | Opaque pointer null/ownership annotations; allocation size/alignment; Box/Cell copy, move, alias, mutation, nested drop and exactly-once free. |
| UTF-8/string | Valid multibyte scalar semantics, byte/scalar lengths, malformed ingress, NUL behavior, concat/slice/copy, array of strings, IO round-trip. |
| Standard library | Existing observable behavior retained or intentionally changed with tests: numeric formatting/parsing, print, IO errors, sequence COW, system output, optional ImGui. |
| Examples | One runnable example per completed phase; existing corpus and headless `ng_ide.ng` remain green. |

## Risks and unresolved decisions

1. **Module identity:** current imports are names resolved relative to a file and then stdlib search roots. The loader has only canonical paths and numeric origin indexes, not a public package/module namespace. The implementation must choose an import-identity rule before symbols can be stable. A temporary rule may use normalized import names and reserve a fixed entry ID, with collisions rejected.
2. **String Unicode contract:** UTF-8 validation and scalar indexing are concrete; Unicode case mapping, normalization, grapheme clusters, locale, and regex semantics require separate choices. Do not silently inherit the C process locale.
3. **Generic allocation layout:** current ordinary NG values often use indirect heap pointers and deep-copy clone helpers. A raw malloc Box cannot be correct until size, alignment, copy/move/drop glue, and nested allocations are specified together.
4. **Allocation ownership:** replace process-lifetime arena allocation only with a defined allocation strategy. If allocations can be individually freed, all strings/arrays/closures/views must avoid dangling references and must release nested values correctly.
5. **Native signature assurance:** external C prototype mismatch remains caller error unless the build compiles an NG-generated C header or uses an external ABI-description mechanism. Neither is required for this first plan. Linker failure is only a missing/incompatible binary symbol failure, not type checking.
6. **Const evaluation:** current const native hosts are implemented through C++ wrappers and NG `Value`; changing runtime native symbols does not automatically migrate compile-time behavior.
7. **Compatibility:** all current std native declarations and generated SSA names are implementation details only insofar as docs promise them; nevertheless tests assert them. Update tests in the same phase as behavior and avoid a compatibility shim that preserves the unwanted NG-object ABI.

## Completion criteria

The redesign is complete when direct native declarations cannot cross NG-owned values, explicit symbols bind exactly without wrappers, generated symbols are stable across import order and machines, native pointers have correct QBE/C ABI classification, C records use verified `repr(C)` layouts, memory/string/sequence semantics are owned by NG/compiler code, remaining host calls use explicit pointer+length/buffer ownership contracts, obsolete runtime wrappers are removed or clearly limited to compile-time/platform support, and all focused tests/examples plus the full corpus pass.
