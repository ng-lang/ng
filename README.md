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
- CMake 4.0+
- C++23 compatible compiler (GCC, Clang, MSVC)
- Make or Ninja

### Building
```bash
mkdir build && cd build
cmake -GNinja ../
ninja
```

### Running Examples
```shell
./ngi ../example/<examplefile>.ng
```

## Documentation

- [Quick Start Guide](./docs/guide/quickstart.md)
- [Language Reference](./docs/ref/Contents.md)
- [Grammar Specification](./docs/ref/Grammar.md)
- [Internals](./docs/ref/Internals.md)

## Examples

Check the [examples directory](./example/) for sample NG code:

## Legacy Implementations

The project includes legacy implementations in:
- [Kotlin](./legacy/implementations/kotlin/ng/)
- [OCaml](./legacy/implementations/ocaml/ng/)

## Testing

Run tests with:
```bash
cd build && ctest
```

## Roadmap

- [x] Assignment Operator
- [x] `module` & `import`
- [x] Using Smart Pointers to Avoid Memory Leaks (Interpreter)
- [ ] Operator Overloading
- [ ] Compile to Native
- [ ] Naive Type Checking
- [ ] Bytecode Based Runtime - ORGASM (Organized Assembly)

## Contributing

See [CONTRIBUTION.md](./CONTRIBUTION.md) for guidelines on how to contribute to the project.
