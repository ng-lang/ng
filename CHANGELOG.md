# Changelog

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

## [Previous]
- Nominal type system: typealias, newtype, bidirectional inference (163/163 tests)
- Tuple literal and destruct support (#25)
- Simplify token type (#20)
- Switch back to exceptions for parsing (#18)
- Function body type check, fix parser (#17)
- Array typecheck and type annotation (#16)
- Unary expression interpret and check (#15)
- Typechecking for function signature and function calls (#12)
- Primitive type checking (#11)
