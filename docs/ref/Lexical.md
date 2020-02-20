Lexical
========


Lexical analysis is independent of the syntax parsing and the semantic analysis. The lexical analyzer splits the source text up into tokens. The lexical grammar describes the syntax of those tokens.

## 1. Source Text

Source text currently only can be in ASCII encoding texts.

## 2. Tokens

```
[Tokens]:
  [Identifier]
  [StringLiteral]
  [CharacterLiteral]
  [NumberLiteral]
  [Keyword]
  [UserDefinedOperator]
  +
  -
  *
  /
  %
  >
  >=
  >>
  <
  <=
  <<
  =
  ==
  :
  ::
  .
  ;
  ,
  ->
  =>
  {
  }
  (
  )
  [
  ]
```

### 2.1 Keywords

```
Keyword:
  type
  fun
  val
  sig
  struct
  cons

  module
  export
  use

  if
  then
  else

  loop
  collect

  case
  return
  break
  continue

  true
  false
  unit
```

for a whole list of reserved symbols please see `src/reserved.inc`.
