# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **Photon Compiler** project - a revolutionary C++20 compiler implementation for the Photon programming language. Photon is a "next-generation systems programming language designed to address the limitations of current languages while maintaining simplicity and blazing-fast performance."

### The Photon Language Vision

Photon is designed as an ultra-lightweight programming language that "moves at the speed of light" with these revolutionary features:

- **⚡ Lightning-fast Compilation** - Incremental compilation with < 1s builds 
- **🛡️ Memory Safety Without GC** - "Quantum ownership" system simpler than Rust
- **🔄 Native Async/Await** - First-class concurrency with zero-overhead async runtime
- **🧬 Quantum Types** - Native support for quantum computing primitives (unique feature!)
- **🔧 C++ Interoperability** - Seamless integration with existing C++ codebases
- **📦 Modern Module System** - Fast, reliable dependency management
- **🎯 Zero-Cost Abstractions** - High-level features compile to optimal machine code

### Design Philosophy (Physics-Inspired)

1. **Speed of Light**: Both compilation and runtime performance are paramount
2. **Minimal Mass**: Every feature must justify its weight in the language  
3. **Wave-Particle Duality**: Can act as both high-level and low-level language
4. **Quantum Safety**: Compile-time guarantees prevent runtime uncertainties
5. **Energy Efficient**: Minimal resource usage in both compiler and output

## Build System

- **Build Tool**: CMake (minimum version 3.31)
- **C++ Standard**: C++20
- **Build Configuration**: 
  ```bash
  # Debug build (CLion default)
  cd cmake-build-debug
  ninja
  
  # Run the executable
  ./photonc
  ```

## Development Architecture

The project follows a structured compiler development approach with these planned phases:

### Phase 1: Foundation
- Lexer with perfect hash keyword lookup
- Diagnostics system with source location tracking  
- Memory pool allocators for AST nodes
- GoogleTest integration for testing

### Phase 2: Frontend
- Pratt parser with precedence climbing
- AST with visitor pattern and const-correctness
- Symbol table with efficient scope management
- Type system supporting algebraic data types

### Phase 3: Analysis  
- Hindley-Milner type inference with extensions
- Single-pass borrow checker for lifetime analysis
- Optimization passes (constant folding, DCE)
- Error recovery during parsing

### Phase 4: Backend
- LLVM IR generation
- Async scheduler runtime implementation
- Module system with incremental compilation
- LSP server foundation

## Code Standards

### Type Aliases (Required)
```cpp
namespace photon {
    template<typename T> using Ptr = std::unique_ptr<T>;
    template<typename T> using Ref = std::shared_ptr<T>;
    template<typename T> using Vec = std::vector<T>;
    template<typename T> using Opt = std::optional<T>;
    template<typename T, typename E> using Result = std::expected<T, E>;
    using String = std::string;
    using StringView = std::string_view;
}
```

### Documentation Requirements
- **Mandatory**: Full Doxygen documentation for all classes and functions
- **Forbidden**: Any non-documentation comments (`//`, `/* */`, `// TODO`, etc.)
- Include `@brief`, `@param`, `@return`, `@throws`, `@complexity`, `@performance` tags
- Document invariants, preconditions, and postconditions

### Architecture Patterns
- **Error Handling**: Monadic `Result<T, E>` type with `map()`, `and_then()`, `map_error()`
- **AST Design**: CRTP base classes to avoid virtual dispatch
- **Expression Problem**: `std::variant` with type-safe visitors
- **Memory Management**: Arena allocators and smart pointer factories
- **Performance**: Small string optimization, perfect hash lookup, zero-copy parsing

## File Structure

```
photonc/
├── CMakeLists.txt          # Build configuration
├── main.cpp               # Entry point (currently template)
├── docs/
│   ├── guidelines.md      # Comprehensive development standards
│   └── *.pdf             # Language documentation
└── cmake-build-debug/    # Build artifacts
```

## Photon Language Features

### Syntax Highlights
```photon
// Hello World - uses emit() instead of print
fn main() {
    emit("Hello, Photon!")
}

// Variables immutable by default, 'mut' for mutable
let x = 42
let mut counter = 0

// Async/await as first-class citizens
async fn simulate_photon(wavelength: f64) -> Result<Path, Error> {
    let source = await Laser::initialize(wavelength)?
    let beam = await source.emit()?
    Ok(await beam.propagate()?)
}

// Pattern matching with ranges and guards  
match photon {
    Photon { energy: 0 } => emit("vacuum fluctuation"),
    Photon { energy: e } if e < 1.0 => emit("infrared"),
    Photon { energy: 1.0..3.0 } => emit("visible light"),
    _ => emit("unknown")
}

// Quantum types (unique feature!)
let qubit = Qubit::new()
let superposition = qubit.hadamard()
let psi = |0⟩ + |1⟩  // Quantum literals!
```

### Memory Management: "Quantum Ownership"
- Simpler than Rust's ownership system
- Values have single owner (particles can't be in two places)
- Ownership can transfer (quantum tunneling)
- Multiple immutable observers (wave function)
- Single mutable observer (measurement collapses wave)

### Unique Quantum Computing Features
- Native quantum types: `Qubit`, `QReg` 
- Quantum literals: `|0⟩`, `|1⟩`, `|00⟩ + |11⟩`
- Quantum operations as first-class functions
- Pattern matching on quantum states
- Quantum circuit DSL via macros

## Key Development Principles

1. **Production Quality**: All code must be immediately production-ready
2. **Modern C++20**: Leverage concepts, coroutines, modules where appropriate  
3. **Zero-Cost Abstractions**: Performance-critical design throughout
4. **Type Safety**: Strong typing with compile-time validation
5. **Memory Safety**: RAII, smart pointers, and custom allocators
6. **Error Resilience**: Comprehensive error handling with source locations
7. **Physics-Inspired Design**: Apply quantum mechanics principles to language design

## Testing Strategy

- **Unit Tests**: GoogleTest framework integration planned
- **Property Testing**: Property-based testing for compiler components
- **Fuzzing**: LibFuzzer integration for robustness
- **Performance**: Compile-time profiling and memory tracking

## Performance Considerations

- Custom arena allocators for AST nodes
- Perfect hash tables for keyword lookup  
- Small string optimization for identifiers
- Zero-copy string handling where possible
- Parallel compilation support planned

## Target Applications

Photon is designed for next-generation applications including:
- **High-Performance Computing**: Ray tracing engines, physics simulations
- **Quantum Computing**: Native quantum circuit simulation and programming
- **Web Services**: Async web frameworks with minimal overhead  
- **Systems Programming**: Operating systems, embedded systems, compilers
- **Scientific Computing**: Optics simulation, wave mechanics, field theory

## Compiler Performance Targets

The compiler aims for exceptional performance benchmarks:
- **Lexing Speed**: 150 MB/s (3x faster than Rust)
- **Parse Time**: 80 MB/s (2.5x faster than Go) 
- **Type Check**: < 50ms for 10K LOC (10x faster than TypeScript)
- **Code Generation**: < 500ms (on par with Clang)
- **Binary Size**: ~100KB (50% smaller than Go)

## The Magic

Photon's revolutionary approach combines:
1. **Physics-inspired language design** - Quantum mechanics principles applied to programming
2. **Quantum ownership system** - Simpler memory safety than Rust using physics metaphors
3. **Native quantum types** - First programming language with built-in quantum computing support
4. **Ultra-fast compilation** - Sub-second builds through incremental compilation
5. **Zero-overhead async** - Coroutines that compile to optimal machine code
6. **Seamless C++ interop** - Direct integration with existing C++ ecosystems

The codebase emphasizes production-ready compiler implementation with modern C++ best practices, comprehensive documentation requirements, and groundbreaking quantum computing integration.