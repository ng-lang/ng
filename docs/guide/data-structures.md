# Data Structures

NG provides several built-in data structures and the ability to define custom types.

## Arrays

Arrays are ordered collections of elements of the same type.

### Array Literals

```ng
val arr = [1, 2, 3, 4, 5];
```

### Index Access

```ng
val first = arr[0];   // 1
arr[0] = 10;          // mutate in place
```

### Appending

The `<<` operator appends an element to the end of a dynamic array:

```ng
arr << 6;             // arr is now [10, 2, 3, 4, 5, 6]
```

### Length

```ng
print(len(arr));      // 6 (using prelude function)
```

### Fixed Arrays

Fixed-size arrays use the `[T; N]` syntax:

```ng
val fixed: [i32; 3] = [1, 2, 3];
```

### Array Slicing

Array slices create `span<T>` views (no copy):

```ng
val arr = [0, 1, 2, 3, 4, 5];
val slice1 = arr[1..4];    // [1, 2, 3]
val slice2 = arr[..3];     // [0, 1, 2]
val slice3 = arr[3..];     // [3, 4, 5]
```

### Range → Array Materialization

Spread a range or span into a vector:

```ng
val vec = [...(0..5)];     // [0, 1, 2, 3, 4]
```

### Spread and Unpacking

The `...` spread operator unpacks an array or tuple into a function call or array literal:

```ng
fun sum(a: i32, b: i32, c: i32) -> i32 => a + b + c;

val args = [1, 2, 3];
val result = sum(...args);   // 6
```

## Tuples

Tuples group multiple values of possibly different types.

### Tuple Literals

```ng
val t = (1, "hello", true);
print(t.0);   // 1 (index access)
print(t.1);   // "hello"
```

### Named Tuples

Tuples can have named fields for better readability:

```ng
val person = (name: "Alice", age: 30);
print(person.name);  // "Alice"
```

### Tuple Destructuring

Pattern matching destructures tuples:

```ng
val (x, y) = (1, 2);       // x = 1, y = 2
val (head, ...tail) = (1, 2, 3, 4);  // head = 1, tail = (2, 3, 4)
```

### Enhanced Tuples

NG supports extended tuple operations including:
- `tuple_element<T, I>` — type-level element projection
- `tuple_concat<A, B>` — type-level concatenation
- Spread expansion over tuples

See [Advanced Generics](advanced-generics.md) for details.

## Objects and Types

Custom types are defined with the `type` keyword and allocated with `new`:

```ng
type Person {
    property firstName: string;
    property lastName: string;

    fun fullName(self: ref<Self>) -> string {
        return self.firstName + " " + self.lastName;
    }
}

val person = new Person {
    firstName: "John",
    lastName: "Doe"
};

print(person.fullName());  // "John Doe"
```

### Object Fields

Fields are accessed with `.` notation and can be mutated:

```ng
person.lastName = "Smith";
```

> **Note:** The `property` keyword is optional when declaring fields — you can use just the field name and type:
>
> ```ng
> type Point {
>     x: i32;
>     y: i32;
> }
> ```

### `new` Keyword and Heap Allocation

`new` allocates on the managed heap and returns a `ref<T>`, so heap objects alias by reference:

```ng
val a = new Point { x: 1, y: 2 };
val b = a;            // b references the same object
a.x = 10;
print(b.x);           // 10 (shared mutation)
```

## Tagged Unions

Tagged unions (also known as discriminated unions or sum types) represent a value that can be one of several variants:

```ng
type Result = Ok(value: i32) | Err(msg: string);

val success = Ok(42);
val failure = Err("something went wrong");
```

Each variant can carry its own payload. You use `switch` to pattern match on the active variant:

```ng
switch (result) {
    case Ok(value) {
        print("Success:", value);
    }
    case Err(msg) {
        print("Failure:", msg);
    }
}
```

### Recursive Tagged Unions

For recursive data structures like linked lists or trees, use `ref<T>` to break the cycle:

```ng
type List<T> = Nil | Cons(head: T, tail: ref<List<T>>);

fun len<T>(list: ref<List<T>>) -> i32 {
    switch (list) {
        case Nil {
            return 0;
        }
        case Cons(head, tail) {
            return 1 + len(tail);
        }
    }
}
```

### Tagged Union Members

You can inspect the active variant at runtime:

```ng
print(success.tag);    // "Ok"
print(success.index);  // 0
```

### `otherwise` Branch

```ng
switch (value) {
    case Ok(v) { print(v); }
    otherwise { print("not Ok"); }
}
```

## Ranges

Ranges are a first-class language construct:

```ng
val exclusive = 0..10;       // 0,1,2,3,4,5,6,7,8,9
val inclusive = 0..=10;     // 0,1,2,3,4,5,6,7,8,9,10
val descending = 10..0;     // 10,9,8,7,6,5,4,3,2,1
```

Use ranges for iteration:

```ng
loop i in 0..=5 {
    print(i);  // 0, 1, 2, 3, 4, 5
}
```

## Union Types (Untagged)

NG also supports untagged union types for values that can be one of several primitive types:

```ng
type Value = i32 | string | bool;

val v1: Value = 42;
val v2: Value = "hello";
val v3: Value = true;
```

Untagged unions use the `is` operator at runtime to check the current type.

## Type Aliases

Create a new name for an existing type:

```ng
type Age = i32;
val myAge: Age = 30;
```

Type aliases are **transparent** — `Age` and `i32` are interchangeable.

## Newtypes

For type safety with distinct types, use `newtype`:

```ng
newtype UserId = i32;
newtype ProductId = i32;

val uid: UserId = UserId(1);
val pid: ProductId = ProductId(2);

// uid = pid;  // ERROR: type mismatch!
```

Newtypes are **opaque** — `UserId` and `i32` are **not** interchangeable. Use explicit casts to convert:

```ng
val raw: i32 = uid as i32;      // unwrap
val uid2 = UserId(raw);         // wrap
```

## What's Next?

Continue to [Modules and Imports](modules-and-imports.md) to learn about code organization.

> **Try it:** `example/06.array.ng` — Arrays
> **Try it:** `example/07.object.ng` — Objects and types
> **Try it:** `example/14.tuple.ng` — Tuples
> **Try it:** `example/16.tagged_union.ng` — Tagged unions
> **Try it:** `example/19.union_type.ng` — Union type annotations
> **Try it:** `example/21.recursive_tagged_union_ref.ng` — Recursive tagged unions
> **Try it:** `example/54.enhanced_tuple_types.ng` — Enhanced tuple types
> **Try it:** `example/57.ranges_slicing_pipeline.ng` — Ranges, slices, and pipeline