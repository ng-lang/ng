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
  +  (addition, string concatenation)
  -  (subtraction)
  *  (multiplication)
  /  (division)
  %  (modulus)
  >  (greater than)
  >= (greater than or equal)
  <  (less than)
  <= (less than or equal)
  == (equality)
  != (inequality)
  :  (property initialization)
  .  (method access)
  ;  (statement terminator)
  ,  (separator)
  => (return)
  { } (block delimiters)
  ( ) (grouping, function call)
```

### 2.1 Keywords

```
Keyword:
  type
  fun
  val
  property
  new
  
  module
  import
  export
  
  if
  else
  
  return
  
  true
  false
  self
```

for a whole list of reserved symbols please see `src/reserved.inc`.
