/**
 * @file diagnostic.hpp
 * @brief Comprehensive diagnostic system for compiler error reporting
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This module provides a sophisticated diagnostic system with hierarchical
 * error types, source location tracking, and advanced formatting capabilities
 * for superior developer experience.
 */

#pragma once

#include "photon/diagnostics/source_location.hpp"
#include "photon/memory/arena.hpp"
#include "photon/common/types.hpp"
#include <format>

namespace photon::diagnostics {

/**
 * @enum DiagnosticLevel
 * @brief Severity levels for diagnostic messages
 */
enum class DiagnosticLevel : u8 {
    Note = 0,     ///< Informational note
    Warning = 1,  ///< Warning that doesn't prevent compilation
    Error = 2,    ///< Error that prevents successful compilation
    Fatal = 3     ///< Fatal error that stops compilation immediately
};

/**
 * @enum DiagnosticCode
 * @brief Structured error codes for different diagnostic categories
 * 
 * Error codes follow the pattern: CATEGORY_SPECIFIC_ERROR
 * This allows for easy categorization and filtering of diagnostics.
 */
enum class DiagnosticCode : u32 {
    // Lexical errors (1000-1999)
    LexInvalidCharacter = 1001,
    LexUnterminatedString = 1002,
    LexInvalidNumber = 1003,
    LexInvalidIdentifier = 1004,
    
    // Syntax errors (2000-2999)
    SyntaxUnexpectedToken = 2001,
    SyntaxMissingToken = 2002,
    SyntaxInvalidExpression = 2003,
    SyntaxInvalidDeclaration = 2004,
    
    // Semantic errors (3000-3999)
    SemanticUndeclaredIdentifier = 3001,
    SemanticTypeMismatch = 3002,
    SemanticInvalidOperation = 3003,
    SemanticDuplicateDeclaration = 3004,
    
    // Type system errors (4000-4999)
    TypeInferenceFailure = 4001,
    TypeCircularDependency = 4002,
    TypeInvalidConstraint = 4003,
    TypeAmbiguousReference = 4004,
    
    // Memory/ownership errors (5000-5999)
    OwnershipMoveAfterBorrow = 5001,
    OwnershipDoubleBorrow = 5002,
    OwnershipLifetimeViolation = 5003,
    OwnershipDanglingReference = 5004,
    
    // Quantum-specific errors (6000-6999)
    QuantumInvalidMeasurement = 6001,
    QuantumEntanglementViolation = 6002,
    QuantumSupepositionCollapse = 6003,
    
    // Internal compiler errors (9000-9999)
    InternalCompilerError = 9001,
    InternalMemoryExhaustion = 9002,
    InternalAssertionFailure = 9003
};

/**
 * @class DiagnosticMessage
 * @brief Individual diagnostic message with location and context
 * 
 * DiagnosticMessage represents a single diagnostic with full context
 * including source location, severity, error code, and formatted message.
 * Optimized for memory arena allocation.
 * 
 * @thread_safety Immutable after construction - thread safe
 * @performance Lightweight value type, 64 bytes
 */
class DiagnosticMessage {
public:
    /**
     * @brief Constructs a diagnostic message
     * @param level Severity level
     * @param code Diagnostic error code
     * @param message Primary diagnostic message
     * @param location Source location where diagnostic occurred
     */
    DiagnosticMessage(DiagnosticLevel level, DiagnosticCode code, 
                     String message, SourceLocation location) noexcept
        : level_(level)
        , code_(code)
        , message_(std::move(message))
        , location_(location) {}

    /**
     * @brief Gets the diagnostic level
     * @return Severity level
     */
    [[nodiscard]] auto level() const noexcept -> DiagnosticLevel {
        return level_;
    }

    /**
     * @brief Gets the diagnostic code
     * @return Error code
     */
    [[nodiscard]] auto code() const noexcept -> DiagnosticCode {
        return code_;
    }

    /**
     * @brief Gets the primary message
     * @return Diagnostic message
     */
    [[nodiscard]] auto message() const noexcept -> const String& {
        return message_;
    }

    /**
     * @brief Gets the source location
     * @return Location where diagnostic occurred
     */
    [[nodiscard]] auto location() const noexcept -> const SourceLocation& {
        return location_;
    }

    /**
     * @brief Checks if this is an error-level diagnostic
     * @return True if level is Error or Fatal
     */
    [[nodiscard]] auto is_error() const noexcept -> bool {
        return level_ == DiagnosticLevel::Error || level_ == DiagnosticLevel::Fatal;
    }

    /**
     * @brief Checks if this is a fatal diagnostic
     * @return True if level is Fatal
     */
    [[nodiscard]] auto is_fatal() const noexcept -> bool {
        return level_ == DiagnosticLevel::Fatal;
    }

    /**
     * @brief Gets the numeric error code
     * @return Error code as integer
     */
    [[nodiscard]] auto error_code() const noexcept -> u32 {
        return static_cast<u32>(code_);
    }

private:
    DiagnosticLevel level_;
    DiagnosticCode code_;
    String message_;
    SourceLocation location_;
};

/**
 * @class Diagnostic
 * @brief Complete diagnostic with primary message and optional notes
 * 
 * Diagnostic represents a complete diagnostic report that can include
 * a primary message along with additional context notes, suggestions,
 * and related locations. Designed for arena allocation.
 * 
 * @performance Primary message + up to 8 notes efficiently packed
 * @thread_safety Immutable after construction - thread safe
 */
class Diagnostic {
public:
    /**
     * @brief Constructs a diagnostic with primary message
     * @param primary Primary diagnostic message
     */
    explicit Diagnostic(DiagnosticMessage primary) noexcept
        : primary_(std::move(primary)) {}

    /**
     * @brief Gets the primary diagnostic message
     * @return Primary message
     */
    [[nodiscard]] auto primary() const noexcept -> const DiagnosticMessage& {
        return primary_;
    }

    /**
     * @brief Gets all additional notes
     * @return Vector of note messages
     */
    [[nodiscard]] auto notes() const noexcept -> const Vec<DiagnosticMessage>& {
        return notes_;
    }

    /**
     * @brief Adds a note to this diagnostic
     * @param note Additional note message
     * @return Reference to this diagnostic for chaining
     */
    auto add_note(DiagnosticMessage note) -> Diagnostic& {
        notes_.push_back(std::move(note));
        return *this;
    }

    /**
     * @brief Adds a simple note with location
     * @param message Note message
     * @param location Source location for the note
     * @return Reference to this diagnostic for chaining
     */
    auto add_note(String message, SourceLocation location) -> Diagnostic& {
        return add_note(DiagnosticMessage(
            DiagnosticLevel::Note,
            DiagnosticCode(0), // Notes don't need error codes
            std::move(message),
            location
        ));
    }

    /**
     * @brief Gets the diagnostic level from primary message
     * @return Severity level
     */
    [[nodiscard]] auto level() const noexcept -> DiagnosticLevel {
        return primary_.level();
    }

    /**
     * @brief Gets the diagnostic code from primary message
     * @return Error code
     */
    [[nodiscard]] auto code() const noexcept -> DiagnosticCode {
        return primary_.code();
    }

    /**
     * @brief Checks if this diagnostic represents an error
     * @return True if primary message is error level
     */
    [[nodiscard]] auto is_error() const noexcept -> bool {
        return primary_.is_error();
    }

    /**
     * @brief Checks if this diagnostic is fatal
     * @return True if primary message is fatal
     */
    [[nodiscard]] auto is_fatal() const noexcept -> bool {
        return primary_.is_fatal();
    }

    /**
     * @brief Gets count of all messages (primary + notes)
     * @return Total message count
     */
    [[nodiscard]] auto message_count() const noexcept -> usize {
        return 1 + notes_.size();
    }

private:
    DiagnosticMessage primary_;
    Vec<DiagnosticMessage> notes_;
};

/**
 * @concept DiagnosticFormatter
 * @brief Concept for diagnostic formatting implementations
 */
template<typename T>
concept FormatterConcept = requires(T formatter, const Diagnostic& diag) {
    { formatter.format(diag) } -> std::convertible_to<String>;
    { formatter.supports_color() } -> std::convertible_to<bool>;
};

} // namespace photon::diagnostics