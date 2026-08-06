# Photon

**A systems programming language, and the C++20 compiler that implements it.**

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C)
![Tests](https://img.shields.io/badge/tests-344%20passing-brightgreen)

---

## Overview

Photon is a systems programming language designed around compile-time memory
safety without a garbage collector, first-class async, and predictable,
zero-cost abstractions.

This repository contains the Photon compiler, written in C++20. **It is an
early-stage project.** The front end — memory management, diagnostics, source
management, lexical analysis and parsing — is implemented and tested. Semantic
analysis and code generation are not yet started, so the compiler can currently
read and understand Photon source but cannot yet produce an executable.

The sections below distinguish clearly between what the compiler does today and
what the language is designed to become.

## Project status

| Component                           | Status      | Tests |
|-------------------------------------|-------------|-------|
| Memory arena allocator              | Implemented | 30    |
| Diagnostics engine and formatter    | Implemented | 107   |
| Source manager                      | Implemented | 45    |
| Lexer                               | Implemented | 47    |
| Parser and AST                      | Implemented | 115   |
| Semantic analysis and type checking | Not started | —     |
| Code generation                     | Not started | —     |
| Runtime and standard library        | Not started | —     |

All 344 test cases pass. The full suite is clean under AddressSanitizer,
UndefinedBehaviorSanitizer and ThreadSanitizer, and the project builds with no
compiler warnings under `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
-Wshadow` (tests additionally build with `-Werror`).

## Getting started

### Prerequisites

| Requirement      | Version                       | Notes                                          |
|------------------|-------------------------------|------------------------------------------------|
| C++20 compiler   | Clang 14+, GCC 11+, MSVC 2022 | Verified with Apple Clang 21.0.0               |
| CMake            | 3.20 or newer                 | Verified with 4.3.1                            |
| Ninja            | Any recent release            | Recommended; Unix Makefiles also works         |
| GoogleTest       | 1.17.0                        | Optional; tests are skipped if not found       |
| Google Benchmark | 1.9.5                         | Optional; benchmarks are skipped if not found  |

On macOS the optional dependencies come from Homebrew:

```bash
brew install cmake ninja googletest google-benchmark
```

On Debian and Ubuntu:

```bash
sudo apt install cmake ninja-build libgtest-dev libbenchmark-dev
```

### Build

```bash
git clone https://github.com/photon-lang/photon.git
cd photon
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

CMake reports which optional dependencies it found during configuration. If
GoogleTest or Google Benchmark are missing, the corresponding targets are
silently disabled and the compiler itself still builds.

### Run the tests

```bash
ctest --test-dir build --output-on-failure
```

Individual suites can also be run directly, which gives per-case output:

```bash
./build/compiler/lexer/tests/lexer_test
./build/compiler/parser/tests/parser_tests
./build/compiler/diagnostics/tests/diagnostics_test
./build/compiler/memory/tests/memory_test
./build/compiler/source/tests/source_test
```

## Usage

### `photonc` — parse a source file and print its AST

```bash
./build/photonc examples/simple.ph
```

```text
🚀 Photon Compiler - Parser Demo
================================

=== File: examples/simple.ph ===
Source:
fn main() {
    let greeting = "Hello, Photon!"
    let number = 42
    let result = number + 10
}

✅ Tokenization successful
✅ Parsing successful

AST:
Program:
  Function: main()
    Block {
      VarDecl: greeting = "Hello, Photon!"
      VarDecl: number = 42
      VarDecl: result = (number + 10)
    }
```

Invoked with no arguments, `photonc` parses a set of built-in examples covering
literals, expressions, declarations and multi-function programs.

### `ast_demo` — construct and traverse an AST programmatically

```bash
./build/ast_demo
```

Demonstrates the AST node types, the visitor interface, arena allocation and
operator precedence handling without going through the parser.

## The language today

The compiler currently accepts the following subset. Everything here is
exercised by the test suite and by the programs in [`examples/`](examples).

```photon
fn add(x: i32, y: i32) -> i32 {
    let sum = x + y
}

fn main() {
    let x = 10
    let mut total = 0
    let result = add(x, 32)
    let scaled = (x + result) * 2
}
```

**Supported**

- Function declarations with typed parameters and an optional return type
- `let` and `let mut` bindings with an optional type annotation and initializer
- Nested blocks and expression statements
- Full expression grammar with correct precedence and associativity: arithmetic,
  comparison, logical, bitwise, shifts, ranges, assignment and compound
  assignment, unary operators, function calls and parenthesised grouping
- Integer literals in decimal, hexadecimal (`0xFF`), binary (`0b1010`) and octal
  (`0o755`) form, with `_` digit separators
- Floating-point literals including exponents (`1e5`, `532e-9`, `3.141_592`)
- String and character literals with escape sequences, and boolean literals
- Line and block comments
- Statements separated by newlines or semicolons
- Error recovery: the parser reports multiple diagnostics per file and
  resynchronises at declaration and block boundaries

**Recognised by the lexer but not yet parsed** — `struct`, `enum`, `trait`,
`impl`, `if`, `else`, `match`, `while`, `for`, `loop`, `return`, `break`,
`continue`, `async`, `await`, `use`, `mod`. These are reserved words today; the
grammar for them has not been implemented.

**Designed but not implemented** — the ownership and borrow model, async/await
execution, generics, the module system, quantum primitives and C++
interoperability.

## Repository layout

```
photon/
├── CMakeLists.txt              Root build configuration
├── main.cpp                    photonc driver
├── ast_demo.cpp                Standalone AST construction demo
├── compiler/
│   ├── common/                 Shared type aliases and Result<T, E>
│   ├── memory/                 Arena allocator (+ tests, benchmarks)
│   ├── diagnostics/            Diagnostic engine and formatter (+ tests, benchmarks)
│   ├── source/                 Source loading, UTF-8 validation, line mapping (+ tests)
│   ├── lexer/                  Tokenizer (+ tests)
│   └── parser/                 Recursive-descent and Pratt parser, AST (+ tests)
└── examples/                   Sample Photon programs
```

Each component is a self-contained CMake target with its own public headers
under `include/photon/<component>/` and its own test suite.

## Development

### Build options

| Option                     | Default | Purpose                           |
|----------------------------|---------|-----------------------------------|
| `PHOTON_ENABLE_TESTING`    | `ON`    | Build the GoogleTest suites       |
| `PHOTON_ENABLE_BENCHMARKS` | `ON`    | Build the Google Benchmark suites |
| `PHOTON_ENABLE_ASAN`       | `OFF`   | AddressSanitizer                  |
| `PHOTON_ENABLE_UBSAN`      | `OFF`   | UndefinedBehaviorSanitizer        |
| `PHOTON_ENABLE_TSAN`       | `OFF`   | ThreadSanitizer                   |
| `PHOTON_ENABLE_COVERAGE`   | `OFF`   | Coverage instrumentation          |
| `PHOTON_ENABLE_LTO`        | `OFF`   | Link-time optimization            |
| `PHOTON_ENABLE_FUZZING`    | `OFF`   | Fuzzing instrumentation           |

### Sanitizer builds

```bash
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DPHOTON_ENABLE_ASAN=ON -DPHOTON_ENABLE_UBSAN=ON -DPHOTON_ENABLE_BENCHMARKS=OFF
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

ThreadSanitizer is configured the same way with `-DPHOTON_ENABLE_TSAN=ON`. It
cannot be combined with AddressSanitizer.

### Benchmarks

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/benchmarks/memory_bench
./build-release/benchmarks/diagnostics_benchmark
```

The arena benchmarks compare bump allocation against `malloc` and
`std::make_unique` across allocation sizes, and measure reset cost, alignment
handling and multi-block behaviour. The diagnostics benchmarks measure
diagnostic construction, filtering, sorting and formatting.

### Coding standards

- C++20 throughout, using the project type aliases (`Ptr`, `Vec`, `Opt`,
  `Result<T, E>`, `String`, `StringView`, `usize`, `u32`, …) from
  `compiler/common/include/photon/common/types.hpp`
- Errors are returned as `Result<T, E>`; exceptions are reserved for genuinely
  exceptional conditions such as allocation failure
- Public classes and functions carry Doxygen documentation, including
  `@complexity` and, where relevant, `@thread_safety`
- New behaviour arrives with tests. A bug fix starts with a test that reproduces
  the defect and fails before the fix lands

### Quality gates

Before a change is merged it must build without warnings, keep the entire test
suite passing, and stay clean under AddressSanitizer, UndefinedBehaviorSanitizer
and ThreadSanitizer.

## Design goals

These are the targets the implementation is working towards. They are stated as
goals, not as measured results — none of them can be validated until the back
end exists.

| Metric              | Target                        |
|---------------------|-------------------------------|
| Lexing throughput   | 150 MB/s                      |
| Parsing throughput  | 80 MB/s                       |
| Type checking       | < 50 ms for 10K LOC           |
| Code generation     | < 500 ms                      |
| Emitted binary size | ~100 KB for a minimal program |
| Runtime overhead    | < 1%                          |

## Roadmap

**Current milestone — front end**

- [x] Arena allocator
- [x] Diagnostics engine with source context and colour output
- [x] Source manager with UTF-8 validation and line mapping
- [x] Lexer
- [x] Parser and AST with error recovery
- [ ] Remaining statement and declaration grammar (control flow, `struct`,
      `enum`, `trait`, `impl`)

**Next — semantic analysis**

- [ ] Symbol tables and scope resolution
- [ ] Type system and inference
- [ ] Ownership and borrow checking

**Later — back end and tooling**

- [ ] LLVM IR generation
- [ ] Async runtime
- [ ] Module system and incremental compilation
- [ ] Language server and formatter

## Contributing

Contributions are welcome. Please open an issue to discuss substantial changes
before starting work.

1. Create a feature branch off `main`
2. Add tests covering the change, and make sure they fail before your fix
3. Verify the full suite passes, including a sanitizer build
4. Open a pull request describing the change and how it was verified

## License

A license file has not yet been added to this repository. Please open an issue
if you need clarity on usage terms before the project settles on one.
