# 🚀 Photon Compiler Development Prompt

## 🎯 Development Directive

You are tasked with implementing the Photon programming language compiler. Your code must be **production-grade**, **architecturally sound**, and **rigorously tested**. No placeholder implementations, no basic solutions—only advanced, robust code that could be deployed immediately. each feature must be on its own branch

## 🛡️ Core Requirements

### Code Quality Standards

1. **Advanced Implementation Only**
    - No trivial/textbook implementations
    - Use modern C++20 features (concepts, ranges, coroutines where appropriate)
    - Implement proper error handling with `std::expected`
    - Zero memory leaks, zero undefined behavior
    - Optimize for both compilation speed and runtime performance

2. **Architecture Principles**
    - **SOLID principles** strictly enforced
    - **Domain-Driven Design** for compiler phases
    - **Hexagonal Architecture** for external dependencies
    - **Repository Pattern** for symbol tables and AST storage
    - **Command Pattern** for compiler passes

3. **Separation of Concerns**
    - Each class has a single, well-defined responsibility
    - Business logic separated from I/O operations
    - Pure functions wherever possible
    - Dependency injection for all external dependencies
    - Interface segregation for all public APIs

## 📋 Development Workflow

### For EVERY Feature Implementation:

#### 1. **Task Decomposition**
Before implementing any feature, create a detailed task breakdown:

```markdown
## Feature: [Feature Name]

### Subtasks:
- [ ] Design interface and API contracts
- [ ] Implement core data structures
- [ ] Implement business logic (pure functions)
- [ ] Add error handling and validation
- [ ] Create unit tests (minimum 90% coverage)
- [ ] Create integration tests
- [ ] Add benchmarks
- [ ] Write performance tests
- [ ] Update documentation
- [ ] Verify thread safety
- [ ] Run static analysis
- [ ] Profile memory usage
```

#### 2. **Pre-Implementation Analysis**
```markdown
## Implementation Plan: [Component Name]

### Affected Files:
- New: [List new files to create]
- Modified: [List existing files to modify]
- Tests: [List test files needed]

### Dependencies:
- Internal: [List internal dependencies]
- External: [List external dependencies]

### Interfaces:
- Public API: [Define public interface]
- Internal API: [Define internal interface]

### Performance Targets:
- Time Complexity: [Specify Big-O]
- Space Complexity: [Specify Big-O]
- Benchmark Target: [Specific metric]
```

#### 3. **Implementation Requirements**

For EACH subtask:

1. **Write Interface First**
   ```cpp
   // Define complete interface with documentation
   class ILexer {
   public:
       virtual ~ILexer() = default;
       [[nodiscard]] virtual auto tokenize(std::string_view source) 
           -> Result<Vec<Token>, LexicalError> = 0;
   };
   ```

2. **Write Tests BEFORE Implementation**
   ```cpp
   TEST_F(LexerTest, HandlesEmptyInput) {
       auto result = lexer->tokenize("");
       ASSERT_TRUE(result.has_value());
       ASSERT_EQ(result->size(), 1);
       EXPECT_EQ(result->at(0).type, TokenType::Eof);
   }
   ```

3. **Implement with Full Error Handling**
    - No `throw` without strong exception guarantee
    - All errors must be recoverable or fatal (clearly distinguished)
    - Resource cleanup guaranteed (RAII)

4. **Verify Implementation**
    - Run ALL tests: `ctest --output-on-failure`
    - Run sanitizers: `./build/asan/tests/[component]_test`
    - Check coverage: `gcov` or `llvm-cov`
    - Static analysis: `clang-tidy`
    - Memory profiling: `valgrind --leak-check=full`

#### 4. **Quality Gates (MUST PASS ALL)**

Before moving to next subtask:

```bash
# 1. All tests pass
ctest --preset=dev --output-on-failure

# 2. No memory leaks
valgrind --leak-check=full --error-exitcode=1 ./build/dev/tests/[component]_test

# 3. No undefined behavior
./build/asan/tests/[component]_test
./build/ubsan/tests/[component]_test

# 4. Code coverage > 90%
cmake --build build/coverage --target coverage
# Verify: build/coverage/coverage.html

# 5. No linting errors
cmake --build build/dev --target tidy

# 6. Performance benchmarks pass
./build/release/benchmarks/[component]_bench --benchmark_min_time=1

# 7. Thread safety verified (if applicable)
./build/tsan/tests/[component]_test
```

#### 5. **Commit Protocol**

After ALL quality gates pass:

```bash
# Stage files with atomic commits
git add [files]

# Commit with detailed message
git commit -m "feat(component): Add [specific feature]

- Implement [specific functionality]
- Add comprehensive test coverage (X%)
- Benchmark results: [metric] at X MB/s
- Memory usage: Peak X MB for Y operations

Tested with:
- GCC 11.3, Clang 14.0, MSVC 2022
- AddressSanitizer: Clean
- ThreadSanitizer: Clean
- Valgrind: No leaks
- Coverage: X%

Refs: #issue_number"
```

## 🏗️ Implementation Order

### Phase 1: Foundation
1. **Memory Arena Allocator**
    - Subtasks: Design API → Implement allocation → Add reset → Test alignment → Benchmark

2. **Diagnostic System**
    - Subtasks: Error types → Source locations → Error formatting → Color output → Test coverage

3. **Source Manager**
    - Subtasks: File loading → UTF-8 validation → Line mapping → Performance optimization

### Phase 2: Frontend
1. **Lexer**
    - Subtasks: Token types → DFA implementation → Keyword perfect hash → Error recovery → Streaming mode → Performance optimization

2. **Parser**
    - Subtasks: Grammar definition → Pratt parser → Error recovery → AST construction → Incremental parsing preparation

3. **AST**
    - Subtasks: Node hierarchy → Visitor pattern → Serialization → Pretty printing → Memory efficiency

### Phase 3: Analysis
1. **Symbol Table**
    - Subtasks: Scope management → Name lookup → Type storage → Import resolution → Concurrent access

2. **Type System**
    - Subtasks: Type representation → Inference engine → Constraint solver → Generic instantiation → Type checking

3. **Borrow Checker**
    - Subtasks: Lifetime inference → Move analysis → Borrow validation → Error messages → Optimization

### Phase 4: Backend
1. **LLVM Integration**
    - Subtasks: Context management → Module creation → IR generation → Optimization passes → Object emission

2. **Runtime Library**
    - Subtasks: Memory allocator → Async scheduler → Panic handler → Platform abstraction

## 🔍 Code Review Checklist

Before considering ANY component complete:

- [ ] **Interface Design**
    - Clear separation of concerns
    - Minimal public API surface
    - Proper use of `[[nodiscard]]`, `noexcept`, `constexpr`
    - No implementation details leaked

- [ ] **Implementation Quality**
    - No code duplication (DRY principle)
    - Algorithms are optimal for use case
    - Memory usage is minimized
    - No premature optimization
    - Clear error messages

- [ ] **Testing**
    - Unit tests for all public methods
    - Integration tests for component interactions
    - Edge cases covered
    - Error paths tested
    - Performance regression tests

- [ ] **Documentation**
    - All public APIs documented
    - Complex algorithms explained
    - Performance characteristics noted
    - Thread safety documented

## 🎭 Example Implementation Pattern

```cpp
// 1. Interface (parser.hpp)
class IParser {
public:
    virtual ~IParser() = default;
    [[nodiscard]] virtual auto parse(TokenStream tokens) 
        -> Result<AST, ParseError> = 0;
};

// 2. Test (parser_test.cpp)
TEST_F(ParserTest, ParsesSimpleFunction) {
    auto tokens = tokenize("fn add(a: i32, b: i32) -> i32 { a + b }");
    auto ast = parser->parse(tokens);
    
    ASSERT_TRUE(ast.has_value());
    auto* func = ast->root->as<FunctionDecl>();
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name(), "add");
    EXPECT_EQ(func->parameters().size(), 2);
}

// 3. Implementation (parser.cpp)
class Parser final : public IParser {
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
public:
    explicit Parser(ParserOptions options);
    ~Parser();
    
    [[nodiscard]] auto parse(TokenStream tokens) 
        -> Result<AST, ParseError> override;
};

// 4. Benchmark (parser_bench.cpp)
BENCHMARK(ParseComplexFunction)->Unit(benchmark::kMicrosecond);
```

## 🚨 Critical Rules

1. **NEVER** proceed to next task if current tests fail
2. **NEVER** commit code with < 90% coverage
3. **NEVER** ignore sanitizer warnings
4. **NEVER** use raw pointers (except LLVM interop)
5. **NEVER** catch exceptions without proper handling
6. **ALWAYS** run full test suite before commits
7. **ALWAYS** profile before optimization
8. **ALWAYS** document non-obvious algorithms

## 🎯 Success Metrics

Each component must achieve:
- ✅ 100% test pass rate
- ✅ 90%+ code coverage
- ✅ Zero memory leaks
- ✅ Zero race conditions
- ✅ Performance within 10% of target
- ✅ Clean static analysis
- ✅ Documented public API
- ✅ Benchmark baselines established

**Remember**: You're building a production compiler that will be used by thousands. Every line of code matters. No shortcuts. No "good enough". Only excellence.