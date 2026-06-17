# Traits

Traits are NG's mechanism for defining shared behavior across types. They are similar to interfaces in Java or type classes in Haskell.

## Defining a Trait

```ng
trait Show {
    fun show(self: ref<Self>) -> string;
}
```

- `trait Show` — declares a trait named `Show`
- `fun show(self: ref<Self>) -> string` — declares a required method
- `Self` (capital S) is the type that implements this trait

### Traits with Multiple Methods

```ng
trait Comparable {
    fun lessThan(self: ref<Self>, other: ref<Self>) -> bool;
    fun equals(self: ref<Self>, other: ref<Self>) -> bool;
}
```

## Implementing a Trait

Use `impl <Trait> for <Type>`:

```ng
type Point { x: i32; y: i32; }

impl Show for Point {
    fun show(self: ref<Self>) -> string {
        return "Point(" + self.x + ", " + self.y + ")";
    }
}

val p = new Point { x: 3, y: 4 };
print(p.show());  // "Point(3, 4)"
```

## Calling Trait Methods

Once implemented, trait methods are available on instances of the type:

```ng
val s: string = point.show();
```

You can also call qualified trait methods to disambiguate:

```ng
print(Show::show(point));   // qualified call
```

## Generic Trait Bounds

Traits serve as **bounds** on generic type parameters, similar to interfaces in Java or constraints in Rust:

```ng
fun printIt<T: Show>(value: ref<T>) {
    print(value.show());
}

printIt(point);   // works because Point implements Show
```

### Multiple Bounds

```ng
fun compareAndShow<T: Show + Comparable>(a: ref<T>, b: ref<T>) {
    print(a.show());
    if (a.lessThan(b)) {
        print("less");
    }
}
```

## Supertraits

A trait can extend another trait, inheriting its requirements:

```ng
trait Show {
    fun show(self: ref<Self>) -> string;
}

trait Log: Show {                     // Log requires Show
    fun log(self: ref<Self>) -> string {
        return "[LOG] " + self.show(); // uses show() from supertrait
    }
}

impl Show for Point { ... }
impl Log for Point { ... }             // must implement Show AND Log (or use defaults)
```

## Default Methods

Traits can provide default implementations:

```ng
trait Greeter {
    fun greet(self: ref<Self>) -> string;
    fun greetLoudly(self: ref<Self>) -> string {
        return self.greet() + "!!!";   // default implementation using required method
    }
}

impl Greeter for Point {
    fun greet(self: ref<Self>) -> string {
        return "Hello from Point";
    }
    // greetLoudly uses the default
}

print(point.greetLoudly());  // "Hello from Point!!!"
```

Implementations can override defaults:

```ng
impl Greeter for Point {
    fun greet(self: ref<Self>) -> string => "Hello";
    fun greetLoudly(self: ref<Self>) -> string => "HELLO!!!";  // overrides default
}
```

## Inherent Methods vs Trait Methods

Methods defined directly on a type (inherent) take precedence over trait methods with the same name:

```ng
type Counter {
    value: i32;
}

// Inherent method
fun get(self: ref<Counter>) -> i32 => self.value;

trait Accessor {
    fun get(self: ref<Self>) -> i32 => 0;  // default
}

impl Accessor for Counter {
    // The inherent get() takes precedence over trait get()
}

val c = new Counter { value: 42 };
print(c.get());  // 42 (inherent method wins)
```

## Trait Objects: `ref dyn Trait`

Trait objects enable **dynamic dispatch** for heterogeneous collections and interfaces:

```ng
trait Show { fun show(self: ref<Self>) -> string; }

type Point { x: i32; y: i32; }
impl Show for Point { ... }

type Circle { radius: i32; }
impl Show for Circle { ... }

val items: [ref dyn Show] = [
    new Point { x: 1, y: 2 },
    new Circle { radius: 5 }
];

loop item in items {
    print(item.show());  // dynamic dispatch
}
```

### Object Safety

A trait is **object-safe** if all its methods:
- Take `self` by `ref<Self>`
- Do not reference `Self` in non-receiver positions (return types, generic parameters)

## Copy, Clone, and Drop

These are built-in marker traits for defining value semantics:

### Copy

Types that are trivially copyable can derive `Copy`:

```ng
type Point: derive(Copy) {
    x: i32;
    y: i32;
}

// Point can now be copied implicitly
val a = Point { x: 1, y: 2 };
val b = a;        // implicit copy (not move)
```

### Clone

Requires an explicit `.clone()` method:

```ng
type Point: derive(Copy + Clone) {
    x: i32;
    y: i32;
}

val a = Point { x: 1, y: 2 };
val b = a.clone();  // explicit clone
```

### Drop

When a value goes out of scope, its `Drop` implementation runs:

```ng
trait Drop {
    fun drop(self: ref<Self>) -> unit;
}

type FileHandle = i32;

impl Drop for FileHandle {
    fun drop(self: ref<Self>) {
        nativeClose(self);  // cleanup
    }
}
```

> **Note:** `Copy` and `Drop` are mutually exclusive — if you implement `Drop`, you cannot derive `Copy`.

## Derive

The `derive` attribute auto-implements standard traits:

```ng
type Point: derive(Copy + Clone) {
    x: i32;
    y: i32;
}
```

Supported derivable traits:
- `Copy` — implicit copy semantics
- `Clone` — explicit `.clone()` method

Derive generates correct implementations only for structural types (not tagged unions with certain payload types).

## Auto Traits

Auto traits are **automatically implemented** for all types that satisfy their structural requirements:

```ng
auto trait Send;

// All types automatically satisfy Send, unless explicitly opted out
type NotSend { ... }

// impl !Send for NotSend;  // exclude (future feature)
```

Auto traits cannot have methods — they are purely marker traits.

## Explicit Impl Selection: `use impl`

When multiple implementations exist for the same trait on the same type, use `use impl` to select:

```ng
use impl Show for Point;  // explicitly selects which impl to use
```

This is especially useful for module-qualified impls:

```ng
use impl my_module::Show for Point;
```

## Module-Level Impls

Traits can be implemented for foreign types in your module. The impl is visible when your module is imported:

```ng
// my_ext.ng
export impl Show for i32 {
    fun show(self: ref<Self>) -> string => "custom int: " + *self;
}

// main.ng
import my_ext (*);
print(42.show());  // uses the custom Show impl
```

## What's Next?

Continue to [Type System in Depth](type-system-in-depth.md) for a deeper exploration of NG's type system.

> **Try it:** `example/25.trait_show.ng` — Basic trait
> **Try it:** `example/26.trait_generic_bound.ng` — Generic trait bounds
> **Try it:** `example/27.trait_receiver_ref.ng` — Trait with ref receiver
> **Try it:** `example/28.trait_supertraits.ng` — Supertraits
> **Try it:** `example/29.trait_qualified_call.ng` — Qualified trait calls
> **Try it:** `example/30.trait_inherent_precedence.ng` — Inherent vs trait method
> **Try it:** `example/31-33.trait_default*.ng` — Default methods, override, supertraits
> **Try it:** `example/34-36.trait_object*.ng` — Trait objects
> **Try it:** `example/37.copy_marker.ng` — Copy marker trait
> **Try it:** `example/38.clone_trait.ng` — Clone trait
> **Try it:** `example/39.drop_raii.ng` — Drop / RAII
> **Try it:** `example/55.auto_derive_traits.ng` — Auto traits and derive