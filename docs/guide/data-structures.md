# Data Structures

Structs, enums, tuples, arrays, ranges, and unions.

## Structs

```ng
struct Counter {
    value: i64,
}
```

Construction uses named fields; mutation goes through `ref mut` or `let mut`
field assignment:

```ng
let counter = Counter { value: 41 };
let read = counter.value;
let mut copy = counter;      // deep copy (value semantics)
copy.value := 42;
```

Structs are nominal types: two structs with the same fields are distinct
types. Generic structs instantiate per concrete argument:

```ng
struct Box<T> {
    value: T,
}
let boxed = Box { value: "hello" };   // Box<string>
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

Recursive structures use `ref<...>` payloads — the sanctioned way to build
linked data without GC:

```ng
enum List<T> {
    Cons(head: T, tail: ref<List<T>>),
    Nil,
}

let empty: List<i64> = List.Nil;
let one: List<i64> = List.Cons(1, ref empty);
```

The standard library ships this exact `List<T>` with traversal helpers and
immutable builders (`listFrom`, `pushFront`, `append`, `reverseList`) — see
[Standard Library](/guide/standard-library).

Array literals build lists directly when the expected type is the list
enum:

```ng
let xs: List<i64> = [1, 2, 3];
let empty: List<i64> = [];
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

Spreads splice values into literals (`at most one spread` per literal):

```ng
let nums = [...(1..5)];        // [1, 2, 3, 4] (half-open range)
let mixed = [0, ...nums, 9];
let doubled = [f(1..4)...];    // map comprehension: per-element calls
let evens = [g(1..8)?...];     // filter marker keeps matching elements
```

Append with value semantics:

```ng
let longer = dynamic << 9;     // dynamic unchanged, longer == [1,2,3,9]
```

Folds run over arrays, slices, and ranges:

```ng
let total = sum([1, 2, 3]);              // stdlib helper
let folded = foldAdd([1, 2, 3]..., 0);   // left fold: f(acc, xs...)
```

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
