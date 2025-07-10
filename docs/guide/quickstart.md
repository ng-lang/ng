# NG Quickstart Guide

## Getting Started

First, build and run the NG interpreter:
```bash
mkdir build && cd build
cmake .. && make
./ngi
```

## Basic Syntax

### Hello World
```ng
print("Hello world!");
```

### Variables and Types
```ng
// Primitive types
val anInt = 1;
val aString = "string";
val aBool = true;

// Collections
val anArray = [1, 2, 3];

// Type declarations
type Person {
    property name;
    property age;
}
```

## Control Flow

### Conditionals
```ng
if (x > 0) {
    print("Positive");
} else if (x < 0) {
    print("Negative");
} else {
    print("Zero");
}
```


## Functions

### Basic Functions
```ng
fun square(x) {
    return x * x;
}
```

### Recursion
```ng
fun factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

## Modules

### Creating a Module
File: `math.ng`
```ng
module math exports *;

fun add(a, b) {
    return a + b;
}

fun sub(a, b) {
    return a - b;
}
```

### Using a Module
```ng
import "math" m;

val sum = m.add(2, 3);
```
