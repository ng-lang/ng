# Data Structures

Structs, enums, tuples, arrays, ranges, and unions.

## Structs

```ng
struct Counter {
    value: i64,
}
```

Construction uses named fields. Nominal structs are affine by default, so
reading a field and then copying the whole value in the same scope partially
moves it:

```ng
let counter = Counter { value: 41 };
let read = counter.value;
let mut copy = counter;      // error
copy.value := 42;
```

```text
type error E-move-partial at bytes [179, 186): use of partially moved value `counter`
```

Deriving `Copy` makes the struct duplicable, so read-and-copy both work:

```ng
import prelude;

struct Counter: derive(Copy) {
    value: i64,
}

fun main() -> unit {
    let counter = Counter { value: 41 };
    let read = counter.value;
    let mut copy = counter;
    copy.value := 42;
    print(read);
    print(copy.value);
    print(counter.value);
}
```

```text
41
42
41
```

Structs are nominal types: two structs with the same fields are distinct
types. Generic structs instantiate per concrete argument; annotate the
binding so the argument can be inferred:

```ng
struct Box2<T> {
    value: T,
}
let boxed: Box2<string> = Box2 { value: "hello" };   // Box2<string>
```

## Enums (tagged unions)

```ng
enum Shape {
    Circle(radius: f64),
    Rectangle(width: f64, height: f64),
    Point,
}
```

Constructors are qualified (`Shape.Circle(5.0)`); multi-field variants
carry tuple payloads. `switch` destructures variants positionally (see
[Control Flow](/guide/control-flow)).

Recursive structures use heap or reference payloads for the tail — the
sanctioned way to build linked data without GC:

```ng
import memory;

enum MyList<T> {
    Cons(head: T, tail: Box<MyList<T>>),
    Nil,
}
```

Box tails come from `std.memory`: `new<T>(value)` builds a `Box<T>`,
`read<T>(ref box)` loads its payload, `write<T>(ref box, value)` stores into
it, and each `Box<T>` releases its cell exactly once on drop.

```ng
import prelude;
import memory;

fun main() -> unit {
    let b = new<i64>(5);
    print(read<i64>(ref b));
    write<i64>(ref b, 9);
    print(read<i64>(ref b));
}
```

```text
5
9
```

The standard library ships its own recursive `List<T>` with `Box<List<T>>`
tails plus traversal helpers and immutable builders (`listFrom`,
`pushFront`, `append`, `reverseList`):

```ng
import prelude;
import list;

fun main() -> unit {
    let empty: List<i64> = List.Nil;
    let base = listFrom([2, 3]);
    let xs = pushFront(ref base, 1);
    print(length(ref xs));
    print(get(ref xs, 0));
    let app = append(ref base, 9);
    print(length(ref app));
    let rev = reverseList(ref base);
    print(get(ref rev, 0));
}
```

```text
3
1
3
3
```

Array literals build lists directly when the expected type is the list
enum:

```ng
let xs: List<i64> = [1, 2, 3];
let empty: List<i64> = [];
```

An empty array literal `[]` has no element type of its own, so it takes one
from the expected type — a binding annotation or a parameter type:

```ng
import seq;

fun show(values: array<i64>) -> i64 { return len(values); }

// show([]) is fine: the parameter supplies array<i64>.
```

Without an expected type it is rejected:
`cannot infer the type of an empty array literal`.

Lists spread back into array literals:

```ng
let flat = [...xs];          // [1, 2, 3]
let mixed = [0, ...xs, 9];
```

## Tuples

```ng
let pair = (1, "two", true);
let head = pair[0];
```

Destructuring and rest patterns:

```ng
let (first, ...rest) = pair;   // rest is a tuple of the remaining elements
```

For `(1, "two", true)`: `first` is `1`, `rest[0]` is `two`, `rest[1]` is
`true`.

## Arrays

Dynamic arrays and fixed-size arrays:

```ng
let dynamic = [1, 2, 3];            // array<i64>
let fixed: array<i64, 3> = [1, 2, 3];
```

Indexing is bounds-checked; slicing uses half-open ranges:

```ng
let middle = dynamic[0..2];   // [1, 2]
let window = dynamic[1..3];   // [2, 3]
```

Spreads splice values into literals:

```ng
let nums = [...(1..5)];        // [1, 2, 3, 4] (half-open range)
let mixed = [0, ...nums, 9];
let doubled = [doubleIt(1..4)...];    // map comprehension: per-element calls
let evens = [even(1..8)?...];         // filter marker keeps matching elements
```

For `[...(1..5)]` that yields 4 elements starting at `1`; `[doubleIt(1..4)...]`
yields `[2, 4, 6]`; `[even(1..8)?...]` yields `[2, 4, 6]`.

Array literals accept at most one spread:

```ng
let both = [...a, ...b];   // error
```

```text
type error E-type at bytes [104, 116): array literals support at most one spread
```

Append with value semantics:

```ng
let longer = dynamic << 9;     // dynamic unchanged, longer == [1,2,3,9]
```

## Folds

`seq.fold` takes the values, the seed, and a two-argument closure:

```ng
import prelude;
import seq;

fun main() -> unit {
    print(fold([1, 2, 3], 0, |acc, x| acc + x));
}
```

```text
6
```

A two-argument function can also fold directly, with the spread in either
argument position — the accumulator seed is whichever argument is not the
spread:

```ng
fun foldAdd(acc: i64, value: i64) -> i64 {
    return acc + value;
}

fun main() -> unit {
    let xs = [1, 2, 3];
    let folded = foldAdd(0, xs...);   // left fold: 6
    let right = foldAdd(xs..., 0);    // seed last: 6
}
```

Arrays, slices, and ranges all fold this way.

## Ranges

`a..b` is a half-open range value; iterate it with map comprehensions,
slice arrays with it, and spread it into literals.

## Unions (`A | B`)

Union annotations accept values of any member type; the value keeps its
member type at runtime (no tag):

```ng
let value: i64 | string = "text";
if (value == "text") { print("matched"); }
```

Equality/ordering narrow the member for comparisons.

Next: [Modules and Imports](/guide/modules-and-imports).
