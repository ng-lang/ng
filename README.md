# The NG Programming Language

NG is a static-typed, multiple paradigm programming language designed for efficiency and productivity. It is a general purpose programming language focused on system programming with implementations in C++, Kotlin, and OCaml.

## Features

- Static typing with type inference
- Multiple programming paradigms (functional, imperative, OOP)
- Module system with imports
- Smart pointers for memory safety
- Cross-platform (macOS, Linux, Windows)

## Getting Started

### Prerequisites
- CMake 4.0+
- C++23 compatible compiler (GCC, Clang, MSVC)
- Make or Ninja

### Building
```bash
mkdir build && cd build
cmake ..
make
```

### Running Examples
```bash
./build/ngi example/01.id.ng
```

## Documentation

- [Quick Start Guide](./docs/guide/quickstart.md)
- [Language Reference](./docs/ref/Contents.md)
- [Grammar Specification](./docs/ref/Grammar.md)
- [Internals](./docs/ref/Internals.md)

## Examples

Check the [examples directory](./example/) for sample NG code:
- Basic syntax: `01.id.ng`, `02.many_defs.ng`
- Functions and calls: `03.funcall_and_idexpr.ng`
- Data structures: `06.array.ng`, `07.object.ng`
- Modules: `08.imports.ng`

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
