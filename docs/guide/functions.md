# Functions

Defining, calling, and composing functions in NG.

## Defining functions

```ng
fun add(left: i64, right: i64) -> i64 {
    return left + right;
}
```

Expression bodies use `=>`:

```ng
fun double(value: i64) -> i64 => value * 2;
```

`main` is the entry point; it may return `unit`, `i64`, `f64`, or `string`.

## Calling

```ng
let sum = add(20, 22);
```

Arguments and returns use copy-first value semantics (see
[References, Moves & Ownership](/guide/references-moves)).

## Generic functions

```ng
fun first<T>(values: array<T>) -> T {
    return values[0];
}
```

Type arguments are inferred from arguments, or written explicitly:

```ng
let head = first([1, 2, 3]);
let typed = first<i64>([4, 5, 6]);
```

Each distinct concrete argument set gets its own monomorphized instance,
re-checked with concrete types.

## Methods

Method-call syntax works on receivers with matching trait impls (see
[Traits](/guide/traits)). The receiver must be an aggregate value — structs,
arrays, and other derived or user-defined receivers — not a bare scalar
literal:

```ng
import prelude;

trait RenderText {
    fun show(self: Self ref) -> string;
}

struct Point: derive(Copy) {
    x: i64,
    y: i64,
}

impl RenderText for Point {
    fun show(self: Self ref) -> string {
        return "point";
    }
}

fun main() -> unit {
    let p = Point { x: 1, y: 2 };
    let text = p.show();
    print(text);
    let qualified = RenderText.show(ref p);
    print(qualified);
}
```

```text
point
point
```

`derive(Show)` synthesizes a `show` method, and a trait can be called
qualified with the receiver passed explicitly:

```ng
import prelude;

struct Point: derive(Show) {
    x: i64,
    y: i64,
}

fun main() -> unit {
    let p = Point { x: 1, y: 2 };
    print(p.show());
    print(Show.show(ref p));
}
```

```text
Point{x: 1, y: 2}
Point{x: 1, y: 2}
```

Calling a method on an integer is a type error:

```ng
let n: i64 = 42;
print(n.show());   // error
```

```text
type error E-type at bytes [68, 76): no method `show` for value of type i64
```

## `native fun`

`native fun` declares a host function supplied by the embedding (the
standard library's string/io/seq/memory/imgui functions are natives):

```ng
export native fun length(text: string) -> i64;
```

Import one from the standard library and call it by name:

```ng
import prelude;
import string (length);

fun main() -> unit {
    print(length("abcd"));
}
```

```text
4
```

`std.system` shells out through `/bin/sh -c`: `system(command)` returns the
exit status and `systemOutput(command)` returns the captured output.

```ng
import prelude;
import system;

fun main() -> unit {
    print(system("exit 7"));
    print(systemOutput("echo hi"));
}
```

```text
7
hi
```

## `const fun`

`const fun` bodies are compile-time capable and runtime callable:

```ng
const fun fact(value: i64) -> i64 {
    if (value == 0) { return 1; }
    return value * fact(value - 1);
}

const if (fact(4) == 24) { print("folded"); }
let runtimeResult = fact(5);   // also runs normally
```

```text
folded
120
```

Const funs support loops, recursion, tail recursion, const-capable native
hosts, and — with type parameters — per-type compile-time evaluation (see
[Compile-Time Programming](/guide/compile-time-programming)).

## Recursion and tail calls

## Closures and captures

A closure literal is `|params| expression` or `|params| { statements }`, and an
explicit capture list is written before the parameter list:

```ng
import prelude;

fun main() -> i64 {
    let base = 10;
    let addBase = [base] |x: i64| x + base;
    return addBase(5);
}
```

```text
15
```

The capture value is threaded into **every** call, which fixes what each
capture kind means:

- **`[x]` (copy capture)** — for an affine type the type must implement
  `Clone`; the capture is deep-copied per call, so the original stays live and
  usable after calls. Copy-capturing an affine value without `Clone` is a
  compile error:

  ```text
  type error E-type: cannot copy-capture affine value of type `P` that does not
  implement Clone; use `[move ...]`, `[ref ...]`, or `derive(Clone)` on the type
  ```

- **`[move x]`** — the closure takes ownership; later uses of `x` in the
  enclosing scope are rejected as use-after-move.
- **`[handle]` where `handle` is a `ref`/`ref mut` binding** — the closure
  becomes an anonymous borrower of the referent; the loan lives until every
  bearer has been used for the last time.

A `Clone`-able affine type can therefore be copy-captured safely:

```ng
import prelude;

struct P { v: i64 }
impl Clone for P {
    fun clone(self: Self ref) -> Self { return P { v: (*self).v }; }
}

fun main() -> i64 {
    let p = P { v: 10 };
    let f = [p] |x: i64| x + p.v;
    return f(1) + f(2) + p.v;   // p is still owned here
}
```

```text
33
```

## The `?` try operator

Inside a function returning `Result<T, E>`, `expr?` unwraps `Ok` to its payload
and returns `Result.Err(...)` early otherwise.

```ng
import prelude;

fun requireEven(x: i64) -> Result<i64, string> {
    if (x % 2 == 0) { let r: Result<i64, string> = Result.Ok(x); return r; }
    let e: Result<i64, string> = Result.Err("odd");
    return e;
}

fun doubleEven(x: i64) -> Result<i64, string> {
    let v = requireEven(x)?;      // Ok -> v; Err -> returns from doubleEven
    let r: Result<i64, string> = Result.Ok(v * 2);
    return r;
}

fun viaReturn(x: i64) -> Result<i64, string> {
    return requireEven(x)?;       // `?` as the whole returned value
}

fun main() -> unit {
    switch (doubleEven(4)) { case Ok(v) { print(v == 8); } case Err(e) { print(false); } }
    switch (doubleEven(3)) { case Ok(v) { print(false); } case Err(e) { print(e == "odd"); } }
    switch (viaReturn(6))  { case Ok(v) { print(v == 6); } case Err(e) { print(false); } }
}
```

```text
true
true
true
```

`?` may appear in an expression (`let v = f()? + g()?;`). Each `?` unwraps in
evaluation order; `Err` returns from the enclosing function.

It is **not** allowed inside a closure body or on a destructuring `let`. The
enclosing function must return `Result`.

Recursion is unrestricted, including self-recursive functions with arguments
and an accumulator.

Next: [Data Structures](/guide/data-structures).
