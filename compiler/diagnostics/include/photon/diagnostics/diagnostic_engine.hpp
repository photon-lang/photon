/**
 * @file diagnostic_engine.hpp
 * @brief Central diagnostic collection and reporting engine
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This module provides the central diagnostic engine for collecting,
 * managing, and reporting compiler diagnostics with advanced filtering
 * and formatting capabilities.
 */

#pragma once

#include "photon/diagnostics/diagnostic.hpp"
#include "photon/memory/arena.hpp"
#include "photon/common/types.hpp"
#include <functional>
#include <atomic>

namespace photon::diagnostics {

/**
 * @class DiagnosticEngine
 * @brief Central engine for collecting and managing compiler diagnostics
 * 
 * DiagnosticEngine provides a high-performance system for collecting
 * diagnostics during compilation with support for filtering, limits,
 * and efficient memory management via arena allocation.
 * 
 * @invariant error_count <= total diagnostic count
 * @invariant fatal_count <= error_count
 * @invariant All diagnostics are arena-allocated for efficiency
 * 
 * @performance O(1) diagnostic addition, O(n) reporting
 * @thread_safety Thread-safe for concurrent diagnostic reporting
 */
class DiagnosticEngine {
public:
    /**
     * @brief Diagnostic filter predicate type
     */
    using FilterPredicate = std::function<bool(const Diagnostic&)>;

    /**
     * @brief Constructs diagnostic engine with arena allocator
     * @param arena Memory arena for diagnostic allocation
     * @param max_errors Maximum number of errors before stopping (0 = unlimited)
     */
    explicit DiagnosticEngine(memory::MemoryArena<>& arena, usize max_errors = 0) noexcept
        : arena_(arena)
        , max_errors_(max_errors)
        , error_count_(0)
        , warning_count_(0)
        , note_count_(0)
        , fatal_encountered_(false) {}

    // Non-copyable but movable
    DiagnosticEngine(const DiagnosticEngine&) = delete;
    auto operator=(const DiagnosticEngine&) -> DiagnosticEngine& = delete;

    DiagnosticEngine(DiagnosticEngine&&) noexcept = delete;
    auto operator=(DiagnosticEngine&&) noexcept -> DiagnosticEngine& = delete;

    /**
     * @brief Reports a diagnostic to the engine
     * @param diagnostic Diagnostic to report
     * @return True if diagnostic was accepted, false if limits exceeded
     * 
     * @post Diagnostic is stored in arena memory
     * @post Counters are updated appropriately
     */
    auto report(Diagnostic diagnostic) -> bool;

    /**
     * @brief Creates and reports a simple diagnostic
     * @param level Severity level
     * @param code Error code
     * @param message Diagnostic message
     * @param location Source location
     * @return True if diagnostic was accepted
     */
    auto report(DiagnosticLevel level, DiagnosticCode code, 
               String message, SourceLocation location) -> bool;

    /**
     * @brief Reports an error diagnostic
     * @param code Error code
     * @param message Error message
     * @param location Source location
     * @return True if diagnostic was accepted
     */
    auto error(DiagnosticCode code, String message, SourceLocation location) -> bool {
        return report(DiagnosticLevel::Error, code, std::move(message), location);
    }

    /**
     * @brief Reports a warning diagnostic
     * @param code Warning code
     * @param message Warning message
     * @param location Source location
     * @return True if diagnostic was accepted
     */
    auto warning(DiagnosticCode code, String message, SourceLocation location) -> bool {
        return report(DiagnosticLevel::Warning, code, std::move(message), location);
    }

    /**
     * @brief Reports a note diagnostic
     * @param message Note message
     * @param location Source location
     * @return True if diagnostic was accepted
     */
    auto note(String message, SourceLocation location) -> bool {
        return report(DiagnosticLevel::Note, DiagnosticCode(0), std::move(message), location);
    }

    /**
     * @brief Reports a fatal error and stops compilation
     * @param code Error code
     * @param message Fatal error message
     * @param location Source location
     * @return Always false (compilation should stop)
     */
    auto fatal(DiagnosticCode code, String message, SourceLocation location) -> bool;

    /**
     * @brief Gets the total number of diagnostics
     * @return Total diagnostic count
     */
    [[nodiscard]] auto total_count() const noexcept -> usize {
        return diagnostics_.size();
    }

    /**
     * @brief Gets the number of error-level diagnostics
     * @return Error count
     */
    [[nodiscard]] auto error_count() const noexcept -> usize {
        return error_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the number of warning diagnostics
     * @return Warning count
     */
    [[nodiscard]] auto warning_count() const noexcept -> usize {
        return warning_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Gets the number of note diagnostics
     * @return Note count
     */
    [[nodiscard]] auto note_count() const noexcept -> usize {
        return note_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Checks if any errors have been reported
     * @return True if error_count > 0
     */
    [[nodiscard]] auto has_errors() const noexcept -> bool {
        return error_count() > 0;
    }

    /**
     * @brief Checks if a fatal error was encountered
     * @return True if fatal error reported
     */
    [[nodiscard]] auto has_fatal_error() const noexcept -> bool {
        return fatal_encountered_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Checks if error limit has been reached
     * @return True if max_errors exceeded
     */
    [[nodiscard]] auto error_limit_reached() const noexcept -> bool {
        return max_errors_ > 0 && error_count() >= max_errors_;
    }

    /**
     * @brief Checks if compilation should stop
     * @return True if fatal error or error limit reached
     */
    [[nodiscard]] auto should_stop_compilation() const noexcept -> bool {
        return has_fatal_error() || error_limit_reached();
    }

    /**
     * @brief Gets all diagnostics
     * @return Vector of all diagnostics
     */
    [[nodiscard]] auto diagnostics() const noexcept -> const Vec<Diagnostic>& {
        return diagnostics_;
    }

    /**
     * @brief Gets diagnostics filtered by predicate
     * @param filter Filter predicate
     * @return Vector of filtered diagnostics
     */
    [[nodiscard]] auto filtered_diagnostics(const FilterPredicate& filter) const -> Vec<Diagnostic>;

    /**
     * @brief Gets diagnostics by severity level
     * @param level Target severity level
     * @return Vector of diagnostics at specified level
     */
    [[nodiscard]] auto diagnostics_by_level(DiagnosticLevel level) const -> Vec<Diagnostic>;

    /**
     * @brief Gets diagnostics by error code
     * @param code Target error code
     * @return Vector of diagnostics with specified code
     */
    [[nodiscard]] auto diagnostics_by_code(DiagnosticCode code) const -> Vec<Diagnostic>;

    /**
     * @brief Clears all diagnostics and resets counters
     * @post All diagnostic counts are zero
     * @post Arena memory is reset
     */
    auto clear() noexcept -> void;

    /**
     * @brief Sets maximum error count before stopping
     * @param max_errors Maximum errors (0 = unlimited)
     */
    auto set_max_errors(usize max_errors) noexcept -> void {
        max_errors_ = max_errors;
    }

    /**
     * @brief Gets the current maximum error limit
     * @return Maximum error count (0 = unlimited)
     */
    [[nodiscard]] auto max_errors() const noexcept -> usize {
        return max_errors_;
    }

    /**
     * @brief Gets memory usage statistics
     * @return Bytes used for diagnostic storage
     */
    [[nodiscard]] auto memory_usage() const noexcept -> usize {
        return arena_.bytes_used();
    }

    /**
     * @brief Sorts diagnostics by source location
     * @post Diagnostics are ordered by file, line, column
     */
    auto sort_by_location() -> void;

    /**
     * @brief Sorts diagnostics by severity level
     * @post Diagnostics are ordered by severity (fatal first)
     */
    auto sort_by_severity() -> void;

private:
    memory::MemoryArena<>& arena_;
    Vec<Diagnostic> diagnostics_;
    usize max_errors_;
    
    std::atomic<usize> error_count_;
    std::atomic<usize> warning_count_;
    std::atomic<usize> note_count_;
    std::atomic<bool> fatal_encountered_;

    auto update_counters(const Diagnostic& diagnostic) noexcept -> void;
};

/**
 * @class DiagnosticBuilder
 * @brief Fluent builder interface for creating complex diagnostics
 * 
 * DiagnosticBuilder provides a convenient fluent API for constructing
 * diagnostics with multiple notes and contextual information.
 * 
 * @performance Lightweight builder that constructs diagnostic on finalize
 */
class DiagnosticBuilder {
public:
    /**
     * @brief Constructs a diagnostic builder
     * @param engine Target diagnostic engine
     * @param level Severity level
     * @param code Error code
     * @param message Primary message
     * @param location Source location
     */
    DiagnosticBuilder(DiagnosticEngine& engine, DiagnosticLevel level,
                     DiagnosticCode code, String message, SourceLocation location) noexcept
        : engine_(engine)
        , diagnostic_(DiagnosticMessage(level, code, std::move(message), location)) {}

    /**
     * @brief Adds a note to the diagnostic
     * @param message Note message
     * @param location Note location
     * @return Reference to this builder for chaining
     */
    auto note(String message, SourceLocation location) -> DiagnosticBuilder& {
        diagnostic_.add_note(std::move(message), location);
        return *this;
    }

    /**
     * @brief Adds a suggestion note
     * @param suggestion Suggested fix
     * @param location Location for suggestion
     * @return Reference to this builder for chaining
     */
    auto suggest(String suggestion, SourceLocation location) -> DiagnosticBuilder& {
        return note("suggestion: " + suggestion, location);
    }

    /**
     * @brief Adds a help note
     * @param help_text Help information
     * @return Reference to this builder for chaining
     */
    auto help(String help_text) -> DiagnosticBuilder& {
        return note("help: " + help_text, SourceLocation{});
    }

    /**
     * @brief Finalizes and reports the diagnostic
     * @return True if diagnostic was accepted by engine
     */
    auto emit() -> bool {
        return engine_.report(std::move(diagnostic_));
    }

    /**
     * @brief Destructor automatically emits if not already emitted
     */
    ~DiagnosticBuilder() {
        if (!emitted_) {
            emit();
        }
    }

    // Move-only type
    DiagnosticBuilder(const DiagnosticBuilder&) = delete;
    auto operator=(const DiagnosticBuilder&) -> DiagnosticBuilder& = delete;

    DiagnosticBuilder(DiagnosticBuilder&& other) noexcept
        : engine_(other.engine_)
        , diagnostic_(std::move(other.diagnostic_))
        , emitted_(other.emitted_) {
        other.emitted_ = true;
    }

private:
    DiagnosticEngine& engine_;
    Diagnostic diagnostic_;
    bool emitted_ = false;
};

// Convenience factory functions for DiagnosticBuilder

/**
 * @brief Creates an error diagnostic builder
 * @param engine Target diagnostic engine
 * @param code Error code
 * @param message Error message
 * @param location Source location
 * @return DiagnosticBuilder for fluent interface
 */
[[nodiscard]] inline auto make_error(DiagnosticEngine& engine, DiagnosticCode code,
                                    String message, SourceLocation location) -> DiagnosticBuilder {
    return DiagnosticBuilder(engine, DiagnosticLevel::Error, code, std::move(message), location);
}

/**
 * @brief Creates a warning diagnostic builder
 * @param engine Target diagnostic engine
 * @param code Warning code
 * @param message Warning message
 * @param location Source location
 * @return DiagnosticBuilder for fluent interface
 */
[[nodiscard]] inline auto make_warning(DiagnosticEngine& engine, DiagnosticCode code,
                                      String message, SourceLocation location) -> DiagnosticBuilder {
    return DiagnosticBuilder(engine, DiagnosticLevel::Warning, code, std::move(message), location);
}

/**
 * @brief Creates a fatal error diagnostic builder
 * @param engine Target diagnostic engine
 * @param code Error code
 * @param message Fatal error message
 * @param location Source location
 * @return DiagnosticBuilder for fluent interface
 */
[[nodiscard]] inline auto make_fatal(DiagnosticEngine& engine, DiagnosticCode code,
                                    String message, SourceLocation location) -> DiagnosticBuilder {
    return DiagnosticBuilder(engine, DiagnosticLevel::Fatal, code, std::move(message), location);
}

} // namespace photon::diagnostics