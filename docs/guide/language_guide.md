# A Guide to the NG Programming Language

Welcome to the NG programming language! This guide will provide a comprehensive introduction to the language, from the basics to more advanced topics. Whether you are a beginner or an experienced programmer, this guide will help you get started with NG.

## 1. Introduction

NG is a modern, statically-typed programming language that is designed to be simple, efficient, and easy to learn. It is a general-purpose language that can be used for a wide range of applications, from scripting to systems programming.

### Philosophy

The design of NG is guided by the following principles:

*   **Simplicity:** The language should have a small, consistent set of features that are easy to understand and use.
*   **Readability:** The syntax should be clean and easy to read, making it easier to maintain and debug code.
*   **Performance:** The language should be able to generate efficient code that runs close to the metal.
*   **Safety:** The language should prevent common programming errors, such as null pointer dereferences and buffer overflows.

## 2. Getting Started

### Prerequisites

Before you can start using NG, you will need to have the following tools installed on your system:

*   **C++ Compiler:** A C++23 compatible compiler (e.g., GCC, Clang, MSVC).
*   **CMake:** Version 3.25.1 or higher.
*   **Build Tool:** Make or Ninja.

### Building the Project

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/ng-lang/ng.git
    cd ng
    ```

2.  **Create a build directory:**

    ```bash
    mkdir build
    cd build
    ```

3.  **Configure the project with CMake:**

    ```bash
    cmake -GNinja ..
    ```

4.  **Build the project:**

    ```bash
    ninja
    ```

### Your First Program

Let's start with a simple "Hello, World!" program.

```ng
// This is a comment.
print("Hello, World!");
```

To run this program, save it to a file called `hello.ng` and run the following command from the `build` directory:

```bash
./ngi ../hello.ng
```

This will print "Hello, World!" to the console. The `print` function is a built-in function that prints its argument to the standard output.

## 3. Language Basics

### Comments

NG supports single-line comments using `//`.

```ng
// This is a single-line comment.
```

### Variables and Mutability

Variables are declared using the `val` keyword. By default, variables are mutable, which means their values can be changed after they are declared.

```ng
val x = 1; // x is 1
x = 2;     // now x is 2
```

### Data Types

NG is a statically-typed language, which means that every variable has a type that is known at compile time. NG has a rich set of built-in data types.

#### Primitive Types

*   **Integers**: `i8`, `i16`, `i32`, `i64` (signed) and `u8`, `u16`, `u32`, `u64` (unsigned).
*   **Floating-point numbers**: `f32`, `f64`.
*   **Booleans**: `true`, `false`.
*   **Strings**: `"hello"`, `'world'`.
*   **Unit**: `unit`, which represents the absence of a value.

#### Type Annotations

You can explicitly specify the type of a variable using a type annotation.

```ng
val x: i32 = 1;
val name: string = "NG";
```

### Operators

NG supports a variety of operators.

*   **Arithmetic**: `+`, `-`, `*`, `/`, `%`
*   **Comparison**: `==`, `!=`, `>`, `<`, `>=`, `<=`
*   **Array Append**: `<<`
*   **Type Check**: `is`

There are no logical operators like `&&` or `||`. Instead, you can use the `not` function from the standard prelude.

### Expressions and Statements

NG is an expression-oriented language, which means that most things are expressions that evaluate to a value. A statement is a piece of code that performs an action but does not produce a value.

## 4. Control Flow

### Conditional Execution

`if/else` statements are used for conditional execution.

```ng
if (x > 0) {
    print("positive");
} else if (x < 0) {
    print("negative");
} else {
    print("zero");
}
```

### Loops

NG has a `loop` construct for iteration.

```ng
fun sum(n: i32) -> i32 {
  val s = 0;
  loop i = 0 {
    s = s + i;
    if (i < n) {
      next i + 1;
    }
  }
  return s;
}
```

The `next` keyword is used to continue the loop, optionally with a new value for the loop variable.

## 5. Functions

Functions are defined with the `fun` keyword.

```ng
fun add(x: i32, y: i32) -> i32 {
    return x + y;
}
```

### Shorthand Syntax

For single-expression functions, you can use the `=>` shorthand.

```ng
fun add(x: i32, y: i32) -> i32 => x + y;
```

### Recursion

Functions can call themselves, which is called recursion.

```ng
fun factorial(n: i32) -> i32 {
    if (n == 0) {
        return 1;
    }
    return n * factorial(n - 1);
}
```

### Native Functions

Functions can be marked as `native`, which means they are implemented in the host language.

```ng
fun my_native_function(arg: i32) -> unit = native;
```

## 6. Data Structures

### Arrays

Arrays are collections of elements of the same type.

```ng
val arr = [1, 2, 3, 4, 5];
print(arr[0]); // 1

arr[0] = 10;
print(arr[0]); // 10

arr << 6; // append 6 to the end
```

### Objects and Types

You can define your own custom types using the `type` keyword.

```ng
type Person {
    property firstName: string;
    property lastName: string;

    fun name() -> string {
        return self.firstName + " " + self.lastName;
    }
}

val person = new Person {
    firstName: "John",
    lastName: "Doe"
};

print(person.name()); // "John Doe"
```

## 7. Modules and Code Organization

### Modules

Each file in NG is a module. You can control what is visible outside the module using the `export` keyword.

```ng
// my_module.ng
module my_module exports *;

export fun my_fun() { ... }
```

### Importing Modules

You can import other modules using the `import` statement.

```ng
// main.ng
import my_module (*);

my_fun();
```

## 8. Standard Library

NG has a small standard library that provides basic functionalities.

### Prelude

The `std.prelude` module is implicitly imported into every module. It provides the following functions:

*   `print(value)`: Prints a value to the console.
*   `assert(condition)`: Asserts that a condition is true.
*   `not(value: bool)`: Returns the logical negation of a boolean value.

## 9. Computer Science Concepts

### Static Typing

NG is a statically-typed language. This means that the type of every variable is known at compile time, which helps to catch errors early and improve performance.

### Memory Management

NG uses automatic memory management, which means you don't have to manually allocate and deallocate memory. The compiler takes care of this for you.

## 10. Contributing

We welcome contributions from the community! If you are interested in contributing to the NG programming language, please read our [Contribution Guide](https://github.com/ng-lang/ng/blob/main/CONTRIBUTING.md) to get started.

## 11. Community

Join the NG community to ask questions, share your ideas, and collaborate with other developers.

-   **Discussions:** For general discussions, questions, and ideas, please use the [GitHub Discussions](https://github.com/ng-lang/ng/discussions).
-   **Issue Tracker:** For bug reports and feature requests, please use the [GitHub Issues](https://github.com/ng-lang/ng/issues).
-   **Pull Requests:** For contributions, please use [GitHub Pull Requests](https://github.com/ng-lang/ng/pulls).
