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

`main` is the entry point; it may take typed arguments and return `unit`,
`i64`, `f64`, or `string`.

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
let typed = first<i64>([1, 2, 3]);
```

Each distinct concrete argument set gets its own monomorphized instance,
re-checked with concrete types.

## Methods

Method-call syntax works on receivers with matching trait impls (see
[Traits](/guide/traits)):

```ng
trait Show {
    fun show(self: Self ref) -> string;
}

impl Show for i64 {
    fun show(self: Self ref) -> string { return "int"; }
}

let text = 42.show();            // "int"
let qualified = Show.show(42);   // same
```

## `native fun`

`native fun` declares a host function supplied by the embedding (the
standard library's string/io/seq/memory/imgui functions are natives):

```ng
export native fun length(text: string) -> i64;
```

The native backend resolves `$ngrt_<name>` symbols; `std.system` even
compiles and runs a source string from inside a running program.

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

Const funs support loops, recursion, tail recursion, const-capable native
hosts, and — with type parameters — per-type compile-time evaluation (see
[Compile-Time Programming](/guide/compile-time-programming)).

## Recursion and tail calls

Recursion is unrestricted; tail recursion is recognized by the VM and runs
in constant stack space.

Next: [Data Structures](/guide/data-structures).
