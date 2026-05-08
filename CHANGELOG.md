# Changelog

All notable changes to the NG programming language project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Each pull request is documented as a separate entry.

## [Unreleased]

### Bug Fixes: Example Runtime Errors
- **VM**: Added `NOP` opcode handler — opcode `0x00` was defined but had no case in the VM execute loop, causing "Unknown opcode: 0" (fixes `example/05.valdef.ng`)
- **Compiler**: Added `visit(ast::UnitLiteral*)` — unit literals in object construction and import contexts had no visitor, causing "Property type mismatch" and "Unknown type for object" errors (fixes `example/07.object.ng`, `example/08.imports.ng`)
- **Compiler**: Push arguments onto stack before emitting `NATIVE_CALL` — native function calls (e.g. `split`, `trim`) were missing argument compilation, causing "Unknown function" errors (fixes `example/16.stdlib_basics.ng`)
- **Example**: Simplified `example/16.union_type.ng` to use only supported operators and val declarations
- **Tests**: 267/267 passing

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

## [PR #31] Implement NG to ORGASM compiler using visitor pattern

### Added

- **Visitor pattern architecture** for code generation with `CodeGenVisitor` base class
  - Template method `visit()` for type-safe dispatching over all AST node types
  - `OpCodeVisitor` concrete implementation that generates ORGASM bytecode from AST
- **NGCompiler** compiler class that:
  - Transforms NG AST nodes directly to ORGASM instructions
  - Supports literals (numeral, string, boolean), binary/unary expressions, variable definitions
  - Handles function definitions with proper stack frame management
  - Supports function calls with parameter passing
  - Generates control flow (if/else, while loops)
  - Supports import/export for modular compilation
  - Handles tagged unions with constructor generation and pattern matching
  - Includes switch statement support for tagged union dispatch
- **Integration with existing ORGASM infrastructure**
  - Uses `BytecodeModule` for output with automatic string constant deduplication
  - Leverages `Opcode` definitions from `opcode.hpp` for all instruction emission
  - Supports type-aware code generation (i32, f64, string, bool, unit)
- **Comprehensive test suite** (20 tests, 65 assertions)
  - Literal code generation for numerals, strings, booleans, unit
  - Binary operations (arithmetic, comparison, logical)
  - Variable definitions and assignments
  - Function definitions with parameters
  - Function calls with arguments
  - If/else and while loop code generation
  - Import/export module support
  - Tagged union type definitions and constructors
  - Switch statement with tagged union pattern matching

### Technical Details

- Compiler maps NG types to ORGASM operand types (int → `OperandType::INT32`, double → `OperandType::FLOAT64`, etc.)
- Functions are compiled with proper local variable tracking via `currentLocals` counter
- String constants are deduplicated through the module's `strings` vector
- Tagged union constructors are registered as functions with associated variant metadata

## [PR #30] ORGASM Level-2 Parser and Interpreter

### Added

- **ORGASM Level-2 Parser and Interpreter** - Complete implementation of intermediate representation
  - Lexer for tokenizing ORGASM directives and instructions
  - Parser for module structure, data sections, and functions
  - Stack-based interpreter/VM for bytecode execution

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

---

## [PR #29] Fix copilot-instructions.md link

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