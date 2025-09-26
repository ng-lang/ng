# On Design of Tuples

Latest update see <https://github.com/ng-lang/ng/discussions/21>


Tuple is just series values with different type grouped together.

There's few convenient things for tuple:
- used as multiple returns
- used as multiple bindings
- used to dynamically apply to function values

Examples:

```ng
val x = (1, "a", false);


val y: (int, string, bool) = x;


val (a, b, c) = y;

fun consume(a: int, b: string, c: bool) -> unit = native;

consume(...x);   // same as `consume(a, b, c);` 
                 //      or `consume(1, "a", false);`


fun provide() -> (int, bool) = native;
```

## Ranges

To support `...` (spread overator), we need also add `..` operator support or consider as an invalid token. Here I am considering adding it as range operator. For example, `1..10` creates a range between 1 (inclusive) and 10 (exclusive) for all integers.

In Ruby,`..` considered as an end-inclusive range operator, where `...` considered as a end-exclusive one, `1...10` creates a range `[1, 2, .., 9]`. There's also a feature that using range as unary operator, `1..` creates an infinite range starting from `1`. And `..10` creates a range ends with `10`.

In C#, `..` only creates an end-exclusive range, but there's another operator `^` that creates "from-end" index (Similar to Python/Ruby's negative index). Like `some_array[^2..^0]` returns last two elements from the array. "from-end" index also counting from 0, but represents not last element, but the end of the collection (since the operator is exclusive). For example:

```csharp
private string[] words = [
                // index from start     index from end
    "first",    // 0                    ^10
    "second",   // 1                    ^9
    "third",    // 2                    ^8
    "fourth",   // 3                    ^7
    "fifth",    // 4                    ^6
    "sixth",    // 5                    ^5
    "seventh",  // 6                    ^4
    "eighth",   // 7                    ^3
    "ninth",    // 8                    ^2
    "tenth"     // 9                    ^1
];              // 10 (or words.Length) ^0
```

In C# usually a Range is only used to access collections (using index operator `[]`). Not for other things like Python `range` function or Ruby's Range.

In Ruby, a range can be either a generative collection. And it's even more flexible than Python's `range`. Any type that implements `#succ` method and `<=>` operator (`Comparable`), can use range operator. (Python `range` only supports integer).

But there's another shortage for Ruby's Range comparing to Python. In python you can create an descending order range by specifying the third argument (`step`) in `range` as negative value, e.g. `range(10, 0, -1)` generates a range that counting down from `10` to `0` (exclusively). In Ruby you can create such object but not everything works as expected.

Compare following two code blocks:

(Python)
```python
r = range(10, 0, -1)

5 in r # => true

list(r) # => [10, 9, .., 1]
```

(Ruby)
```ruby
# These works for ascending ranges
(1..10).include? 5      # => true
(1..10).cover? (1..5)   # => true
(1..10).to_a            # => [1, 2, .., 10]

r = 10..1

# But these are not work
r.include? 5    # => false
r.cover? (5..1) # => false
r.to_a          # => []

# How ever if you use `#step` method:
r.step(-1) do |x|
  puts x. # => output from 10 to 1, it worked!
end

# Officially they suggest using `#reverse_each` method:
(1..10).reverse_each.to_a # => [10, 9, .., 1]
```

In Rust, Range operator are pretty similar to Ruby, but use different syntax where `..` for end-exclusive, and `..=` for end inclusive.

There's no such operator in C++ or JavaScript, but they support `...` unpacking. C++ also has a fold expression that utilize `...` syntax to simplify map / reduce actions on packs (which I think that can be expanded to ranges instead). C# also have a way to unpack range, but they use `..` syntax instead.

## Fold

Considering add fold expression in NG like C++ but apply to any Collections.

The spread operator have different meanings depend on which kind of way use it. When used as a prefix, it's unpacking, or introduce a new multiple binding. When used as postfix, it is an operator for mapping / filtering or folding. When it used standalone, it can be treated as variadic function declaration or used in pipeline syntax for mapping / filtering / folding operand.

### Case 1. Unpacking / Binding

```ng
val x = 1..2; # range
val y = [1, 2, 3];

val z = [...y]; # unpack a array then apply it to array creator

val a = (1, true, "hello");

val (num, fact, text) = a;

val [first, ...rest] = y; # rest = [y[1], ...,  y[^1]];
```

When binding and applying (unpacking then applying to function) mismatches tuple size and array size, and function arity, raise either index out of bound error, or type checking error. This compliances with function optional parameter's rule, for a function that with default value, e.g. `fun (int, bool, string = default) -> unit`, apply a tuple `(int, bool)` to it passes the type checker.


### Case 2. Mapping / Folding

```ng
fun plus1(x: int) -> int {
    return x + 1;
}

val x = [1, 2, 3];

// map expression
val y = [plus1(x)...];  // same as => val y = map(plus1, x);

// fold expression
fun sum(x: int, y: int) -> int {
    return x + y;
}

val evens = [even(x)?...]; // same as => val evens = filter(even, x);

val z = sum(x..., 0);   // same as => val z = foldr(sum, 0, x);
// or
// val z = sum(0, x...);
```

## Variadic functions

There's another feature that might involve `...` syntax. In C, there's variadic function (for example `printf`), which use `...` for their variable length function parameters.

Here we'll introduce a variadic syntax for native function declarations just now. More about variadic functions will covered in other drafts.

- `fun print(...) -> unit = native;` any length tuple of any types, `typeof(...)` => a tuple.

- `fun assert(...assertions: [bool]) -> native;` any length vector of bool, `typeof(assertions)` => a vector of bool

To allow tuple type level operations, we may also need following operators on types:

- `typeof(<expression>)` get type of an expression
- `<tuple>.size` get arguments length of a tuple
- `<tuple>[<n>]` get n-th element type of a tuple type
- `fun (<types>) -> <type>` a function type.
  - also `fun <tuple-type> -> type` 

## New added syntax and features

### Stage 1

- [ ] Tuple literal `(<values>)`
- [ ] Tuple type literal/annotation `(<types>)`
- [ ] Tuple operations, `<tuple>.size`, `<tuple>.<n>`, and `<tuple>[<n>]`, and `<tuple>.<n> := <value>`
  - [ ]  `<tuple>[a..b]` ranged index will be covered in stage 3
- [ ] Tuple/collection unpacking syntax: `...<tuple>` or `[...vector]`
- [ ] Multiple returns `return (<values>)`

### Stage 2

- [ ] Variadic native functions `<fnName>(...<argName>)`
- [ ] Multiple `val` bindings, `val (<a>, <b>: <type>) = <tuple>;`
- [ ] Apply function with unpacked tuple `<fnName>(...<tuple>)`
- [ ] Enhanced tuple type support:
  - [ ] `typeof(<expression>)` get type of an expression
  - [ ] `<tuple>.size` get arguments length of a tuple
  - [ ] `<tuple>.<n>` get n-th element type of a tuple type
  - [ ] `(<types>) -> <type>` a function type.
    - [ ] also `<tuple-type> -> type` 

### Stage 3

- [ ] Range `a..b`, `a..^b`, `^a..^b`, (end exclusive) and `..a`, `a..` syntax.
- [ ] Using ranges (and from-end index) to index collections/tuples `arr[a..b]`, `arr[^a]`
- [ ] Range as first-class collections, can be also applied to construct array: `[a..b]`

### Stage 4

- [ ] Fold expression on collections
  - [ ] Map `<fn>(<collection>)...`
  - [ ] Filter `<pred>(collection)?...`
  - [ ] Fold right `<fn>(<collection>..., <init>)`
  - [ ] Fold left `<fn>(<init>, <collection>...)`
  - [ ] Fold without init `<fn>(<collection>..., _)`, Here `_` used as a place holder that `fn` will take first element in `<collection>` as `<init>`.
  - [ ] Map result can be placed into fold, `<fn_a>(<fn_b>(<collection>)..., <init>)` or
  - [ ] Map result can construct a new collection `[<fn>(<collection>)...]`. or
  - [ ] Map result can be applied with new map. `[<fn_a>(<fn_b>(<collection>)...)...]`
- [ ] Pipeline syntax on collections
  - [ ] Example `<collection> |> <map_fn>(...) |> <filter_fn>(?...) |> fold_fn(..., <init>)`
