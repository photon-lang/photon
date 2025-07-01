/**
 * @file formatter.hpp
 * @brief Advanced diagnostic formatting with color support and highlighting
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This module provides sophisticated diagnostic formatting capabilities
 * including ANSI color support, source code highlighting, and customizable
 * output styles for superior developer experience.
 */

#pragma once

#include "photon/diagnostics/diagnostic.hpp"
#include "photon/common/types.hpp"
#include <iostream>
#include <string_view>

namespace photon::diagnostics {

/**
 * @enum ColorCode
 * @brief ANSI color codes for terminal output
 */
enum class ColorCode : u8 {
    Reset = 0,
    Bold = 1,
    Dim = 2,
    
    // Foreground colors
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    
    // Bright foreground colors
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97
};

/**
 * @class ColorFormatter
 * @brief ANSI color escape sequence formatter
 * 
 * ColorFormatter provides utilities for generating ANSI escape sequences
 * for colored terminal output with automatic detection of terminal support.
 * 
 * @thread_safety Thread-safe static methods
 * @performance Zero-cost when colors disabled
 */
class ColorFormatter {
public:
    /**
     * @brief Checks if terminal supports color output
     * @return True if colors should be used
     */
    [[nodiscard]] static auto supports_color() noexcept -> bool;

    /**
     * @brief Enables or disables color output
     * @param enable Whether to enable colors
     */
    static auto set_color_enabled(bool enable) noexcept -> void {
        force_color_enabled_ = enable;
        color_detection_override_ = true;
    }

    /**
     * @brief Creates ANSI escape sequence for color
     * @param color Color code
     * @return ANSI escape sequence or empty string if disabled
     */
    [[nodiscard]] static auto escape_sequence(ColorCode color) noexcept -> StringView;

    /**
     * @brief Wraps text with color formatting
     * @param text Text to colorize
     * @param color Color to apply
     * @return Colorized text or original if colors disabled
     */
    [[nodiscard]] static auto colorize(StringView text, ColorCode color) -> String;

    /**
     * @brief Creates bold text
     * @param text Text to make bold
     * @return Bold formatted text
     */
    [[nodiscard]] static auto bold(StringView text) -> String {
        return colorize(text, ColorCode::Bold);
    }

    /**
     * @brief Creates red text (typically for errors)
     * @param text Text to make red
     * @return Red formatted text
     */
    [[nodiscard]] static auto red(StringView text) -> String {
        return colorize(text, ColorCode::Red);
    }

    /**
     * @brief Creates yellow text (typically for warnings)
     * @param text Text to make yellow
     * @return Yellow formatted text
     */
    [[nodiscard]] static auto yellow(StringView text) -> String {
        return colorize(text, ColorCode::Yellow);
    }

    /**
     * @brief Creates blue text (typically for notes)
     * @param text Text to make blue
     * @return Blue formatted text
     */
    [[nodiscard]] static auto blue(StringView text) -> String {
        return colorize(text, ColorCode::Blue);
    }

    /**
     * @brief Creates green text (typically for success)
     * @param text Text to make green
     * @return Green formatted text
     */
    [[nodiscard]] static auto green(StringView text) -> String {
        return colorize(text, ColorCode::Green);
    }

    /**
     * @brief Creates cyan text (typically for locations)
     * @param text Text to make cyan
     * @return Cyan formatted text
     */
    [[nodiscard]] static auto cyan(StringView text) -> String {
        return colorize(text, ColorCode::Cyan);
    }

    /**
     * @brief Creates dim text (typically for secondary info)
     * @param text Text to make dim
     * @return Dim formatted text
     */
    [[nodiscard]] static auto dim(StringView text) -> String {
        return colorize(text, ColorCode::Dim);
    }

private:
    static inline bool force_color_enabled_ = false;
    static inline bool color_detection_override_ = false;
};

/**
 * @class DiagnosticFormatter
 * @brief Comprehensive diagnostic formatting with source code display
 * 
 * DiagnosticFormatter provides sophisticated formatting of diagnostic
 * messages with source code context, highlighting, and customizable
 * output styles similar to modern compilers like Rust or Clang.
 * 
 * @performance Optimized string building with minimal allocations
 * @thread_safety Thread-safe when used with separate instances
 */
class DiagnosticFormatter {
public:
    /**
     * @brief Formatting options for diagnostic output
     */
    struct Options {
        bool show_colors = true;           ///< Enable color output
        bool show_source_context = true;  ///< Show source code lines
        u32 context_lines = 2;            ///< Lines of context around error
        bool show_line_numbers = true;    ///< Show line numbers
        bool show_column_markers = true;  ///< Show column position markers
        bool compact_mode = false;        ///< Compact single-line format
        u32 max_line_length = 120;       ///< Maximum line length before truncation
    };

    /**
     * @brief Constructs formatter with options
     * @param options Formatting configuration
     */
    explicit DiagnosticFormatter(Options options) noexcept;
    DiagnosticFormatter() noexcept;

    /**
     * @brief Formats a single diagnostic message
     * @param diagnostic Diagnostic to format
     * @return Formatted diagnostic string
     */
    [[nodiscard]] auto format(const Diagnostic& diagnostic) const -> String;

    /**
     * @brief Formats a collection of diagnostics
     * @param diagnostics Vector of diagnostics to format
     * @return Formatted output for all diagnostics
     */
    [[nodiscard]] auto format_all(const Vec<Diagnostic>& diagnostics) const -> String;

    /**
     * @brief Formats diagnostic summary statistics
     * @param error_count Number of errors
     * @param warning_count Number of warnings
     * @param note_count Number of notes
     * @return Formatted summary string
     */
    [[nodiscard]] auto format_summary(usize error_count, usize warning_count, usize note_count) const -> String;

    /**
     * @brief Gets current formatting options
     * @return Current options
     */
    [[nodiscard]] auto options() const noexcept -> const Options& {
        return options_;
    }

    /**
     * @brief Updates formatting options
     * @param options New formatting options
     */
    auto set_options(Options options) noexcept -> void {
        options_ = options;
    }

    /**
     * @brief Checks if formatter supports color output
     * @return True if colors will be used
     */
    [[nodiscard]] auto supports_color() const noexcept -> bool {
        return options_.show_colors && ColorFormatter::supports_color();
    }

private:
    Options options_;

    [[nodiscard]] auto format_diagnostic_header(const DiagnosticMessage& message) const -> String;
    [[nodiscard]] auto format_source_context(const SourceLocation& location) const -> String;
    [[nodiscard]] auto format_column_marker(u32 column, u32 length = 1) const -> String;
    [[nodiscard]] auto get_level_color(DiagnosticLevel level) const -> ColorCode;
    [[nodiscard]] auto get_level_prefix(DiagnosticLevel level) const -> StringView;
    [[nodiscard]] auto truncate_line(StringView line) const -> String;
};

/**
 * @class SourceCodeReader
 * @brief Utility for reading source code lines for diagnostic display
 * 
 * SourceCodeReader provides efficient access to source file lines
 * for displaying context in diagnostic messages.
 * 
 * @performance Caches file content for repeated access
 * @thread_safety Not thread-safe, use separate instances per thread
 */
class SourceCodeReader {
public:
    /**
     * @brief Reads a specific line from a source file
     * @param filename Source file path
     * @param line_number Line number (1-based)
     * @return Source line or empty if not found
     */
    [[nodiscard]] auto read_line(StringView filename, u32 line_number) const -> Opt<String>;

    /**
     * @brief Reads multiple lines from a source file
     * @param filename Source file path
     * @param start_line Starting line number (1-based, inclusive)
     * @param end_line Ending line number (1-based, inclusive)
     * @return Vector of source lines
     */
    [[nodiscard]] auto read_lines(StringView filename, u32 start_line, u32 end_line) const -> Vec<String>;

    /**
     * @brief Checks if a source file exists and is readable
     * @param filename Source file path
     * @return True if file can be read
     */
    [[nodiscard]] auto file_exists(StringView filename) const noexcept -> bool;

    /**
     * @brief Clears cached file content
     */
    auto clear_cache() -> void {
        file_cache_.clear();
    }

private:
    mutable Map<String, Vec<String>> file_cache_;

    [[nodiscard]] auto read_file_lines(StringView filename) const -> Opt<Vec<String>>;
};

/**
 * @class StreamFormatter
 * @brief Stream-oriented diagnostic formatter for real-time output
 * 
 * StreamFormatter provides efficient streaming output of diagnostics
 * suitable for compiler drivers and interactive tools.
 */
class StreamFormatter {
public:
    /**
     * @brief Constructs stream formatter
     * @param output Target output stream
     * @param formatter Diagnostic formatter to use
     */
    StreamFormatter(std::ostream& output, DiagnosticFormatter formatter = DiagnosticFormatter{}) noexcept
        : output_(output), formatter_(std::move(formatter)) {}

    /**
     * @brief Writes a diagnostic to the stream
     * @param diagnostic Diagnostic to write
     * @return Reference to this formatter for chaining
     */
    auto write(const Diagnostic& diagnostic) -> StreamFormatter& {
        output_ << formatter_.format(diagnostic) << '\n';
        return *this;
    }

    /**
     * @brief Writes diagnostic summary to the stream
     * @param error_count Number of errors
     * @param warning_count Number of warnings
     * @param note_count Number of notes
     * @return Reference to this formatter for chaining
     */
    auto write_summary(usize error_count, usize warning_count, usize note_count) -> StreamFormatter& {
        output_ << formatter_.format_summary(error_count, warning_count, note_count) << '\n';
        return *this;
    }

    /**
     * @brief Flushes the output stream
     * @return Reference to this formatter for chaining
     */
    auto flush() -> StreamFormatter& {
        output_.flush();
        return *this;
    }

private:
    std::ostream& output_;
    DiagnosticFormatter formatter_;
};

} // namespace photon::diagnostics