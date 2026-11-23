# Traits

Traits define shared behavior; impls provide it per type. NG supports
default methods, supertraits, generic impls, auto traits, `derive`, trait
bounds, and dynamic `ref<Trait>` views.

## Defining a trait

Methods take the receiver as `self: Self ref` (or `Self ref mut`):

```ng
trait Tag {
    fun tag(self: Self ref) -> string;
}
```

Default methods may call the trait's own methods through `self`:

```ng
trait Tag {
    fun tag(self: Self ref) -> string;

    fun bracketed(self: Self ref) -> string {
        return "[" + self.tag() + "]";
    }
}
```

## Impls

```ng
struct Counter {
    label: string,
}

impl Tag for Counter {
    fun tag(self: Self ref) -> string {
        return (*self).label;
    }
}
```

An empty impl uses the trait's defaults:

```ng
trait Display {
    fun label(self: Self ref) -> string { return "unknown"; }
}

struct Number { v: i64 }

impl Display for Number { }
```

Given `let n = Number { v: 1 }`, `n.label()` yields `"unknown"`.

Impls are coherent. A duplicate impl is rejected:

```
type error E-type: duplicate impl for trait `T1`
```

Unknown methods and missing methods that have no default are errors.

### Generic impls

`impl<T> Trait for Type<T>` matches concrete receivers per call:

```ng
impl<T> Tag for List<T> {
    fun tag(self: Self ref) -> string { return "list"; }
}

impl<T> Tag for array<T> {
    fun tag(self: Self ref) -> string { return "array"; }
}
```

With `let l = listFrom([1, 2, 3])` and `let values = [1, 2, 3]`, `values.tag()`
yields `"array"` and `l.tag()` yields `"list"`.

Concrete impls take precedence over generic ones; each call site
instantiates the impl's methods per concrete receiver
(`example/generic_impls.ng`).

## Supertraits

```ng
trait Eq {
    fun same(self: Self ref, other: Self ref) -> bool;
}

trait Ord: Eq {
    fun less(self: Self ref, other: Self ref) -> bool;
}
```

An impl of `Ord` must supply the supertrait's methods too, otherwise it is
rejected:

```
type error E-type: impl for trait `Ord` is missing method `same`
```

At a **concrete** receiver, implementing `Ord` makes the `Eq` methods callable:
`a.same(ref b)` works when `a` and `b` have a concrete type implementing `Ord`.

A trait **bound** does not carry that over. Inside a generic body, a `T: Ord`
bound does not grant calling the supertrait's method:

```ng
fun genericSame<T: Ord>(a: T ref, b: T ref) -> bool {
    return a.same(ref b);          // rejected
}
```

```
type error E-type: no trait bound provides method `same` for type parameter `T`
```

A bound gives only the methods declared by that trait itself; to call an `Eq`
method generically, bound `T: Eq` as well.

Note that the prelude exports its own traits `Copy`, `Clone`, `Eq`, and `Show`.
Importing the prelude and also declaring `trait Show` in the module is a
duplicate declaration:

```
resolution error E-resolve: duplicate module declaration `Show`
```

## Qualified calls

A qualified call names the trait and passes the receiver as an ordinary
argument; the receiver must be an aggregate value such as a struct binding or
a `ref` to one:

```ng
let c = Counter { label: "seven" };
let text = Tag.tag(ref c);   // qualified
let same = c.tag();          // method syntax
```

Both forms print `seven`. A bare trait method call with no receiver
(`Tag.tag(42)`) has no receiver to dispatch through and is not valid.

## Trait bounds

```ng
fun render<U: Tag>(value: U ref) -> string {
    return value.tag();
}
```

Bounds appear on generic parameters or in where clauses; impl evidence is
checked per instance.

## Auto traits and derive

```ng
auto trait Send {}
```

Auto traits are implemented for every concrete type implicitly.

`derive(Copy + Clone)` on a struct synthesizes the impls, including a
deep-copying `clone()` method; `derive(Eq)` synthesizes structural `==`:

```ng
struct Point: derive(Copy + Clone) {
    x: i64,
    y: i64,
}
```

```ng
let point = Point { x: 20, y: 22 };
let mut cloned = point.clone();
cloned.x := 30;
```

(`example/derive.ng`.)

## Dynamic views: `ref<Trait>`

`ref<Trait>` is a shared-borrowed view over any implementing value,
dispatched through per-concrete tables:

```ng
fun render(item: ref<Tag>) -> string {
    return item.tag();
}

let counter = Counter { label: "seven" };
let view: ref<Tag> = counter;
let text = view.tag();          // dynamic dispatch
print(render(counter));         // view built at the call site
```

Views coerce from bindings and call arguments: passing a binding where a
`ref<Tag>` parameter is expected builds the view at the call site, so
`render(counter)` dispatches to `Counter`'s impl.

An array of views gives heterogeneous dynamic dispatch:

```ng
let views: array<ref<Tag>> = [counter, number];
print(views[0].tag());
print(views[1].tag());
```

Bare trait values (`let x: Tag`) are rejected — `ref<Tag>` is the only
dynamic form:

```
type error E-type: trait `Tag` is not a value type; use `ref<Tag>`
```

Next: [Type System in Depth](/guide/type-system-in-depth).
