# Traits

Traits define shared behavior; impls provide it per type. NG supports
default methods, supertraits, generic impls, auto traits, `derive`, trait
bounds, and dynamic `ref<Trait>` views.

## Defining a trait

Methods take the receiver as `self: Self ref` (or `Self ref mut`):

```ng
trait Show {
    fun show(self: Self ref) -> string;
}
```

Default methods may call the trait's own methods through `self`:

```ng
trait Show {
    fun show(self: Self ref) -> string;

    fun bracketed(self: Self ref) -> string {
        return "[" + self.show() + "]";
    }
}
```

## Impls

```ng
impl Show for Counter {
    fun show(self: Self ref) -> string {
        return (*self).label;
    }
}
```

An empty impl uses the trait's defaults:

```ng
impl Display for Number { }
```

Impls are coherent: duplicates are rejected, and unknown or missing
methods (without defaults) are errors.

### Generic impls

`impl<T> Trait for Type<T>` matches concrete receivers per call:

```ng
impl<T> Show for List<T> {
    fun show(self: Self ref) -> string { return "list"; }
}

impl<T> Show for array<T> {
    fun show(self: Self ref) -> string { return "array"; }
}
```

Concrete impls take precedence over generic ones; each call site
instantiates the impl's methods per concrete receiver.

## Supertraits

```ng
trait Ord: Eq {
    fun less(self: Self ref, other: Self ref) -> bool;
}
```

Implementing `Ord` provides the `Eq` methods too.

## Qualified calls

```ng
let text = Show.show(42);       // qualified
let text = 42.show();           // method syntax
```

## Trait bounds

```ng
fun render<U: Show>(value: U ref) -> string {
    return value.show();
}
```

Bounds appear on generic parameters or in where clauses; impl evidence is
checked per instance (abstract calls defer to monomorphization).

## Auto traits and derive

```ng
auto trait Send {}
```

Auto traits are implemented for every concrete type implicitly.

`derive(Copy + Clone)` on a struct synthesizes the impls, including a
deep-copying `clone()` method:

```ng
struct Point: derive(Copy + Clone) {
    x: i64,
    y: i64,
}
```

## Dynamic views: `ref<Trait>`

`ref<Trait>` is a shared-borrowed view over any implementing value,
dispatched through per-concrete tables:

```ng
fun render(item: ref<Show>) -> string {
    return item.show();
}

let counter = Counter { label: "seven" };
let view: ref<Show> = counter;
let text = view.show();          // dynamic dispatch
```

Views coerce from bindings, call arguments, and array elements; default
methods dispatch through views too. Bare trait values (`let x: Show`)
are rejected — `ref<Show>` is the only dynamic form.

Next: [Type System in Depth](/guide/type-system-in-depth).
