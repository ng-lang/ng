# Changelog

All notable changes to the NG programming language project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Each pull request is documented as a separate entry.

## [Unreleased]

### Generic Functions & Variadic Parameters (v0.5.0)
- **Parser**: Generic function/type syntax `<T>`, `<T, U>`, `<T...>` (parameter packs); suffix syntax `T TypeName` (left-associative desugaring); `<T: Bound>` type constraints; nested generics `Option<Option<int>>`; `>>` split handling
- **AST**: `GenericParam` node with `name`, `isPack`, `bound` fields; `genericParams` added to `FunctionDef`, `TypeDef`, `TypeAliasDef`, `NewTypeDef`, `TaggedUnionDef`; `TypeAnnotation` gains `genericArgs` field
- **Type checker**: `GenericParamType` (tag `0xC0`), `GenericDefType` (tag `0xC1`); first-pass generic function registration; type parameter inference via unification (`extractGenericBindings`); monomorphization (`monomorphizeGenericCall`); suffix array type annotation support (`T array` → `ArrayType<T>`)
- **Interpreter**: Runtime monomorphization of generic functions; parameter pack collection and expansion
- **Standard library**: `print<T...>`, `assert<T...>` (variadic native functions); `len<T>(xs: T array) -> u32` (native, supports arrays, strings, tuples); `build_prelude_type_index()` auto-loads prelude types for type checking
- **Tests**: 258/258 passing (4 new generic/typecheck tests)
- **Examples**: Updated `example/15.generics.ng` with `len()` tests


### Tagged Union Types (Phase 3A-3F)
- **AST**: Added `TaggedUnionDef`, `TaggedValueExpression`, `SwitchStatement`, `VariantDef`, `CaseClause` nodes
- **Parser**: `type Result = Ok(value: i32) | Err(msg: string);` with optional named parameters; `switch (expr) { case Variant(bindings) { body } }` pattern matching
- **Type system**: `TaggedUnionType` (nominal match by name), `VariantType` (matches parent union), `UnionType` (structural `A | B`); `typeinfo_tag` values `TAGGED_UNION=0xB3`, `VARIANT=0xB4`, `UNION=0xB5`
- **Type checker**: Registers tagged union types with variant payload types; validates switch scrutinee is a tagged union; binds payload variables in case bodies; resolves variant names in function calls to `VariantType`
- **Runtime**: `NGTaggedValue` struct with `unionName`, `variantName`, `variantIndex`, `payload` fields; `type()`, `show()`, `boolValue()` implementations
- **ORGASM opcodes**: `CONSTRUCT_TAGGED` (type_idx, variant_idx, num_payload), `GET_TAG`, `GET_PAYLOAD` (field_idx), `SWITCH_TAG` (numCases + jump table)
- **Compiler**: Registers tagged union types + variants in `variant_map`; FunCallExpression resolves variant names for `CONSTRUCT_TAGGED` emission; SwitchStatement compiles with inline jump table
- **VM**: Handlers for all tagged union opcodes; `SWITCH_TAG` peeks at tagged value, reads jump table, jumps to matching case
- **Interpreter**: `TaggedUnionDef` registers variant constructors as functions; `SwitchStatement` matches variant name and binds payload variables

### Object/Struct Memory Layout (Phase 2)
- **Value semantics**: `NGStructuralObject` now maintains `Vec<RuntimeRef<NGObject>> fields` for flat index-based access alongside `Map<Str, RuntimeRef<NGObject>> properties`
- **Compiler**: `GET_PROPERTY`/`SET_PROPERTY` use field index (O(1) access); `NEW_OBJECT` pushes values in type property order; `find_field_index()` resolves property names at compile time
- **VM**: `GET_PROPERTY` reads `fields[fieldIdx]`; `SET_PROPERTY` writes both `fields` and `properties`; `GET_PROPERTY_STR`/`SET_PROPERTY_STR` added for dynamic string-based lookup

### Multi-Module Compilation (Phase 1C)
- `BytecodeModule::merge()` combines modules with full bytecode index remapping (strings, functions, constants)
- Exports re-registered with optional prefix

### Native Call / FFI (Phase 1B)
- `native_bridge.hpp`: Auto-marshaling FFI bridge between C++ types and NG runtime types
- Template-based `wrap_native()` for lambda/function support with automatic type conversion
- `VM::register_native<T>(name, func)` for ergonomic native function registration
- Supported C++ types: int8-uint64, float, double, bool, string, RuntimeRef<NGObject>

### Operand Index Widening (Phase 1A)
- Operand indices widened from `uint8_t` to `uint16_t` (max 65535 constants/strings/functions)
- Instruction format: `[opcode: u8][operand: u16 LE]` (3 bytes)
- `emit_u16()` and `read_u16()` added to compiler and VM

## [PR #XX] ORGASM Level-2 Parser and Interpreter - 2025-10-17

### Added

- **ORGASM Level-2 Parser and Interpreter** - Complete implementation of intermediate representation
  - Lexer for tokenizing ORGASM directives and instructions (`src/orgasm/lexer.cpp`)
  - Parser for module structure, data sections, and functions (`src/orgasm/parser.cpp`)
  - Stack-based interpreter/VM for bytecode execution (`src/orgasm/interpreter.cpp`)

- **Type System Support**
  - Signed integers: i8, i16, i32, i64, i128
  - Unsigned integers: u8, u16, u32, u64, u128
  - Floating point: f16, f32, f64, f128
  - Boolean, char, unit, and address types
  - Vector types: v128, v256, v512
  - Atomic types: atomic.i32, atomic.i64, atomic.addr

- **Operations**
  - Arithmetic: add, subtract, multiply, divide
  - Comparison: gt, lt, ge, le, eq, ne
  - Logical: and, or, xor, not
  - Bitwise: shl, shr
  - Control flow: br, goto, return with label support
  - Function calls and native import handling
  - Tuple operations: tuple_create, tuple_destroy, tuple_get, tuple_set
  - Type casting between compatible types
  - Stack operations: load_*, store_*, push_param

- **Security Features**
  - Platform-specific secure memory initialization (explicit_bzero, SecureZeroMemory)
  - Volatile pointer fallback for other platforms
  - Proper memory cleanup with tuple_destroy opcode

- **Documentation**
  - Added ORGASM architecture to AGENTS.md
  - Example ORGASM assembly files in `example/orgasm/` directory
  - Comprehensive inline documentation

- **Testing**
  - 49 test cases with 128 assertions for ORGASM components
  - Lexer tests: tokenization, comments, punctuation, EOF handling
  - Parser tests: modules, data sections, arrays, multiple data types
  - Interpreter tests: arithmetic, control flow, tuple operations, type casting
  - Integration tests: end-to-end execution, complex scenarios

### Fixed

- Security vulnerability in tuple memory initialization (replaced memset with secure alternatives)
- Parser validation for duplicate parameter definitions
- Error handling for instructions without proper context
- Function parameter and instruction parsing order

### Changed

- Updated CMakeLists.txt to include ORGASM sources and tests
- Modified .gitmodules to use HTTPS URLs for submodules
- Enhanced compiler flag detection for coverage support

### Commits

```
87eca02 docs: add more tests, update AGENTS.md and create CHANGELOG.md
fc4deb8 fix(orgasm): fix security issue and add comprehensive test coverage
73f1da5 fix(orgasm): add validation and error handling in parser
5d8f9c2 feat(orgasm): add integration tests and fix function/instruction parsing
05c43e8 feat(orgasm): add interpreter/VM with stack-based execution
e947a85 feat(orgasm): add lexer and parser for ORGASM Level-2
efe958b chore: initial plan for ORGASM Level-2 parser and interpreter
bf3bd1b Initial plan
```

### Statistics

- **Files Added**: 12 (6 headers, 3 implementation, 4 test files)
- **Lines of Code**: ~1,972 total (~1,465 implementation, ~507 headers)
- **Test Coverage**: 49 test cases with 128 assertions
- **Overall Tests**: 174 test cases with 907 assertions passing

---

## [PR #29] Fix copilot-instructions.md link - 2025-10-17

### Fixed

- Fixed broken link in copilot-instructions.md documentation

## [Previous]
- Nominal type system: typealias, newtype, bidirectional inference (163/163 tests)
- Tuple literal and destruct support (#25)
- Simplify token type (#20)
- Switch back to exceptions for parsing (#18)
- Function body type check, fix parser (#17)
- Array typecheck and type annotation (#16)
- Unary expression interpret and check (#15)
- Typechecking for function signature and function calls (#12)
- Primitive type testing (#11)