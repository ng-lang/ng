# Fed Programming Language

 - Derived from Ng
 - Explicit type, although sometimes it can be inferred
 - No macros, they are all **bullshit** (instead you can invent one)
 - Templates made simple
 - ADT && HKT support
 - Unified function call syntax
 - Value semantics
 - Strong type safety
 - No GC and lifetime annotation

# Examples

```haskell
-- cons means: Constructor
type List<'t> = cons Cons('t, List<'t>) | cons Nil

-- this is it
type array<'t, arity: Int>

get :: array<'t, n> -> Int -> Maybe 't

-- You can also emit the `array<Int, 5>` shit by some compiler magic
val shit: array<Int, 5> {1, 2, 3, 4, 5}

-- It's 4, seriously
shit.get(3) |> print

val list: Cons(1, Cons(2, Cons(3, Cons(4, Cons(5)))))

length :: List<'t> -> Int
length l = case l
   | Cons(x, xs) => length(xs) + 1
   | Nil => 0

type Fact<x: Int> = {
    value: x * Fact<x-1>.value
}

-- Fuck me, please
type Fact<0> = {value: 1}

fact :: Int -> Int
fact n = Fact<n>::value

-- array_of_int<5> == array<Int, 5>
type array_of_int = array<Int>

-- array_of_char<5> != array<Char, 5>
type! array_of_char = array<Char>

type ref<'t> = ref 't

(~>) :: ref<'t> -> 't

val shit2 = ref shit

-- :) shit2~>get(3) for short
shit2.~>.get(3)

```
