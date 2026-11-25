# Standard Library

The standard library lives in `lib/std/` and is imported by module name.
Value-typed sequences, a recursive-enum `List`, and a GC-free memory module.

## `prelude`

```ng
import prelude;
```

Printing and assertions — `print` (overloaded for i64, i8, i16, i32, u8,
u16, u32, u64, string, bool, f64, f32), `assert(condition: bool) -> unit`,
and `not(value: bool) -> bool`.

Numeric and string conversions:

```ng
print(toFloat(3));          // f64 from i64 (also toFloat(u64))
print(toSigned(5u64));      // i64 from u64
print(toString(42));        // also i32, u64, f64, bool overloads
print(canParseInt("12"));   // true
print(parseRawInt("-7"));   // i64; traps on invalid input
```

Result-based integer parsing — `parseInt(text: string) -> Result<i64, string>`
returns `Result.Ok` when `canParseInt` holds and `Result.Err(text)` otherwise.

Generic enums:

```ng
enum Option<T> { Some(value: T), None }
enum Result<T, E> { Ok(value: T), Err(error: E) }
```

Combinators over them, each taking a closure:

```ng
let some: Option<i64> = Option.Some(4);
let doubled: Option<i64> = optMap(some, |v: i64| v * 2);              // Some(8)
let chained: Option<i64> = optAndThen(some, |v: i64| Option.Some(v + 1));
let fallback: Option<i64> = optOrElse(none, || Option.Some(7));

let parsed: Result<i64, string> = parseInt("42");
let bumped: Result<i64, string> = resultMap(parsed, |v: i64| v + 1);  // Ok(43)
let recovered: Result<i64, string> = resultOrElse(error, |m: string| Result.Ok(0));
```

`resultAndThen(value: Result<T, E>, f: |T| -> Result<U, E>) -> Result<U, E>`
threads the Ok path and short-circuits the Err path.

Traits declared here so any module can use them after importing the prelude:
`Copy`, `Clone` (with `clone(self: Self ref) -> Self`), `Eq` (with
`eq(self: Self ref, other: Self ref) -> bool`), and `Show` (with
`show(self: Self ref) -> string`). Auto traits `Fn` and `FnOnce` mark callable
closure types. The const predicate `is_ref` is exported here too.

The prelude re-exports the `io` and `string` surfaces, so `import prelude;`
alone makes those functions available.

## `string`

```ng
print(length("hello"));              // 5
print(charAt("hello", 1));           // "e"
print(substring("hello", 1, 3));     // "el"
print(trim("  hi  "));               // "hi"
print(join(split("a,b,c", ","), "|"));   // "a|b|c"
print(contains("hello", "ell"));     // true
print(replace("aaa", "a", "b"));     // "bbb"
print(startsWith("hello", "he"));    // true
print(endsWith("hello", "lo"));      // true
print(toUpper("hi"));                // "HI"
print(toLower("HI"));                // "hi"
print(regexMatch("a1b2", "a.b."));   // true
```

Signatures:

```ng
export native fun split(text: string, delimiter: string) -> array<string>;
export native fun join(items: array<string>, separator: string) -> string;
export native fun trim(text: string) -> string;
export native fun contains(haystack: string, needle: string) -> bool;
export native fun replace(text: string, old: string, replacement: string) -> string;
export native fun startsWith(text: string, prefix: string) -> bool;
export native fun endsWith(text: string, suffix: string) -> bool;
export native fun toUpper(text: string) -> string;
export native fun toLower(text: string) -> string;
export native fun length(text: string) -> i64;
export native fun charAt(text: string, index: i64) -> string;
export native fun substring(text: string, start: i64, end: i64) -> string;
export native fun regexMatch(text: string, pattern: string) -> bool;
```

## `seq`

Array helpers as free functions over value-typed arrays
(`sum`, `arrayContains` and `reverse` are i64-specific):

```ng
let xs = [1, 2, 3, 4];
print(len(xs));                     // 4 — len<T>(array<T>) -> i64
print(sum(xs));                     // 10 — array<i64>
print(arrayContains(xs, 3));        // true — (array<i64>, i64) -> bool
print(len(reverse(xs)));            // 4 — array<i64> -> array<i64>
```

Higher-order combinators take closures:

```ng
let doubled = map(xs, |v: i64| v * 2);          // array<i64>
let big     = filter(xs, |v: i64| v > 2);       // array<i64>
let total   = fold(xs, 0, |acc: i64, v: i64| acc + v);   // i64
```

`pushBack` appends in place to a uniquely-owned array binding; when a copy of
the binding exists the checker routes the call to a copy-on-write helper, so
array value semantics always hold:

```ng
let mut grow: array<i64> = [1];
pushBack(ref mut grow, 5);     // grow is [1, 5]
```

Signatures:

```ng
export native fun len<T>(values: array<T>) -> i64;
export native fun sum(values: array<i64>) -> i64;
export native fun arrayContains(values: array<i64>, value: i64) -> bool;
export native fun reverse(values: array<i64>) -> array<i64>;
export native fun pushBack<T>(values: array<T> ref mut, value: T) -> unit;
export fun map<T, U>(values: array<T>, f: |T| -> U) -> array<U>;
export fun filter<T>(values: array<T>, pred: |T| -> bool) -> array<T>;
export fun fold<T, U>(values: array<T>, initial: U, f: |U, T| -> U) -> U;
```

To measure the length of a `string` array such as a `split` result, import
`seq` alongside `string` — `string` supplies no `len`.

## `list`

The recursive-enum sequence; node tails are `Box<List<T>>` heap cells, so box
reads, clones, and drops are well-defined:

```ng
enum List<T> {
    Cons(head: T, tail: Box<List<T>>),
    Nil,
}
```

Traversal, all taking `ref<List<T>>`:

```ng
let made = listFrom([1, 2, 3]);
print(length(ref made));        // 3
print(get(ref made, 0));        // 1
print(contains(ref made, 2));   // true
```

`pushFront`, `append` and `reverseList` are immutable builders — each returns a
fresh sequence and leaves the receiver unchanged:

```ng
let grown    = append(ref made, 7);      // made stays length 3
let front    = pushFront(ref made, 5);   // 5 at index 0
let reversed = reverseList(ref made);    // [3, 2, 1]
```

`pushBack` mutates through a receiver binding, rebuilding the spine and writing
the fresh list back:

```ng
let mut items = listFrom([1, 2, 3]);
pushBack(ref mut items, 7);     // items is now length 4
```

It is O(n) per push; `seq.pushBack` is the amortized-O(1) in-place builder for
arrays.

Signatures:

```ng
export fun length<T>(list: ref<List<T>>) -> i64;
export fun get<T>(list: ref<List<T>>, index: i64) -> T;
export fun contains<T>(list: ref<List<T>>, value: T) -> bool;
export fun listFrom<T>(values: array<T>) -> List<T>;
export fun pushFront<T>(list: ref<List<T>>, value: T) -> List<T>;
export fun append<T>(list: ref<List<T>>, value: T) -> List<T>;
export fun reverseList<T>(list: ref<List<T>>) -> List<T>;
export fun pushBack<T>(list: List<T> ref mut, value: T) -> unit;
```

Constructing nodes directly uses the `memory` box constructor:

```ng
import memory;
import list;

let empty: List<i64> = List.Nil;
let one: List<i64> = List.Cons(1, new<List<i64>>(empty));
```

## `memory`

A generic affine `Box<T>` over a session-owned native handle table. There is
no GC; the owning `Box<T>` releases its cell drop-exactly-once under the
checker's affine move rules.

```ng
import prelude;
import memory;

fun main() -> unit {
    let cell = box(7);
    write(ref cell, 9);
    print(read(ref cell));          // 9
    print(outstanding() == 1);      // true
}
```

Signatures:

```ng
export struct Box<T> { handle: Handle }
export fun new<T>(value: T) -> Box<T>;
export fun box<T>(value: T) -> Box<T>;
export fun read<T>(value: Box<T> ref) -> T;
export fun write<T>(value: Box<T> ref, updated: T) -> unit;
export native fun outstanding() -> i64;
```

`read` deep-copies out and `write` snapshots in, so the cell is always the sole
live view. `Box<T>` also implements `Clone` (`clone()` deep-copies into a fresh
cell) and `Drop` (releases the handle). Payloads ride a 64-bit word slot:
integers, bool, unit, string, arrays, structs, enums, tuples, f32 and f64 all
round-trip, floats bit-preservingly.

`Cell<T>` is an affine owning handle with interior mutability: `cellGet` and
`cellSet` take a *shared* `ref`, so any number of co-borrowers may read and
write the same cell without a `ref mut`. It implements `Clone`: `clone` and
`.clone()` deep-copy into a fresh cell.

```ng
let counter = newCell(0);
let left = ref counter;
let right = ref counter;
cellSet(right, cellGet(right) + 2);
print(cellGet(left));       // 2
```

```ng
export struct Cell<T> { handle: Handle }
export fun newCell<T>(value: T) -> Cell<T>;
export fun cellGet<T>(cell: Cell<T> ref) -> T;
export fun cellSet<T>(cell: Cell<T> ref, updated: T) -> unit;
```

The underlying handle natives are exported as `allocate<T>`,
`load<T>`, `store<T>` and `release`; the generic call-site inference cannot
carry a type parameter across a callee boundary, so `new`, `read` and `write`
pass the type argument explicitly.

See `example/box.ng`, `example/cell.ng`, and `example/heap_box.ng`.

## `io`

```ng
writeFile("/tmp/ngtest.txt", "content");
print(readFile("/tmp/ngtest.txt"));    // content
print(currentExecutablePath() != "");
```

Signatures:

```ng
export native fun readLine() -> string;
export native fun readFile(path: string) -> string;
export native fun writeFile(path: string, content: string) -> unit;
export native fun currentExecutablePath() -> string;
```

## `system`

`system(command) -> i64` executes the command through `/bin/sh -c` and returns
only the command's exit status.

`systemOutput(command) -> string` executes the command the same way but
returns the captured output; on failure it appends a trailing `[exit N]`
marker carrying the nonzero exit status.

```ng
print(system("exit 3"));              // 3
print(systemOutput("echo hello"));    // hello
print(systemOutput("exit 4"));        // [exit 4]
```

Because both bindings execute through `/bin/sh -c`, callers must not
interpolate untrusted values (user input, file contents, environment data)
into command strings. There is no shell-escaping API and no argument-vector
variant in the stdlib. A safe approach is to validate untrusted data against
a strict allowlist (for example, matching an exact constant or a narrow
pattern such as a fixed set of literals) before interpolating it; correct
single-quote shell escaping of an interpolated value is another safe approach
when applied to every interpolated segment (as in `example/ng_ide.ng`). Raw
interpolation without either is unsafe.

## `imgui`

The Dear ImGui binding (`lib/std/imgui.ng`) — see
[ImGui Integration](/guide/imgui-integration).

## Umbrella

`lib/std.ng` contains `import prelude;` and so re-exports the prelude surface;
individual modules are the usual import path (`import string;`, `import seq;`,
...).

Next: [Memory Management](/guide/memory-management).
