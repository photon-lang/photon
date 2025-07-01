/**
 * @file lexer.hpp
 * @brief High-performance lexical analyzer for the Photon programming language
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#pragma once

#include "photon/lexer/token.hpp"
#include "photon/source/source_manager.hpp"
#include "photon/memory/arena.hpp"
#include <memory>

namespace photon::lexer {

/**
 * @brief Lexer configuration options
 */
struct LexerOptions {
    bool preserve_whitespace = false;     ///< Keep whitespace tokens
    bool preserve_comments = false;       ///< Keep comment tokens  
    bool enable_streaming = false;        ///< Enable streaming mode for large files
    usize buffer_size = 64 * 1024;       ///< Buffer size for streaming (64KB)
    bool strict_mode = true;              ///< Strict error handling vs recovery
    bool optimize_identifiers = true;     ///< Use string interning for identifiers
};

/**
 * @brief Abstract lexer interface for dependency injection
 */
class ILexer {
public:
    virtual ~ILexer() = default;
    
    /**
     * @brief Tokenize source content into a token stream
     * @param source_id File ID from source manager
     * @return Token stream or lexical error
     */
    [[nodiscard]] virtual auto tokenize(source::FileID source_id) 
        -> Result<TokenStream, LexicalError> = 0;
    
    /**
     * @brief Tokenize string content directly
     * @param content Source code content
     * @param filename Virtual filename for error reporting
     * @return Token stream or lexical error
     */
    [[nodiscard]] virtual auto tokenize(StringView content, StringView filename = "<string>")
        -> Result<TokenStream, LexicalError> = 0;
    
    /**
     * @brief Get lexer statistics for performance monitoring
     */
    struct Statistics {
        usize tokens_produced = 0;        ///< Total tokens generated
        usize bytes_processed = 0;        ///< Total bytes lexed
        usize lines_processed = 0;        ///< Total lines processed
        f64 tokens_per_second = 0.0;      ///< Tokenization rate
        usize memory_used = 0;            ///< Peak memory usage
        usize errors_recovered = 0;       ///< Number of errors recovered from
    };
    
    [[nodiscard]] virtual auto get_statistics() const noexcept -> Statistics = 0;
    virtual auto reset_statistics() noexcept -> void = 0;
};

/**
 * @brief High-performance DFA-based lexer implementation
 * 
 * Features:
 * - Deterministic Finite Automaton for optimal performance
 * - Perfect hash table for keyword recognition
 * - Streaming support for large files
 * - Error recovery with detailed diagnostics
 * - Memory-efficient token representation
 * - SIMD optimizations for common patterns
 */
class Lexer final : public ILexer {
private:
    class Impl;                           ///< PIMPL for implementation hiding
    std::unique_ptr<Impl> impl_;
    
public:
    /**
     * @brief Construct lexer with source manager and options
     * @param source_manager Source file management system
     * @param arena Memory arena for allocations
     * @param options Lexer configuration
     */
    explicit Lexer(source::SourceManager& source_manager,
                   memory::MemoryArena<>& arena,
                   LexerOptions options = {});
    
    ~Lexer();
    
    // Non-copyable but movable
    Lexer(const Lexer&) = delete;
    auto operator=(const Lexer&) -> Lexer& = delete;
    Lexer(Lexer&&) noexcept;
    auto operator=(Lexer&&) noexcept -> Lexer&;
    
    // ILexer implementation
    [[nodiscard]] auto tokenize(source::FileID source_id) 
        -> Result<TokenStream, LexicalError> override;
    
    [[nodiscard]] auto tokenize(StringView content, StringView filename = "<string>")
        -> Result<TokenStream, LexicalError> override;
    
    [[nodiscard]] auto get_statistics() const noexcept -> Statistics override;
    auto reset_statistics() noexcept -> void override;
    
    /**
     * @brief Advanced tokenization with custom error handler
     * @param source_id Source file identifier
     * @param error_handler Custom error recovery function
     * @return Token stream with potentially recovered errors
     */
    [[nodiscard]] auto tokenize_with_recovery(
        source::FileID source_id,
        std::function<bool(LexicalError, diagnostics::SourceLocation)> error_handler)
        -> Result<TokenStream, LexicalError>;
    
    /**
     * @brief Streaming tokenization for very large files
     * @param source_id Source file identifier
     * @param callback Token processing callback
     * @return Success or error status
     */
    [[nodiscard]] auto tokenize_streaming(
        source::FileID source_id,
        std::function<bool(Token)> callback)
        -> Result<std::monostate, LexicalError>;
};

/**
 * @brief Factory for creating lexers with standard configurations
 */
class LexerFactory {
public:
    /**
     * @brief Create a standard lexer for compilation
     * @param source_manager Source management system
     * @param arena Memory arena
     * @return Configured lexer instance
     */
    [[nodiscard]] static auto create_standard_lexer(
        source::SourceManager& source_manager,
        memory::MemoryArena<>& arena) -> std::unique_ptr<ILexer>;
    
    /**
     * @brief Create a lexer optimized for IDE features
     * @param source_manager Source management system  
     * @param arena Memory arena
     * @return IDE-optimized lexer instance
     */
    [[nodiscard]] static auto create_ide_lexer(
        source::SourceManager& source_manager,
        memory::MemoryArena<>& arena) -> std::unique_ptr<ILexer>;
    
    /**
     * @brief Create a lexer for testing with relaxed error handling
     * @param source_manager Source management system
     * @param arena Memory arena
     * @return Test-configured lexer instance
     */
    [[nodiscard]] static auto create_test_lexer(
        source::SourceManager& source_manager,
        memory::MemoryArena<>& arena) -> std::unique_ptr<ILexer>;
};

} // namespace photon::lexer