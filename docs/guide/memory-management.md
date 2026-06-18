# Memory Management

NG uses automatic memory management with compile-time ownership tracking. This chapter explains how memory works in NG.

## Value Semantics (Stack)

By default, values live on the **stack** and are copied on assignment:

```ng
val a = 42;
val b = a;    // a is copied into b (both on stack)
```

Primitive types (`i32`, `f64`, `bool`, `unit`) and fixed-size arrays are always stack-allocated and cheap to copy.

## Heap Allocation with `new`

The `new` keyword allocates on the **managed heap** and returns a `ref<T>`:

```ng
type Point { x: i32; y: i32; }

val p = new Point { x: 1, y: 2 };
// p: ref<Point> — heap-allocated
```

### Reference Semantics

Heap objects are reference-counted. Assignment shares the reference:

```ng
val a = new Point { x: 1, y: 2 };
val b = a;         // b points to the same heap object
a.x := 10;
print(b.x);        // 10 (same object)
```

## Automatic Deallocation

When all references to a heap object go out of scope, the memory is automatically reclaimed:

```ng
fun example() {
    val p = new Point { x: 1, y: 2 };
    // p is alive here
}
// p goes out of scope → memory freed
```

## Garbage Collection for Cycles

NG's managed heap includes a **tracing garbage collector** that detects and collects unreachable cycles:

```ng
type Node {
    value: i32;
    next: ref<Node>;
}

fun cycle() {
    val a = new Node { value: 1, next: null };
    val b = new Node { value: 2, next: a };
    a.next = b;   // cycle: a → b → a
}
// When function exits, both a and b are unreachable
// The GC collects the cycle even with reference counting
```

### GC Safety

The GC runs automatically when:
- The heap grows beyond a threshold
- Explicit collection is triggered
- During idle time

It traces from **root references** (globals, call frames, operand stacks) and sweeps unmarked cells.

## Drop and RAII

Types implementing the `Drop` trait get a **finalizer** called when the value is deallocated:

```ng
trait Drop {
    fun drop(self: ref<Self>) -> unit;
}

type FileHandle {
    fd: i32;
}

impl Drop for FileHandle {
    fun drop(self: ref<Self>) {
        nativeClose(self.fd);     // release OS resource
    }
}

fun readFile(path: string) -> FileHandle {
    val fd = nativeOpen(path);
    return FileHandle { fd: fd };
}
// FileHandle.drop() is called automatically
```

### Drop Order

Drop runs when:
- A local variable goes out of scope
- A heap object is collected by the GC
- An object property is overwritten (old value is dropped)

### Rule: No Copy + Drop

If a type implements `Drop`, it **cannot** derive `Copy`. This ensures that the finalizer runs exactly once:

```ng
type Handle {
    fd: i32;
}

impl Drop for Handle { ... }

// handle is moved, not copied
fun example() {
    val h1 = Handle { fd: 1 };
    val h2 = move h1;     // transfer ownership
    // h1.drop() does NOT run — h2.drop() runs when h2 goes out of scope
}
```

## Move Semantics

Use `move` to transfer ownership without copying:

```ng
val arr = [1, 2, 3, 4, 5];
val moved = move arr;
// arr is now invalid — don't use it
print(moved[0]);  // OK
```

### Use-After-Move Detection

The runtime detects use-after-move and raises an error:

```ng
val x = [1, 2, 3];
val y = move x;
// print(x[0]);   // Runtime error: use after move
x = [4, 5, 6];    // Reassign — OK now
print(x[0]);      // OK
```

### Move in Function Arguments

```ng
fun takeOwnership(arr: [i32]) {
    // arr owns the data
}

val data = [1, 2, 3];
takeOwnership(move data);   // transfer ownership
// data is now invalid
```

## Partial Moves

Individual fields of an object can be moved independently:

```ng
type Person { name: string; age: i32; }

val p = Person { name: "Alice", age: 30 };
val name = move p.name;     // only name is moved
// print(p.name);            // ERROR: partially moved
print(p.age);               // OK: age is still valid

p.name = "Bob";             // restore the field
print(p.name);              // OK now
```

### Nested Partial Moves

```ng
type Inner { x: i32; y: i32; }
type Outer { inner: Inner; }

val o = Outer { inner: Inner { x: 1, y: 2 } };
val x = move o.inner.x;
print(o.inner.y);   // OK: y is still accessible
// print(o.inner.x); // ERROR: partially moved
```

## Smart Pointers

The `std.memory` module provides smart pointer types:

```ng
import std.memory;

// Unique ownership
val ptr: UniquePtr<i32> = nativeMalloc(8);
// ptr is automatically freed via Drop
```

## Clone

For types deriving `Clone`, use `.clone()` for explicit deep copies:

```ng
type Point: derive(Clone) {
    x: i32;
    y: i32;
}

val a = Point { x: 1, y: 2 };
val b = a.clone();   // explicit deep copy
```

## Memory Safety Summary

| Mechanism | Enforced By | Purpose |
|---|---|---|
| Value semantics | Compiler | Default copy behavior |
| Reference counting | Runtime | Shared heap ownership |
| GC cycle detection | Runtime | Collect cyclic garbage |
| Use-after-move check | Runtime | Prevent invalid access |
| Partial move tracking | Type checker | Field-level ownership |
| Drop finalizers | Runtime | Resource cleanup |
| No Copy + Drop | Type checker | Ensure unique finalization |

## What's Next?

Continue to [ImGui Integration](imgui-integration.md) for GUI programming with NG.

> **Try it:** `example/39.drop_raii.ng` — Drop and RAII
> **Try it:** `example/41.drop_smart_pointer.ng` — Smart pointers with Drop
> **Try it:** `example/50.partial_move.ng` — Partial move semantics
> **Try it:** `example/51.partial_move_drop.ng` — Partial moves with Drop