# Changelog

All notable changes to the NG programming language project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Each pull request is documented as a separate entry.

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
