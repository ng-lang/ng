# Grammar

This document describes the syntax of ng language.

## 1. Declarations

ng programs consist of declarations including:

```
[Module]:
   [ModuleDeclaration]
   [UseDeclaration]
   [ExportDeclaration]
   [FunctionDeclaration]
   [ValueDeclaration]
```

### 1.1 Module and use declaration

Module declaration is used to specify a module name for current compilation unit.
Module name is separated by DOT (`.`), module name can also be a string literal if
it is a keyword.
```
[ModuleDeclaration]:
   module [ModuleName];
[ModuleName]:
   [StringLiteral]
   [Identifier]
   [Identifier].[ModuleName]
   [StringLiteral].[ModuleName]
```

Use declaration is to import symbols from module
```
[UseDeclaration]:
    use [ModuleName];
    use [ModuleName].*;
    use [ModuleName].[Identifier];
```

### 1.2 Export declaration

Export declaration is to specify which symbols are exposed to other modules.
```
[ExportDeclaration]:
    export [Identifier] ";"
    export "{"[Identifier] ("," [Identifier])* "}"
```

### 1.3 Function declaration

Function declaration is to declare or define a function inside a module.
```
[FunctionDeclaration]:
    fun [FunctionSignature]
    fun [FunctionSignature] [FunctionBody]

[FunctionSignature]:
    [FunctionName] "(" ([FunctionParameter] ("," [FunctionParameter])* )? ")"

[FunctionName]:
    [Identifier]

[FunctionParameter]:
    [Identifier] ":" [Type]

[FunctionBody]:
    [Statement]
```

### 1.4 Value declaration

Value declaration is to define variables inside a module or a function
```
[ValueDeclaration]:
    val [Identifier] (":" [Type])? "=" [Expression]
```

## 2. Statements

Statement is basic execution unit of ng, and it is assured that statement must execute in sequential order.

Value declaration is a special case because it is also treat like a statement inside function.

```
[Statement]:
    [SimpleStatement]
    [CompoundStatement]
    [IfStatement]
    [ReturnStatement]
    [ValueDeclaration]
```

### 2.1 Simple statement and Compound statement

Simple statement is literally an expression with a semicolon(`;`) at the end.

Compound statement is a couple of statements grouped in a curly-brace pair(`{}`)

```
[SimpleStatement]:
    [Expression] ";"

[CompoundStatement]:
    "{"  [Statement]*  "}"
```

### 2.2 Conditional statements

`if` and `case` are basic conditional statements using conditional expression or pattern
matching.
```
[IfStatement]:
    if "(" [Expression] ")" [Statement]
    if "(" [Expression] ")" [Statement] else [Statement]
```

### 2.3 Jump statements

ng has few jump statement to implement advanced control flow:
  - `return`
  - `continue`
  - `break`
  - `yield` (in design)

```
[ReturnStatement]:
    return [Expression] ";"
    "=>" [Expression] ";"
```

## 3. Expression

Expression is used to describe the calculations and function calls in ng to perform actual program logic.

```
[Expression]:
    [PrimaryExpression]
    [FunCallExpression] 
    [IdAccessorExpression]
    [BinaryExpression]
    [NewObjectExpression]

[NewObjectExpression]:
    new [TypeName] "{" ([PropertyAssignment] ",")* [PropertyAssignment] "}"

[PropertyAssignment]:
    [Identifier] ":" [Expression]

[TypeDeclaration]:
    type [TypeName] "{" 
        ([PropertyDeclaration] ";")*
        ([MethodDeclaration] ";")*
    "}"

[PropertyDeclaration]:
    property [Identifier]

[MethodDeclaration]:
    fun [MethodName] "(" ([FunctionParameter] ("," [FunctionParameter])* ")" [FunctionBody]

[PrimaryExpression]:
    [Identifier]
    [Literals]
[FunCallExpression]:
    [Expression]"(" [Expression]* ")"

[IdAccessorExpression]:
    [Expression].[Identifier]
    [Expression]."(" [Expression] ")"

[BinaryExpression]:
    [Expression] [Operator] [Expression]
