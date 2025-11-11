# LCC: A Lightweight C Compiler

A simple compiler for a C-like language, built from scratch using C++17 and CMake. This project is an educational exploration into the principles of compiler design and implementation, targeting the MIPS architecture.

## Project Status

This project is currently under development. The progress of each major component is tracked below:

- **Frontend**
  - [x] Lexical Analysis (Lexer)
  - [x] Parsing (Parser)
  - [x] Abstract Syntax Tree (AST) Generation
  - [x] Semantic Analysis
  - [ ] Intermediate Representation (IR) Generation
- **Backend**
  - [ ] MIPS Assembly Generation

## Language Grammar

The compiler supports a subset of the C language, including basic types, variables, functions, and control flow statements. For a complete and formal definition of the language syntax, please see the [EBNF.md](EBNF.md) file.

## Getting Started

### Prerequisites

- [CMake](https://cmake.org/) (version 3.10 or higher)
- A C++ compiler that supports C++17 (e.g., GCC, Clang)
- [Make](https://www.gnu.org/software/make/) (or another build system like Ninja)

### Building the Project

1. **Clone the repository:**

    ```bash
    git clone <your-repository-url>
    cd lcc
    ```

2. **Create a build directory:**

    ```bash
    mkdir build
    cd build
    ```

3. **Configure the project with CMake:**

    ```bash
    cmake ..
    ```

4. **Compile the source code:**

    ```bash
    make
    ```

## Usage

After a successful build, the executable `Compiler` will be located in the `build` directory.

To compile a source file, run:

```bash
./Compiler
```

## License

This project is licensed under the terms of the [LICENSE](LICENSE) file.
