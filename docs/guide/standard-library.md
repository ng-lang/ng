# Standard Library

NG includes a small but capable standard library organized into modules. The library is written in NG itself, with some primitives backed by native (C++) implementations.

## Module Overview

```
lib/
├── std.ng              # Root module re-exporting all sub-modules
└── std/
    ├── prelude.ng       # Auto-imported basics (print, assert, not, len)
    ├── io.ng            # File and terminal I/O
    ├── string.ng        # String manipulation
    ├── array.ng         # Array operations
    ├── list.ng          # Linked list
    ├── seq.ng           # Sequence operations
    ├── tuple.ng         # Enhanced tuple operations
    └── memory.ng        # Memory management utilities
```

## Prelude (Auto-Imported)

The `std.prelude` module is **implicitly imported** into every NG file. It provides:

### `print<T...>(args: T...)`

Prints values to stdout. Accepts any number of arguments of any type:

```ng
print("Hello");                // "Hello"
print(1, 2, 3);                // "1, 2, 3"
print("value:", 42, true);     // "value: 42, true"
```

### `assert<T...>(condition: T...)`

Asserts a condition is true. Crashes with a message if the first argument is falsy:

```ng
assert(1 + 1 == 2);           // OK
assert(len([1, 2]) == 2);     // OK
// assert(false);              // Runtime error: assertion failed
```

### `not(value: bool) -> bool`

Logical negation:

```ng
val result = not(x > 0);       // equivalent to !(x > 0)
```

### `len<T>(xs: string | vector<T>) -> u32`

Returns the length of a string or the number of elements in an array:

```ng
print(len("hello"));            // 5
print(len([1, 2, 3]));          // 3
```

## I/O (`std.io`)

```ng
import std.io;
```

| Function | Description |
|---|---|
| `readLine() -> string` | Read a line from stdin |
| `readFile(path: string) -> string` | Read entire file contents |
| `writeFile(path: string, content: string) -> unit` | Write string to file (overwrites) |
| `currentExecutablePath() -> string` | Get path to the running ngi binary |
| `runNgi(path: string) -> string` | Execute ngi on a file and return output |

### Example

```ng
import std.io;

val content = readFile("data.txt");
print("File has", len(content), "characters");
```

## String Operations (`std.string`)

```ng
import std.string;
```

| Function | Description |
|---|---|
| `split(s: string, delimiter: string) -> vector<string>` | Split by delimiter |
| `join(items: vector<string>, separator: string) -> string` | Join with separator |
| `trim(s: string) -> string` | Remove leading/trailing whitespace |
| `contains(haystack: string, needle: string) -> bool` | Check substring |
| `replace(s: string, old: string, rep: string) -> string` | Replace all occurrences |
| `startsWith(s: string, prefix: string) -> bool` | Prefix check |
| `endsWith(s: string, suffix: string) -> bool` | Suffix check |
| `toUpper(s: string) -> string` | Convert to uppercase |
| `toLower(s: string) -> string` | Convert to lowercase |
| `regexMatch(value: string, pattern: string) -> bool` | Regex match |

### Example

```ng
import std.string;

val msg = "Hello, World!";
val parts = split(msg, ", ");
print(parts[0]);                    // "Hello"
print(startsWith(msg, "Hello"));    // true
print(toUpper(msg));                // "HELLO, WORLD!"
```

## Array Operations (`std.array`)

```ng
import std.array;
```

| Function | Description |
|---|---|
| `reverse<T>(xs: vector<T>) -> vector<T>` | Return reversed copy |

> **Note:** Only `reverse` is currently available. Sort, isEmpty, and contains are not yet implemented in the stdlib.

### Example

```ng
import std.array;

val arr = [3, 1, 4, 1, 5];
val rev = reverse(arr);
print(rev);                         // [5, 1, 4, 1, 3]
```

## List Operations (`std.list`)

```ng
import std.list;
```

The `std.list` module provides a doubly-linked list:

| Function | Description |
|---|---|
| `list<T>() -> List<T>` | Create empty list |
| `listof<T>(args: T...) -> List<T>` | Create list from elements |
| `pushBack<T>(list: ref<List<T>>, value: T) -> unit` | Append to end |
| `append<T>(list: ref<List<T>>, value: T) -> unit` | Append (alias for pushBack) |
| `get<T>(list: ref<List<T>>, index: i32) -> T` | Get element by index |

### Example

```ng
import std.list;

val lst = listof(1, 2, 3, 4, 5);
print(get(lst, 2));     // 3
```

## Sequence Operations (`std.seq`)

```ng
import std.seq;
```

The `std.seq` module defines the `Sequence<T>` trait for indexable types:

```ng
trait Sequence<T> {
    fun size(self: ref<Self>) -> u32;
    fun get(self: ref<Self>, index: i32) -> T;
}
```

Both `vector<T>` and `List<T>` implement `Sequence<T>`.

## Enhanced Tuple Operations (`std.tuple`)

```ng
import std.tuple;
```

| Function | Description |
|---|---|
| `is_tuple<T>` | Const predicate: true if T is a tuple |
| `tuple_size<T>` | Const generic: number of elements |
| `tuple_element<T, I>` | Type of the I-th element |
| `tuple_concat<A, B>` | Concatenate two tuple types |

See [Advanced Generics](advanced-generics.md) for details.

### Example

```ng
import std.tuple;

val t = (1, "hello", 3.14);
const if (is_tuple<typeof(t)>) {
    type Second = tuple_element<typeof(t), 1>;
    // Second is string
}
```

## Memory Management (`std.memory`)

```ng
import std.memory;
```

Utilities for manual memory handling:

| Type / Function | Description |
|---|---|
| `UniquePtr<T>` | Unique ownership pointer (opaque native type) |
| `nativeMalloc<T>(size: i32) -> UniquePtr<T>` | Allocate memory |
| `nativeFree<T>(ptr: UniquePtr<T>) -> unit` | Free memory |
| `nativeOutstandingAllocations() -> i32` | Debug: check live allocations |
| `gcFree() -> unit` | Trigger garbage collector |

```ng
import std.memory;

val ptr = nativeMalloc<i32>(8);
// ptr is automatically freed when it goes out of scope
// or you can call nativeFree(ptr) explicitly
```

## Ranges and Slices (Language Built-in)

Ranges and slices are part of the language syntax, not the stdlib:

```ng
val r1 = 0..10;                  // exclusive range: 0-9
val r2 = 0..=10;                 // inclusive range: 0-10
val arr = [0, 1, 2, 3, 4, 5];
val slice = arr[1..4];           // span: [1, 2, 3]
val materialized = [...arr[..3]]; // materialize slice to vector
```

See [Data Structures](data-structures.md) for more details.

## What's Next?

Continue to [ORGASM Backend](orgasm-backend.md) to learn about bytecode compilation and the VM internals.

> **Try it:** `example/18.stdlib_basics.ng` — Standard library basics
> **Try it:** `example/56.stdlib_modules.ng` — Using multiple stdlib modules
> **Try it:** `example/57.ranges_slicing_pipeline.ng` — Range and slicing pipeline
> **Try it:** `example/59.std_list_sequence.ng` — List and sequence operations