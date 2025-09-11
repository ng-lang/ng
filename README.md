# A NostalGic (NG) Programming Language

[![build](https://github.com/ng-lang/ng/actions/workflows/build.yml/badge.svg)](https://github.com/ng-lang/ng/actions/workflows/build.yml) 
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e72d75eb4dbf4a0e9617cbced2f4ec1e)](https://app.codacy.com/gh/ng-lang/ng/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) 
[![Codacy Badge](https://app.codacy.com/project/badge/Coverage/e72d75eb4dbf4a0e9617cbced2f4ec1e)](https://app.codacy.com/gh/ng-lang/ng/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_coverage) 
[![codecov](https://codecov.io/github/ng-lang/ng/graph/badge.svg?token=T5RV6EWVSG)](https://codecov.io/github/ng-lang/ng)

NG is a static-typed, multiple paradigm programming language designed for efficiency and productivity.

## Features

- Minimum mutability required
- Minimum runtime overhead
- Let others rewrite everything and bind them
- Direct hardware mapping

## Getting Started

### Prerequisites

-   **C++ Compiler:** A C++23 compatible compiler (e.g., GCC, Clang, MSVC).
-   **CMake:** Version 4.0 or higher.
-   **Build Tool:** Make or Ninja.

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

### Running the Interpreter

After building the project, you can use the `ngi` interpreter to run NG scripts.

```bash
./ngi ../example/01.id.ng
```

## Documentation

- [A Guide to the NG Programming Language](./docs/guide/language_guide.md)

## Roadmap

### Core Language Features
- [ ] Robust error handling (e.g., `try/catch` or a `Result` type)
- [ ] Pattern matching
- [ ] Closures/Lambdas
- [ ] Generics
- [ ] Enums
- [ ] Compile to Native
- [ ] Naive Type Checking
- [ ] Bytecode Based Runtime - ORGASM (Organized Assembly)

### Standard Library
- [ ] Comprehensive file I/O module
- [ ] Advanced string manipulation module (e.g., regex)
- [ ] Rich collections library (e.g., hashmaps, sets)
- [ ] Process management module
- [ ] Networking module (e.g., HTTP)
- [ ] Date and time module

### Tooling and Ecosystem
- [ ] Package manager
- [x] REPL (Read-Eval-Print Loop)
- [ ] Automatic code formatter
- [ ] Linter
- [ ] Debugger

## Community

We welcome contributions and feedback from the community! Here are a few ways to get involved:

-   **Discussions:** For general discussions, questions, and ideas, please use the [GitHub Discussions](https://github.com/ng-lang/ng/discussions).
-   **Issue Tracker:** For bug reports and feature requests, please use the [GitHub Issues](https://github.com/ng-lang/ng/issues).
-   **Pull Requests:** For contributions, please use [GitHub Pull Requests](https://github.com/ng-lang/ng/pulls).

## Contributing

We welcome contributions from everyone. Please read our [Contribution Guide](./CONTRIBUTING.md) to get started.
