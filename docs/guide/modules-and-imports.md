# Modules and Imports

NG uses a file-based module system. Every `.ng` file is a module, and you control visibility with `export` and `import`.

## Module Basics

### Every File is a Module

When you create `math.ng`:

```ng
// math.ng — this is a module
fun add(x: i32, y: i32) -> i32 => x + y;
```

By default, nothing is visible to other modules. You must **export** what you want to expose.

## Exporting

### The `export` Keyword

```ng
// math.ng
export fun add(x: i32, y: i32) -> i32 => x + y;
export fun multiply(x: i32, y: i32) -> i32 => x * y;

// This is private — not visible outside this file
fun helper(x: i32) -> i32 => x + 1;
```

### Wildcard Export

Use `exports *` in the module declaration to export everything:

```ng
module math exports *;

fun add(x: i32, y: i32) -> i32 => x + y;
fun multiply(x: i32, y: i32) -> i32 => x * y;
```

All top-level bindings are now exported.

### Module Declaration Syntax

```ng
module <name> exports <list>;
```

The module name is used for qualified imports.

## Importing

### Wildcard Import

Import all exported symbols into the current namespace:

```ng
// main.ng
import math (*);

add(1, 2);          // OK
multiply(3, 4);     // OK
```

### Named Import

Import specific symbols:

```ng
import math (add);   // only add is imported
add(1, 2);           // OK
// multiply(3, 4);   // ERROR: not imported
```

### Qualified Import

Use a module prefix to avoid name conflicts:

```ng
import math;

val result = math::add(1, 2);
```

### Mixed Import

```ng
import math (add);           // unqualified add
import math;                 // also provides math:: prefix
```

### Renaming Imports

You can alias imports to avoid name clashes:

```ng
import std.prelude (print as show);
show("Hello!");   // prints "Hello!"
```

## Module Resolution

### Search Paths

When you `import math`, NG searches for `math.ng` in:

1. The directory of the importing file
2. The `lib/` directory (standard library)
3. The `../lib/` directory (relative to binary)

### Module Paths with Dots

Dots in module names map to directory paths:

```ng
import std.prelude;   // loads lib/std/prelude.ng
import std.string;    // loads lib/std/string.ng
import std.io;        // loads lib/std/io.ng
```

## Circular Imports

Circular imports are **not supported**. If module A imports module B, module B cannot import module A (directly or transitively).

## Standard Library Import

The `std.prelude` module is implicitly imported into every NG file. Other standard library modules must be explicitly imported:

```ng
import std.io;

val content = readFile("data.txt");
import std.string;

val lines = split(content, "\n");
```

## Bytecode Module Artifacts (`.ngo`)

NG can compile modules to bytecode for faster loading:

```bash
# Compile to bytecode
./build/ngi --emit-ngo math.ngo example/math.ng

# Run referencing the .ngo artifact
./build/ngi example/main.ng
```

The module loader automatically:
1. Checks for a `.ngo` file alongside the source
2. Verifies the source hash matches (detects staleness)
3. Falls back to source compilation if the artifact is stale or incompatible

## Exporting Types

Types, functions, and even trait implementations can be exported:

```ng
// shapes.ng
module shapes exports *;

type Point {
    x: i32;
    y: i32;
}

export fun origin() -> Point {
    return new Point { x: 0, y: 0 };
}

// Importing export impls
module math exports *;
impl Show for i32 { ... }
```

When you import `shapes`, you get access to the `Point` type and the `origin` function.

## What's Next?

Continue to [Generics](generics.md) to learn about writing reusable, type-parameterized code.

> **Try it:** `example/08.imports.ng` — Basic imports
> **Try it:** `example/13.import_std_prelude.ng` — Importing standard library
> **Try it:** `example/56.stdlib_modules.ng` — Multiple module interactions