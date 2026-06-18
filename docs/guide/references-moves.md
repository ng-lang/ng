# References, Moves & Ownership

NG uses a **value semantics** model by default — variables are copied on assignment — with explicit **references** and **moves** for shared ownership and efficient transfers.

## Value Semantics (Default)

By default, bindings are independent copies:

```ng
val a = [1, 2, 3];
val b = a;        // b is a copy of a
b[0] := 99;
print(a[0]);      // 1 (a is unchanged)
```

This applies to function arguments and return values too:

```ng
fun modify(arr: [i32]) {
    arr[0] := 99;   // modifies the local copy only
}

val arr = [1, 2, 3];
modify(arr);
print(arr[0]);     // 1 (unchanged)
```

## Reference Types: `ref<T>`

Create a reference to share access to a value:

```ng
val x = 42;
val r: ref<i32> = ref x;   // r refers to x
x = 43;
print(*r);                  // 43 (reads through the reference)
```

### Creating References

Use the `ref` operator on a mutable place:

```ng
val x = 1;
val r = ref x;     // reference to x
```

### Reading Through References

Dereference with `*`:

```ng
print(*r);         // reads the value
```

### Writing Through References

Use deref assignment `:=`:

```ng
*r := 10;          // writes through the reference
print(x);          // 10
```

### Reference Parameters

Functions can accept references:

```ng
fun swap<T>(a: ref<T>, b: ref<T>) {
    val tmp = move *a;
    *a := move *b;
    *b := move tmp;
}

val x = 1;
val y = 2;
swap(ref x, ref y);
print(x, y);       // 2, 1
```

## Move Semantics

`move` transfers ownership of a value, invalidating the original location:

```ng
val a = [1, 2, 3];
val b = move a;     // a is now invalid
// print(a[0]);     // ERROR: use after move
print(b[0]);        // 1
```

### Move Dereference

Combine `move` and `*` to move a value out of a reference:

```ng
fun take<T>(dest: ref<T>, src: ref<T>) {
    *dest := move *src;   // moves value from src into dest
}
```

### Runtime Move Checking

The interpreter tracks moved values at runtime and throws an error if you try to read a moved location before reassigning it:

```ng
val x = move a;    // a is now "moved"
// print(a);       // Runtime error: use after move
a = [4, 5, 6];     // reassign — OK now
print(a);          // OK
```

## References to Object Properties

References can point to object fields:

```ng
type Point { x: i32; y: i32; }
val p = new Point { x: 10, y: 20 };
val rx = ref p.x;
*rx := 99;
print(p.x);        // 99
```

## Partial Moves

You can move individual fields from an object while leaving others accessible:

```ng
type Person {
    name: string;
    age: i32;
}

val p = new Person { name: "Alice", age: 30 };

val name = move p.name;   // p.name is moved
// print(p.name);         // ERROR: field was partially moved
print(p.age);             // OK: age is still accessible
```

### Restoration After Partial Move

Writing to a partially-moved field restores full access:

```ng
p.name = "Bob";           // restores p.name
print(p.name);            // OK now
```

### Partial Move Tracking in Objects

The type checker tracks which fields have been partially moved and prevents reads from moved fields. This extends to nested objects and tuple fields:

```ng
val nested = new Wrapper { inner: new Inner { x: 1, y: 2 } };
val x = move nested.inner.x;
// print(nested.inner.x); // ERROR: partially moved
print(nested.inner.y);    // OK
```

## References and Aliasing

### Direct Ref Aliasing

A `ref` creates a borrow that remains active within its lexical scope. While a direct ref is alive, neither the original value nor the ref can be invalidated by moves:

```ng
val x = 1;
val r = ref x;
// val y = move x;       // ERROR: can't move while ref is active
print(*r);                // OK — ref is still usable
```

### Ref Borrow Ends at Scope Boundary

```ng
val x = 1;
{
    val r = ref x;        // borrow starts
    print(*r);
}                         // borrow ends
val y = move x;           // OK — ref is gone
```

## Heap-Allocated Objects (`new`)

Objects created with `new` are heap-allocated and alias by reference:

```ng
val a = new Point { x: 1, y: 2 };
val b = a;                // b aliases the same heap object
a.x = 10;
print(b.x);               // 10 (shared)
```

## What's Next?

Continue to [Traits](traits.md) to learn about NG's trait system for abstraction and polymorphism.

> **Try it:** `example/22.ref_move_swap.ng` — Reference swap
> **Try it:** `example/23.ref_places.ng` — References to places
> **Try it:** `example/24.move_value_semantics.ng` — Move semantics
> **Try it:** `example/50.partial_move.ng` — Partial moves
> **Try it:** `example/51.partial_move_drop.ng` — Partial moves with Drop