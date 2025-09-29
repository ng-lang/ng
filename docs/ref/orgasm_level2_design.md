# Level-2 ORGASM (Organized Assembly) Language — Design Document

## Overview

Level-2 ORGASM (Organized Assembly) is a modern, symbolic, and strongly-typed intermediate representation for the NG language. It is designed for clarity, modularity, and efficient mapping to real-world architectures (x86/ARM). Level-2 emphasizes explicit types, local addressing, and symbolic linkage, making it suitable for advanced analysis, optimization, and code generation.

---

## 1. Key Features

### Symbolic and Typed Design
- All functions, parameters, values, constants, and arrays are named and explicitly typed, supporting strong static analysis and type safety.

### Local Instruction Addressing
- Each function, start block, or method uses its own local instruction addresses (e.g., `00:`), improving modularity, debugging, and incremental compilation.

### Explicit Parameter and Data Declarations
- Parameters, constants, arrays, and values must declare their type. Parameters use `.param [name:type, ...]` syntax, and all data is grouped by type for clarity and efficient lookup.

### Symbol Table and Linkage
- A `.symbols` section lists all identifiers for linkage, reflection, and tooling. Imports and exports are explicit, supporting modular code and cross-module calls.

### Strongly-Typed Instruction Set
- All data and arithmetic instructions are type-annotated (e.g., `add.i32`, `load_const.f64`). Type promotion is explicit via `cast.<type>`, ensuring predictable and safe operations.

### Modern Control Flow and Method Invocation
- Supports structured control flow (`br`, `goto`, labels) and object-oriented method invocation (`push_self`, `load_symbol`, `invoke_method`).

### Readable and Maintainable Syntax
- All instructions and data declarations support trailing comments for clarity. The syntax is designed for both human readability and ease of code generation.

### Extensible and Analysis-Friendly
- The IR is designed for extensibility (future support for new types, instructions, or features) and is well-suited for static analysis, optimization, and direct mapping to bytecode or machine code.


---

## 2. Program Structure

### Modules
- Declared with `.module <name>` ... `.endmodule`.
- Contains `.symbols`, `.const`, `.str`, `.array`, `.val`, `.fun`, `.method`, `.start`.

### Symbols Table
- `.symbols [name1, name2, ...]` declares all identifiers used in the module.
- Used for functions, parameters, properties, methods, etc.

### Functions
- Declared with `.fun <name>` ... `.endfun <name>`.
- Parameters are named and typed: `.param [n:i32, m:addr, ...]`.
- Each instruction is addressed: `00:    ...`, and addresses are local to the function.
- Labels: `.label <name>, [+offset]`.


### Data Declaration Order and Organization

ORGASM follows a strict declaration order to optimize CPU cache performance by grouping same-type data together:

**Declaration Order:** `.const` → `.str` → `.array` → `.val`

This ordering ensures cache-friendly sequential access patterns and predictable memory layout.

#### Typed Constants: `.const <type> <value>`
All constants must declare their type explicitly, e.g., `.const u16 1u16`. Constants are referenced by their definition index (offset), not by name. If type is omitted, defaults are `i32` for integers and `f64` for floats. Group all constants of the same type together for cache efficiency.

**Example:**
```plaintext
.const i32 1
.const i32 42
.const bool false
.const bool true
.const f64 3.14
```

#### Strings: `.str [value]`
Strings are referenced by index. Place all string declarations after constants.

**Example:**
```plaintext
.str ["hello"]
.str ["world"]
```

#### Typed Arrays: `.array <type> [elements]`
Arrays must declare their element type, e.g., `.array i32 [1, 2, 3]`. Arrays are referenced by their definition index. Group arrays of the same type together for cache efficiency.

**Example:**
```plaintext
.array i32 [1, 2, 3, 4, 5]
.array i32 [10, 20, 30]
.array f64 [1.1, 2.2, 3.3]
```

#### Typed Values: `.val <type> <value>`
All values must declare their type and initial value, e.g., `.val i32 0`. If omitted, defaults as above. Group variables by type for cache efficiency.

**Example:**
```plaintext
.val addr 0     // memory address
.val i32 100    // integer value
.val bool false // boolean flag
```

**Note:** These declarations define the program's memory layout at load time. Actual initialization logic is performed at runtime using `load*` and `store*` instructions.

### Imports/Exports
- Import: `.import <symbol>, <module>`
- Export: `.export <symbol>`

---

## 3. Instruction Set

Level-2 ORGASM instructions are more explicit and symbolic, with direct analogs to x86/ARM:


### Data/Stack Operations
- `cast.<type>` — Converts the value at the top of the stack to the specified type (e.g., `cast.i64`, `cast.f32`). Used for explicit type promotion before arithmetic or logic operations. If the conversion is invalid (e.g., overflow, incompatible types), it is recommended to throw an exception or trigger a trap.
- `load_param.<type> param.n` — Load parameter by index, with type (e.g., `load_param.i32 param.0`).
- `load_const.<type> const.n` — Load constant by index and type (e.g., `load_const.u16 const.1`).
- `load_value.<type> val.n` — Load value by index and type (e.g., `load_value.f32 val.2`).
- `store_value.<type> val.n` — Store value by index and type.
- `load_volatile.<type> addr.n` — Load from memory with volatile semantics (prevents optimization)
- `store_volatile.<type> addr.n` — Store to memory with volatile semantics (prevents optimization)
- `load_str str.n` — Load string by index.
- `load_array.<type> array.n` — Load array by index and type (e.g., `load_array.i32 array.0`).
- `push_param` — Push parameter for function/method call.
- `push_self` — Push current object for method call.
- `load_symbol symbol.n` — Load symbol for dynamic dispatch.
- `invoke_method` — Call method via symbol (like vtable call).
- `call fun.<index>` — Call function by index.
- `call import.n` — Call imported function.
- `ignore` — Discard stack top.

### Arithmetic/Logic
> **Type Promotion:**
>
> For arithmetic and logic operations, operands must be of the same type. Use the `cast.<type>` instruction to explicitly convert (promote) values to the required type before performing the operation. This avoids implicit type conversion and ensures predictable behavior.
>
> **Example:**
> ```
> load_const.i32 const.0
> cast.f32
> load_const.f32 const.1
> add.f32
> ```
>
> Here，`cast.f32` converts the i32 value to f32 before the addition.

- `add.<type>`, `subtract.<type>`, `multiply.<type>`, `divide.<type>`, `gt.<type>`, `eq.<type>`, `shl.<type>` — All arithmetic/logic instructions must use a type suffix (e.g., `add.i32`, `eq.f64`), and operand types must match.
> **Overflow Handling:**
>
> Arithmetic operations can specify overflow behavior:
> - `add.i32 [wrap]` — Wrap on overflow (default for unsigned)
> - `add.i32 [trap]` — Trap on overflow (safe mode)
> - `add.i32 [saturate]` — Saturate on overflow (multimedia)
>
> **Example:**
> ```
> load_const.i32 const.max
> load_const.i32 const.1
> add.i32 [trap]  // Will trap if overflow occurs
> ```

> **Cast Failure & Overflow:**
>
> If a `cast.<type>` instruction encounters an invalid conversion (such as incompatible types or overflow), the recommended behavior is to throw an exception, trigger a trap, or terminate execution to avoid undefined behavior.
  
### Control Flow
- `br <label1>, <label2>` — Conditional branch (labels are local to the current function/method/start block).
- `goto <address>` — Unconditional jump (address is local to the current function/method/start block).
- `.label <name>, [+offset]` — Label with optional offset (local scope).
- `return` — Return from function.

### SIMD Operations (Optional)
> **SIMD Support:**
>
> SIMD instructions operate on vector types and map directly to CPU SIMD instructions. These are optional and only available on targets with SIMD support.

- `vadd.<type>.<lanes>` — Vector addition (e.g., `vadd.f32.4` for 4x32-bit floats)
- `vsub.<type>.<lanes>` — Vector subtraction
- `vmul.<type>.<lanes>` — Vector multiplication
- `vload.128 addr.n` — 128-bit vector load (maps to SSE movaps, NEON vld1q)
- `vstore.128 addr.n` — 128-bit vector store (maps to SSE movaps, NEON vst1q)
- `vload.256 addr.n` — 256-bit vector load (maps to AVX vmovaps)
- `vstore.256 addr.n` — 256-bit vector store (maps to AVX vmovaps)

### Tuple Operations (Memory-Based)
> **Tuple Design Philosophy:**
>
> Tuples are type-erased, contiguous memory blocks with explicit offset-based access. ORGASM doesn't track element types at runtime, only memory layout. Type safety is ensured through correct offset calculations and explicit casting.
>
> **Memory Layout:** Tuples are stored as contiguous bytes. Each element's position is determined by its offset from the start. Element sizes follow platform conventions (i32=4 bytes, f64=8 bytes, addr=8 bytes on 64-bit systems).
>
> **Type Safety:** Since ORGASM is type-erased, element types must be tracked at compile time. The programmer must ensure correct offsets and use explicit `tuple_get.<type>` / `tuple_set.<type>` instructions when retrieving values.

#### Tuple Creation and Management
- `tuple_create size` — Create a tuple with specified total size in bytes
- `tuple_destroy` — Deallocate tuple memory (optional, for explicit memory management)

#### Tuple Element Access (Type-Aware with Offsets)
- `tuple_get.<type> offset` — Load typed value from specified byte offset
  - `tuple_get.i32 0` — Load i32 from offset 0 (reads 4 bytes)
  - `tuple_get.f64 4` — Load f64 from offset 4 (reads 8 bytes)
  - `tuple_get.addr 12` — Load address from offset 12 (reads 8 bytes on 64-bit)
- `tuple_set.<type> offset` — Store typed value to specified byte offset
  - `tuple_set.i32 0` — Store i32 to offset 0 (writes 4 bytes)
  - `tuple_set.f64 4` — Store f64 to offset 4 (writes 8 bytes)
  - `tuple_set.addr 12` — Store address to offset 12 (writes 8 bytes on 64-bit)

#### Memory Layout and Offsets
Element positions are determined by byte offsets. The compiler tracks element types and sizes:
- `i32`: 4 bytes
- `f64`: 8 bytes
- `addr`: 8 bytes (64-bit systems)
- `bool`: 1 byte
- `string`: 8 bytes (address reference)

For complex layouts, document offsets in comments:
```plaintext
// Tuple (i32, f64, string): offsets [0, 4, 12], total size = 20
// Tuple (addr, i32, f64): offsets [0, 8, 12], total size = 20
```

#### Example: Tuple (i32, f64, string)
```plaintext
// Create tuple: total size = 4 + 8 + 8 = 20 bytes
load_const.i32 20
tuple_create
store_value.addr val.0; my_tuple

// Set elements with type-aware operations
load_value.addr val.0; my_tuple
load_const.i32 42              // value for offset 0
tuple_set.i32 0                // set i32 at offset 0 (automatically writes 4 bytes)

load_value.addr val.0; my_tuple
load_const.f64 3.14            // value for offset 4
tuple_set.f64 4                // set f64 at offset 4 (automatically writes 8 bytes)

load_value.addr val.0; my_tuple
load_str str.hello             // string address for offset 12
tuple_set.addr 12              // set addr at offset 12 (automatically writes 8 bytes)

// Get elements - type-safe, no casting needed
load_value.addr val.0; my_tuple
tuple_get.i32 0                // load i32 from offset 0 (automatically reads 4 bytes)

load_value.addr val.0; my_tuple
tuple_get.f64 4                // load f64 from offset 4 (automatically reads 8 bytes)

load_value.addr val.0; my_tuple
tuple_get.addr 12              // load addr from offset 12 (automatically reads 8 bytes)
```

#### Tuple Spread and Unpack Operations

##### Tuple Spread Operation
Tuple spread is implemented by explicitly extracting elements from the source tuple and inserting them into the target tuple. This is done using `tuple_get` operations followed by `tuple_set` operations.

**Example: `val y = (0, ...x, 4)` where `x = (1, false, "hello")`**
```plaintext
// Create tuple y: (i32, i32, bool, string, i32)
// Total size = 4 + 4 + 1 + 8 + 4 = 21 bytes (aligned to 24)
load_const.i32 24
tuple_create
store_value.addr val.0; y

// Set element 0: y[0] = 0 (i32)
load_value.addr val.0; y
load_const.i32 0
tuple_set.i32 0

// Set element 1: y[1] = x[0] = 1 (i32) - spread from x
load_value.addr val.0; y
load_value.addr val.1; x    // get tuple x
tuple_get.i32 0             // get x[0] (i32 at offset 0)
tuple_set.i32 4             // set at offset 4

// Set element 2: y[2] = x[1] = false (bool) - spread from x
load_value.addr val.0; y
load_value.addr val.1; x    // get tuple x
tuple_get.bool 4            // get x[1] (bool at offset 4)
tuple_set.bool 8            // set at offset 8

// Set element 3: y[3] = x[2] = "hello" (string) - spread from x
load_value.addr val.0; y
load_value.addr val.1; x    // get tuple x
tuple_get.addr 8            // get x[2] (string at offset 8)
tuple_set.addr 12           // set at offset 12

// Set element 4: y[4] = 4 (i32)
load_value.addr val.0; y
load_const.i32 4
tuple_set.i32 20
```

##### Tuple Unpack Operation
Tuple unpack extracts elements from a tuple into individual variables. For remainder patterns (`...d`), create a new tuple from the remaining elements.

**Example: `val (a, b, c, ...d) = y` where `y = (0, 1, false, "hello", 4)`**
```plaintext
// Extract a = y[0] (i32)
load_value.addr val.0; y
tuple_get.i32 0
store_value.i32 val.2; a

// Extract b = y[1] (i32)
load_value.addr val.0; y
tuple_get.i32 4
store_value.i32 val.3; b

// Extract c = y[2] (bool)
load_value.addr val.0; y
tuple_get.bool 8
store_value.bool val.4; c

// Create d from remaining elements: d = (y[3], y[4]) = ("hello", 4)
// d has type (string, i32)
load_const.i32 16           // 16 bytes total (aligned)
tuple_create

// Set element 0: d[0] = y[3] = "hello" (string)
load_value.addr val.0; y
tuple_get.addr 12           // get y[3] (string at offset 12)
tuple_set.addr 0            // set at offset 0

// Set element 1: d[1] = y[4] = 4 (i32)
load_value.addr val.0; y
tuple_get.i32 20            // get y[4] (i32 at offset 20)
tuple_set.i32 8             // set at offset 8

store_value.addr val.5; d
```

##### Key Points:
- **Type Safety**: Element types must be tracked at compile time. Use appropriate `tuple_get.<type>` and `tuple_set.<type>` operations.
- **Offset Calculation**: Offsets are byte positions, calculated based on element sizes and alignment requirements.
- **Explicit Operations**: No implicit spread/unpack - all operations must be explicit for clarity and safety.
- **Memory Layout**: Document tuple layouts with comments showing offsets and total sizes for maintainability.

### Atomic Operations
> **Atomic Support:**
>
> Atomic instructions ensure thread-safe operations on shared memory. These map directly to CPU atomic instructions.

- `atomic_add.<type> addr.n` — Atomic add (x86: lock add, ARM: ldadd)
- `atomic_sub.<type> addr.n` — Atomic subtract
- `atomic_and.<type> addr.n` — Atomic AND
- `atomic_or.<type> addr.n` — Atomic OR
- `atomic_cas.<type> addr.n expected.n new.n` — Compare-and-swap (x86: cmpxchg, ARM: cas)
- `fence` — Memory barrier/fence (x86: mfence, ARM: dmb)

---

## 4. Calling Convention

- Parameters are named and referenced symbolically.
- Return values are left on the stack.
- Local values are named and accessed via `.val`/`store_value`/`load_value`.
- Method calls use `push_param`, `push_self`, `load_symbol`, and `invoke_method` for dynamic dispatch.

---



## 5. Data Types & Representation

### Supported Primitive Types

- **Integer Types:**
	- Signed: `i8`, `i16`, `i32` (default), `i64`, `i128`
	- Unsigned: `u8`, `u16`, `u32`, `u64`, `u128`
- **Floating-Point Types:**
	- `f16`, `f32`, `f64` (default), `f128`
- **Vector Types (SIMD):**
	- `v128` (128-bit vector for SSE/NEON)
	- `v256` (256-bit vector for AVX)
	- `v512` (512-bit vector for AVX-512)
- **Atomic Types:**
	- `atomic.i32`, `atomic.i64`, `atomic.addr` (for lock-free programming)
- **Other Types:**
	- `bool`, `char`, `unit` (for no-value)
	- `addr` (for address/reference types; used for pointers or references to complex types)
- **Composite Types (Reference-based):**
	- Arrays: `.array <type> [elements]` — Contiguous elements of same type
	- Tuples: Memory blocks with explicit offset-based access (see Tuple Operations section)
	- Objects: User-defined types with properties and methods
	- Strings: Byte sequences with encoding information

### Type Suffixes

- Integer and floating-point constants must use a type suffix (e.g., `1i16`, `42u32`, `3.14f32`).
- If no suffix is provided, integer constants default to `i32`, and floating-point constants default to `f64`.

### Typed Declarations

- **Constants:** `.const <type> <value> [attributes]`
	- Example: `.const u64 4294967295u64 // MAX`
	- All constants of the same type should be grouped together for clarity and efficient lookup. Any text after `//` or `;` is a comment.
	- Optional attributes: `[align=n]` for alignment requirements
	- **Note:** `.const` defines runtime constants, not compile-time symbolic constants
- **Arrays:** `.array <type> [elements] [attributes]`
	- Example: `.array i32 [1, 2, 3] // ARR`
	- Arrays of the same type should be grouped together. Any text after `//` or `;` is a comment.
	- Optional attributes: `[align=n]` for alignment (e.g., `[align=64]` for cache line alignment)
- **Values:** `.val <type> <value> [attributes]`
	- Example: `.val i32 0 // counter`
	- Optional attributes: `[align=n]`, `[volatile]` for volatile memory

### Typed Instructions

All value/parameter/constant/array loads and stores must use the typed form:
	- `load_const.<type> const.n`
	- `load_value.<type> val.n`
	- `store_value.<type> val.n`
	- `load_param.<type> param.n`
	- `load_array.<type> array.n`
	- Example: `load_const.u16 const.0`, `load_value.f32 val.2`, `load_param.i64 param.1`, `load_array.i32 array.0`

### Word Size

- All values have a defined size and type; word size is not assumed to be uniform. This enables precise mapping to hardware and prevents type confusion.

### References

- Arrays, constants, and strings are referenced by their definition index (offset), not by name.

### Constants and Values

- Constants, arrays, and values are referenced by their definition index (offset), not by name. Their type is explicit in all declarations and uses.

### Typical Usage of addr Type

- Used for passing references to arrays, objects, strings, and other composite types:
	- Example: `.param [arr:addr]`, `load_param.addr param.0`, can be used with `load_array.<type>` to implement pointer/reference operations.
- Used for pointer semantics, function pointers, indirect calls, and other advanced features.

---

## 6. Example: Factorial Function

```plaintext
.const i32 0 // ZERO
.const i32 1 // ONE
.fun fact
.param [n:i32]
00:    load_param.i32 param.0
01:    load_const.i32 const.0
02:    gt
03:    br fact_01, fact_02; +3, +9
.label fact_01
04:    load_param.i32 param.0
05:    load_param.i32 param.0
06:    load_const.i32 const.1
07:    subtract
08:    push_param
09:    call fun.0
10:    multiply
11:    return
.label fact_02
12:    load_const.i32 const.1
13:    return
.endfun fact
```

---

## 16. Comparison with x86/ARM Assembly

| ORGASM Level-2           | x86/ARM Equivalent                | Notes                           |
| ------------------------ | --------------------------------- | ------------------------------- |
| `load_param.i32 param.0` | `mov reg, [esp+4]`                | Load i32 argument from stack    |
| `load_const.i32 const.1` | `mov reg, 1`                      | Immediate i32 value             |
| `add.i32`                | `add reg1, reg2`                  | i32 addition                    |
| `add.i32 [trap]`         | `add reg1, reg2; jo trap_handler` | i32 addition with overflow trap |
| `br label1, label2`      | `jz label1; jmp label2`           | Conditional branch              |
| `call fun.0`             | `call fun`                        | Function call by index          |
| `call import.0`          | `call [import]`                   | Call imported function          |
| `push_param`             | `push reg`                        | Prepare argument for call       |
| `push_self`              | `mov ecx, this`                   | Set up self for method call     |
| `load_symbol symbol.n`   | `mov reg, [symbol]`               | Load method symbol              |
| `invoke_method`          | `call [vtable+offset]`            | Dynamic method call             |
| `goto 22`                | `jmp 22`                          | Unconditional jump              |
| `store_value.i32 val.0`  | `mov [var], reg`                  | Store i32 to memory             |
| `load_value.i32 val.0`   | `mov reg, [var]`                  | Load i32 from memory            |
| `vadd.f32.4`             | `addps xmm1, xmm2`                | SIMD 4x32-bit float add (SSE)   |
| `vadd.f32.4`             | `vaddq_f32 v1, v2`                | SIMD 4x32-bit float add (NEON)  |
| `vload.128 addr.n`       | `movaps xmm, [addr]`              | SIMD 128-bit load (SSE)         |
| `vload.128 addr.n`       | `vld1q_f32 {addr}`                | SIMD 128-bit load (NEON)        |
| `atomic_add.i32 addr`    | `lock add [addr], reg`            | Atomic add (x86)                |
| `atomic_add.i32 addr`    | `ldadd reg, [addr]`               | Atomic add (ARMv8.1+)           |
| `atomic_cas.i32 addr`    | `cmpxchg [addr], reg`             | Compare-and-swap (x86)          |
| `atomic_cas.i32 addr`    | `cas reg, [addr]`                 | Compare-and-swap (ARM)          |
| `fence`                  | `mfence`                          | Memory barrier (x86)            |
| `fence`                  | `dmb ish`                         | Memory barrier (ARM)            |

---

## 17. Extensibility & Limitations

- Designed for direct mapping to bytecode or machine code.
- Symbolic names and addresses enable better debugging and optimization.
- SIMD operations are optional and only available on targets with SIMD support (SSE, AVX, NEON).
- Atomic operations require hardware support (x86: lock prefix, ARM: LSE or ldadd instructions).
- No explicit hardware I/O or interrupts; these are handled through imported functions.
- No explicit registers; stack and symbolic values are used for all computation.

### Future Extensible Types (Reserved)
- `struct`: Structure type, declared as `.struct <name> { ... }`, supports field access.
- `enum`: Enumeration type, declared as `.enum <name> { ... }`.
- `funptr`: Function pointer type, declared as `.val funptr <type>`, supports indirect calls.
### Error Handling and Undefined Behavior
- For type mismatches, out-of-bounds access, undefined instructions, etc., the recommended behavior is to throw an exception, trigger a trap, or terminate execution to avoid undefined behavior.

---

## 9. Intended Use

- As a lower-level IR for the NG compiler.
- For advanced analysis, optimization, and code generation.
- For teaching advanced compilation and code generation concepts.

---

## 10. Example Module

```plaintext
.module example01
.symbols [example01, id, n]
.const i32 0 // ZERO
.start none
00:    // ... start block instructions ...
.fun id
.param [n:i32]
00:    load_param.i32 param.0
01:    return
.endfun id
.endmodule
```

---

## 11. Debugging and Symbol Information

- The `.symbols` section provides a symbol table for debuggers and analyzers, mapping variables, functions, constants, etc. to source code.
- It is recommended to retain source line numbers, variable names, and other information in the IR for breakpoints, single-stepping, and static analysis.
- For complex scenarios, a `.debug` section can be added to record additional debugging metadata.

---

## 12. Example: SIMD Vector Addition

```plaintext
.module simd_demo
.const f32 1.0
.const f32 2.0
.const f32 3.0
.const f32 4.0
.array f32 [1.0, 2.0, 3.0, 4.0] [align=16]     // vec_a (16-byte aligned)
.array f32 [5.0, 6.0, 7.0, 8.0] [align=16]     // vec_b (16-byte aligned)
.array f32 [0.0, 0.0, 0.0, 0.0] [align=16]     // result (16-byte aligned)

.fun vector_add
.param []
00:    load_array.f32 array.0    // load vec_a[0] address
01:    vload.128                 // load 4 floats into SIMD register
02:    load_array.f32 array.1    // load vec_b[0] address
03:    vload.128                 // load 4 floats into SIMD register
04:    vadd.f32.4                // SIMD add: 4x32-bit floats
05:    load_array.f32 array.2    // load result[0] address
06:    vstore.128                // store 4 floats from SIMD register
07:    return
.endfun vector_add
```

---

## 13. Example: Lock-Free Counter with Atomics

```plaintext
.module atomic_demo
.val atomic.i32 0  // shared_counter

.fun increment_counter
.param []
00:    load_value.atomic.i32 val.0    // load atomic counter
01:    load_const.i32 const.1         // load 1
02:    atomic_add.i32 val.0           // atomic increment
03:    // Result is now in atomic.i32 format
04:    return
.endfun increment_counter

.fun safe_compare_swap
.param [expected:i32, new:i32]
00:    load_param.i32 param.0         // expected value
01:    load_param.i32 param.1         // new value
02:    load_symbol val.0              // address of atomic variable
03:    atomic_cas.i32 val.0           // compare-and-swap
04:    // Returns 1 if successful, 0 if failed
05:    return
.endfun safe_compare_swap
```

---

## 14. Example: Safe Arithmetic with Overflow Handling

```plaintext
.module safe_math_demo
.const i32 100
.const i32 200
.const i32 0

.fun safe_multiply
.param [a:i32, b:i32]
.val i32 0
00:    load_param.i32 param.0        // load first operand
01:    load_param.i32 param.1        // load second operand
02:    multiply.i32 [trap]           // multiply with overflow trap
03:    store_value.i32 val.0         // store result
04:    goto success
05:    // If overflow occurred, execution would trap before reaching here
06:    success:
07:    load_value.i32 val.0
08:    return
.endfun safe_multiply

.fun saturating_add
.param [a:i32, b:i32]
00:    load_param.i32 param.0        // load first operand
01:    load_param.i32 param.1        // load second operand
02:    add.i32 [saturate]            // add with saturation
03:    return                        // Returns max/min value on overflow
.endfun saturating_add
```

---

## 15. Comprehensive Example: Type Promotion, Reference Passing, and Method Call

```plaintext
.module demo
.symbols [demo, arr, sum, i, n, result, print]
.const i32 0 // ZERO
.const i32 1 // ONE
.array i32 [1, 2, 3, 4]
.val i32 0 // result
.fun sum
.param [arr:addr, n:i32]
00:    load_const.i32 const.0
01:    store_value.i32 val.0 // result = 0
02:    load_const.i32 const.0
03:    store_value.i32 val.1 // i = 0
04:    loop_start:
05:    load_value.i32 val.1 // i
06:    load_param.i32 param.1 // n
07:    gt.i32
08:    br loop_end, loop_body
09:    loop_body:
10:    load_value.i32 val.0 // result
11:    load_param.addr param.0 // arr
12:    load_value.i32 val.1 // i
13:    load_array.i32 array.0
14:    add.i32
15:    store_value.i32 val.0 // result += arr[i]
16:    load_value.i32 val.1
17:    load_const.i32 const.1
18:    add.i32
19:    store_value.i32 val.1 // i++
20:    goto loop_start
21:    loop_end:
22:    load_value.i32 val.0
23:    return
.endfun sum
.fun main
.param []
00:    load_array.addr array.0
01:    load_const.i32 const.1
02:    call fun.0 // sum
03:    store_value.i32 val.0 // result
04:    load_value.i32 val.0
05:    call import.0 // print
06:    return
.endfun main
.endmodule
```

---

## 13. Glossary

| Term                  | Description                                                                      |
| --------------------- | -------------------------------------------------------------------------------- |
| addr                  | Address/reference type, points to arrays, objects, strings, or composite data    |
| cast.<type>           | Type promotion/casting instruction, converts the top of stack to the type        |
| fun.0                 | Call label for the 0th function, numbered by definition order                    |
| array.n               | Reference to the nth array, numbered by definition order                         |
| param.n               | The nth parameter, numbered by parameter list order                              |
| val.n                 | The nth local variable, numbered by definition order                             |
| const.n               | The nth constant, numbered by definition order                                   |
| .symbols              | Symbol table section, records all identifiers for debugging, reflection, linking |
| .debug                | (Optional) Debug info section, records source line numbers, variable names, etc. |
| trap                  | Exception/trap behavior, prevents undefined or dangerous operations              |
| v128                  | 128-bit SIMD vector type (maps to SSE/NEON registers)                            |
| v256                  | 256-bit SIMD vector type (maps to AVX registers)                                 |
| v512                  | 512-bit SIMD vector type (maps to AVX-512 registers)                             |
| atomic.i32            | Atomic 32-bit integer for lock-free operations                                   |
| atomic.i64            | Atomic 64-bit integer for lock-free operations                                   |
| atomic.addr           | Atomic address type for lock-free pointer operations                             |
| vadd.f32.4            | SIMD vector add: 4x32-bit floats                                                 |
| vload.128             | SIMD 128-bit vector load instruction                                             |
| vstore.128            | SIMD 128-bit vector store instruction                                            |
| vload.256             | SIMD 256-bit vector load instruction                                             |
| vstore.256            | SIMD 256-bit vector store instruction                                            |
| atomic_add            | Atomic add instruction for thread-safe operations                                |
| atomic_sub            | Atomic subtract instruction for thread-safe operations                           |
| atomic_and            | Atomic AND instruction for thread-safe operations                                |
| atomic_or             | Atomic OR instruction for thread-safe operations                                 |
| atomic_cas            | Atomic compare-and-swap instruction                                              |
| fence                 | Memory barrier/fence instruction                                                 |
| load_volatile         | Volatile load instruction (prevents optimization)                                |
| store_volatile        | Volatile store instruction (prevents optimization)                               |
| tuple_create          | Create tuple with specified size in bytes                                        |
| tuple_get.type offset | Load value from tuple at specified byte offset                                   |
| tuple_set.type offset | Store value to tuple at specified byte offset                                    |
| [wrap]                | Overflow mode: wrap around on overflow (default for unsigned)                    |
| [trap]                | Overflow mode: trap on overflow (safe arithmetic)                                |
| [saturate]            | Overflow mode: saturate to max/min value on overflow                             |
| [align=n]             | Alignment attribute for data declarations                                        |
| [volatile]            | Volatile attribute to prevent optimization of memory accesses                    |
| SIZEOF_I32            | Constant for i32 size (4 bytes) in tuple offset calculations                     |
| SIZEOF_F64            | Constant for f64 size (8 bytes) in tuple offset calculations                     |
| SIZEOF_ADDR           | Constant for address size (8 bytes on 64-bit) in tuple offset calculations       |

- [x86 Assembly Guide](https://www.cs.virginia.edu/~evans/cs216/guides/x86.html)
- [ARM Assembly Basics](https://azeria-labs.com/writing-arm-assembly-part-1/)
- NG Language Documentation

---

*This document is AI-generated and should be reviewed for accuracy and completeness before use in production or documentation.*
