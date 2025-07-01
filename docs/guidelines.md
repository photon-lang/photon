# 🚀 Photon Compiler Development Guidelines

## 🛡 Enforcement Objective
All AI-generated C++ code for the Photon compiler must be **production-ready**, **rigorously tested**, and **adhere to modern C++20 best practices**. The code must be of such quality that it can be immediately integrated into a production compiler without modification.

**⚠️ Critical Rule:** Under no circumstances should the code contain any non-documentation comments, including `//`, `/* */`, `// TODO`, `// FIXME`, or similar. **Only Doxygen-style documentation blocks for classes, functions, and modules are permitted.**

---

## 📋 Development Phases

### Phase 1: Foundation
- **Lexer**: Token generation with perfect hash lookup
- **Diagnostics**: Source location tracking and error reporting
- **Memory Pool**: Custom allocators for AST nodes
- **Testing Framework**: GoogleTest integration

### Phase 2: Frontend
- **Parser**: Pratt parser with precedence climbing
- **AST**: Visitor pattern with const-correctness
- **Symbol Table**: Efficient scope management
- **Type System**: Algebraic data types support

### Phase 3: Analysis
- **Type Inference**: Hindley-Milner with extensions
- **Borrow Checker**: Single-pass lifetime analysis
- **Optimization**: Constant folding and DCE
- **Error Recovery**: Continue parsing after errors

### Phase 4: Backend
- **LLVM Integration**: IR generation
- **Runtime Library**: Async scheduler implementation
- **Module System**: Incremental compilation
- **Tooling**: LSP server foundation

---

## 📚 Documentation Standards

### **Module Documentation**
```cpp
/**
 * @file lexer.hpp
 * @brief Lexical analyzer for the Photon programming language
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This module provides high-performance tokenization of Photon source code
 * with zero-copy string handling and perfect hash lookup for keywords.
 */
```

### **Class Documentation**
```cpp
/**
 * @class Lexer
 * @brief High-performance lexical analyzer with streaming support
 * 
 * The Lexer class tokenizes Photon source code using a hand-optimized DFA
 * for maximum performance. It supports incremental parsing and provides
 * detailed error diagnostics with source location tracking.
 * 
 * @tparam Allocator Memory allocator for token storage
 * 
 * @invariant The lexer never modifies the source buffer
 * @invariant All tokens contain valid source locations
 * 
 * @performance O(n) time complexity, O(1) space for streaming mode
 * @thread_safety Not thread-safe, use separate instances per thread
 */
template<typename Allocator = std::allocator<Token>>
class Lexer final {
```

### **Function Documentation**
```cpp
/**
 * @brief Tokenizes the entire source buffer
 * @param source UTF-8 encoded source code
 * @return Vector of tokens or lexical error
 * 
 * @throws std::bad_alloc If memory allocation fails
 * 
 * @pre source must be valid UTF-8
 * @post Returns either complete token stream ending with EOF or error
 * 
 * @complexity O(n) where n is source length
 * @memory O(m) where m is number of tokens
 * 
 * @example
 * ```cpp
 * auto result = lexer.tokenize("let x = 42");
 * if (result) {
 *     for (const auto& token : result.value()) {
 *         process_token(token);
 *     }
 * }
 * ```
*/
[[nodiscard]] auto tokenize(std::string_view source)
-> Result<Vec<Token>, LexicalError>;
```

---

## ✅ Required Standards

### **Type Aliases for Clarity**
```cpp
namespace photon {
    template<typename T>
    using Ptr = std::unique_ptr<T>;
    
    template<typename T>
    using Ref = std::shared_ptr<T>;
    
    template<typename T>
    using WeakRef = std::weak_ptr<T>;
    
    template<typename T>
    using Vec = std::vector<T>;
    
    template<typename T>
    using Opt = std::optional<T>;
    
    template<typename T, typename E>
    using Result = std::expected<T, E>;
    
    template<typename K, typename V>
    using Map = std::unordered_map<K, V>;
    
    template<typename T>
    using Set = std::unordered_set<T>;
    
    using String = std::string;
    using StringView = std::string_view;
}
```

### **Error Types Hierarchy**
```cpp
/**
 * @class CompilerError
 * @brief Base class for all compiler errors with source location
 */
class CompilerError : public std::exception {
    SourceLocation location_;
    String message_;
    ErrorCode code_;
    
public:
    [[nodiscard]] auto what() const noexcept -> const char* override;
    [[nodiscard]] auto location() const noexcept -> const SourceLocation&;
    [[nodiscard]] auto code() const noexcept -> ErrorCode;
    [[nodiscard]] auto format() const -> String;
};
```

### **Smart Pointer Factory Functions**
```cpp
template<typename T, typename... Args>
[[nodiscard]] auto make_ptr(Args&&... args) -> Ptr<T> {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
[[nodiscard]] auto make_ref(Args&&... args) -> Ref<T> {
    return std::make_shared<T>(std::forward<Args>(args)...);
}
```

### **Concepts for Template Constraints**
```cpp
template<typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template<typename T>
concept Visitable = requires(T t) {
    { t.accept(std::declval<ASTVisitor&>()) };
};

template<typename T>
concept Serializable = requires(T t, ByteBuffer& buffer) {
    { t.serialize(buffer) } -> std::same_as<void>;
    { T::deserialize(buffer) } -> std::same_as<Result<T, SerializationError>>;
};
```

---

## 🏗️ Architecture Patterns

### **Monadic Error Handling**
```cpp
/**
 * @brief Monadic operations for Result type
 */
template<typename T, typename E>
class [[nodiscard]] Result {
public:
    template<typename F>
    [[nodiscard]] auto map(F&& f) -> Result<std::invoke_result_t<F, T>, E>;
    
    template<typename F>
    [[nodiscard]] auto and_then(F&& f) -> std::invoke_result_t<F, T>;
    
    template<typename F>
    [[nodiscard]] auto map_error(F&& f) -> Result<T, std::invoke_result_t<F, E>>;
    
    [[nodiscard]] auto value_or(T default_value) -> T;
    
    [[nodiscard]] auto expect(StringView message) -> T;
};
```

### **Expression Problem Solution**
```cpp
/**
 * @brief Type-safe expression visitor using std::variant
 */
using Expression = std::variant
    BinaryExpression,
    UnaryExpression,
    LiteralExpression,
    IdentifierExpression,
    CallExpression,
    LambdaExpression
>;

template<typename Visitor>
[[nodiscard]] auto visit_expression(Visitor&& vis, const Expression& expr) {
    return std::visit(std::forward<Visitor>(vis), expr);
}
```

### **CRTP for Static Polymorphism**
```cpp
/**
 * @brief CRTP base for AST nodes to avoid virtual dispatch
 */
template<typename Derived>
class ASTNode {
public:
    [[nodiscard]] auto source_location() const -> const SourceLocation& {
        return static_cast<const Derived*>(this)->location_;
    }
    
    template<typename Visitor>
    [[nodiscard]] auto accept(Visitor&& visitor) {
        return visitor.visit(static_cast<Derived&>(*this));
    }
};
```

---

## 🔧 Performance Optimizations

### **Small String Optimization**
```cpp
/**
 * @class SmallString
 * @brief String with small buffer optimization for identifiers
 * @tparam N Size of inline buffer (default 23 for 32-byte total)
 */
template<std::size_t N = 23>
class SmallString {
    union {
        char inline_buffer_[N + 1];
        struct {
            char* heap_buffer_;
            std::size_t size_;
            std::size_t capacity_;
        };
    };
    bool is_small_;
};
```

### **Memory Pool Allocator**
```cpp
/**
 * @class ArenaAllocator
 * @brief Fast bump allocator for AST nodes
 * 
 * @performance O(1) allocation, no individual deallocation
 * @thread_safety Thread-safe with mutex, or use thread_local pools
 */
template<std::size_t BlockSize = 4096>
class ArenaAllocator {
    struct Block {
        alignas(std::max_align_t) char data[BlockSize];
        Ptr<Block> next;
    };
    
public:
    [[nodiscard]] auto allocate(std::size_t size, std::size_t alignment) 
        -> void*;
    
    auto reset() noexcept -> void;
};
```

### **Perfect Hash for Keywords**
```cpp
/**
 * @brief Compile-time perfect hash for keyword lookup
 * @note Generated using gperf or custom constexpr algorithm
 */
[[nodiscard]] constexpr auto is_keyword(StringView str) noexcept -> Opt<TokenType> {
    constexpr auto hash = [](StringView s) constexpr -> std::size_t {
        std::size_t h = 0;
        for (char c : s) h = h * 31 + c;
        return h;
    };
    
    switch (hash(str)) {
        case hash("let"): return TokenType::Let;
        case hash("mut"): return TokenType::Mut;
        case hash("fn"): return TokenType::Fn;
        default: return std::nullopt;
    }
}
```

---

## 🎯 Code Generation Helpers

### **LLVM IR Builder Wrapper**
```cpp
/**
 * @class IRBuilder
 * @brief Type-safe wrapper around LLVM IRBuilder
 */
class IRBuilder {
    llvm::IRBuilder<> builder_;
    llvm::LLVMContext& context_;
    
public:
    [[nodiscard]] auto create_add(Value lhs, Value rhs) 
        -> Result<Value, CodeGenError>;
    
    [[nodiscard]] auto create_call(Function func, Vec<Value> args) 
        -> Result<Value, CodeGenError>;
};
```

### **Pattern Matching Utilities**
```cpp
/**
 * @brief Pattern matching for algebraic data types
 */
template<typename... Patterns>
class Matcher {
public:
    template<typename T>
    [[nodiscard]] auto match(const T& value) -> Opt<std::any>;
};

#define MATCH(expr) \
    photon::match_builder{} << (expr) >>

#define CASE(pattern, action) \
    photon::case_builder{pattern, [&]{ return action; }}
```

---

## 🧪 Testing Utilities

### **Property-Based Testing**
```cpp
/**
 * @brief Property testing for compiler components
 */
template<typename T>
class Property {
public:
    [[nodiscard]] static auto arbitrary() -> Generator<T>;
    
    template<typename Predicate>
    auto check(Predicate&& pred, std::size_t num_tests = 100) -> TestResult;
};

PROPERTY_TEST(ParserRoundTrip, 
    Property<Expression>::check([](const Expression& expr) {
        auto serialized = serialize(expr);
        auto parsed = parse(serialized);
        return parsed && *parsed == expr;
    })
);
```

### **Fuzzing Harness**
```cpp
/**
 * @brief LibFuzzer integration for robustness testing
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    StringView source{reinterpret_cast<const char*>(data), size};
    
    Lexer lexer;
    auto tokens = lexer.tokenize(source);
    
    if (tokens) {
        Parser parser;
        auto ast = parser.parse(tokens.value());
    }
    
    return 0;
}
```

---

## 📊 Profiling and Metrics

### **Compile-Time Profiling**
```cpp
/**
 * @class CompileTimeProfiler
 * @brief Tracks compilation phase timings
 */
class CompileTimeProfiler {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    
    struct PhaseData {
        String name;
        std::chrono::nanoseconds duration;
        std::size_t memory_allocated;
    };
    
public:
    [[nodiscard]] auto start_phase(StringView name) -> PhaseGuard;
    [[nodiscard]] auto report() const -> ProfileReport;
};
```

### **Memory Usage Tracking**
```cpp
/**
 * @brief Custom allocator that tracks memory usage
 */
template<typename T>
class TrackingAllocator {
    static inline std::atomic<std::size_t> total_allocated_{0};
    static inline std::atomic<std::size_t> total_deallocated_{0};
    
public:
    [[nodiscard]] auto allocate(std::size_t n) -> T*;
    auto deallocate(T* p, std::size_t n) noexcept -> void;
    
    [[nodiscard]] static auto current_usage() noexcept -> std::size_t;
    [[nodiscard]] static auto peak_usage() noexcept -> std::size_t;
};
```

---

## 🚀 Advanced Features

### **Incremental Compilation Support**
```cpp
/**
 * @class IncrementalCompiler
 * @brief Supports incremental recompilation of changed modules
 */
class IncrementalCompiler {
    Map<ModuleId, ModuleInfo> module_cache_;
    DependencyGraph dep_graph_;
    
public:
    [[nodiscard]] auto compile_incremental(const Vec<SourceFile>& changed_files)
        -> Result<CompilationUnit, CompilerError>;
    
    [[nodiscard]] auto invalidate_dependents(ModuleId module) 
        -> Set<ModuleId>;
};
```

### **Parallel Compilation**
```cpp
/**
 * @brief Thread pool for parallel compilation phases
 */
class CompilerThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    
public:
    template<typename F>
    [[nodiscard]] auto async(F&& task) -> std::future<std::invoke_result_t<F>>;
    
    auto wait_all() -> void;
};
```

### **JIT Compilation Support**
```cpp
/**
 * @class JITCompiler
 * @brief Just-in-time compilation for REPL and hot reload
 */
class JITCompiler {
    Ptr<llvm::orc::LLJIT> jit_;
    
public:
    [[nodiscard]] auto compile_and_run(const Expression& expr) 
        -> Result<Value, JITError>;
    
    [[nodiscard]] auto add_module(llvm::Module module) 
        -> Result<void, JITError>;
};
```

---

## 🛡️ Security Considerations

### **Input Sanitization**
```cpp
/**
 * @brief Validates and sanitizes source input
 */
[[nodiscard]] auto sanitize_source(StringView input) 
    -> Result<String, ValidationError> {
    if (!is_valid_utf8(input)) {
        return std::unexpected(ValidationError::InvalidEncoding);
    }
    
    if (input.size() > MAX_SOURCE_SIZE) {
        return std::unexpected(ValidationError::SourceTooLarge);
    }
    
    return normalize_line_endings(input);
}
```

### **Resource Limits**
```cpp
/**
 * @class CompilerLimits
 * @brief Enforces resource limits to prevent DoS
 */
struct CompilerLimits {
    std::size_t max_ast_depth = 1000;
    std::size_t max_type_complexity = 100;
    std::size_t max_function_size = 10'000;
    std::chrono::seconds max_compile_time{30};
};
```

---

## 🎨 Code Generation Templates

### **Visitor Pattern Template**
```cpp
/**
 * @brief CRTP template for implementing visitors
 */
template<typename Derived, typename ReturnType = void>
class ExpressionVisitor {
public:
    [[nodiscard]] auto visit(const BinaryExpression& expr) -> ReturnType {
        return static_cast<Derived*>(this)->visit_binary(expr);
    }
    
    [[nodiscard]] auto visit(const UnaryExpression& expr) -> ReturnType {
        return static_cast<Derived*>(this)->visit_unary(expr);
    }
};
```

---

**Remember:** Every piece of code must be self-documenting through excellent naming and structure. Documentation blocks explain the "why" and performance characteristics, while the code itself clearly shows the "how".