# NG Language Semantics

## Object System

### Type Definition
```ng
type TypeName {
    property propName;
    property propName2;

    fun methodName(param1, param2) {
        // method body
        return expression;
    }
}
```

- Types can define properties and methods
- Properties are dynamically typed
- Methods have access to `self` reference

### Object Creation
```ng 
val obj = new TypeName {
    propName: value,
    propName2: value2
};
```

- Objects are created with `new` keyword
- Property values can be initialized in constructor
- Nested object creation is supported

## String Operations

### Concatenation
```ng
"Hello" + " " + "World"  // "Hello World"
```

- Strings can be concatenated with `+` operator

### Methods
```ng
"text".size()  // returns length
"text".charAt(index)  // returns character at position
```

## Modules

### Import/Export
```ng
import { name1, name2 } from "module";
import * as alias from "module";

export name1, name2;
export *;
```

- Modules can export definitions
- Selective or wildcard imports supported
- Aliases can be created for modules

## Built-in Functions

```ng
print(expr1, expr2, ...)  // prints values to stdout
assert(condition)  // raises error if condition is false
```

## Expression Evaluation

- Binary operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`
- Method calls: `obj.method(args)`
- Property access: `obj.property`
- Array indexing: `array[index]`
