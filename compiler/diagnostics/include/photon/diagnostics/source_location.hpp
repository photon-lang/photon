/**
 * @file source_location.hpp
 * @brief Source location tracking for compiler diagnostics
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This module provides precise source location tracking with line/column
 * information for superior error reporting and debugging capabilities.
 */

#pragma once

#include "photon/common/types.hpp"
#include <compare>

namespace photon::diagnostics {

/**
 * @class SourceLocation
 * @brief Represents a specific location in source code
 * 
 * SourceLocation provides precise tracking of positions within source files
 * including filename, line number, column number, and byte offset. Optimized
 * for memory efficiency and fast comparison operations.
 * 
 * @invariant line_number >= 1 (1-based indexing)
 * @invariant column_number >= 1 (1-based indexing)
 * @invariant byte_offset is valid offset into source buffer
 * 
 * @performance Size: 32 bytes (optimized for cache efficiency)
 * @thread_safety Immutable after construction - thread safe
 */
class SourceLocation {
public:
    /**
     * @brief Constructs an invalid/unknown source location
     * @post is_valid() returns false
     */
    SourceLocation() noexcept = default;

    /**
     * @brief Constructs a source location with full information
     * @param filename Name of the source file
     * @param line_number Line number (1-based)
     * @param column_number Column number (1-based)
     * @param byte_offset Byte offset from start of file
     * 
     * @pre line_number >= 1
     * @pre column_number >= 1
     * @post is_valid() returns true
     */
    SourceLocation(StringView filename, u32 line_number, u32 column_number, u32 byte_offset) noexcept
        : filename_(filename)
        , line_number_(line_number)
        , column_number_(column_number)
        , byte_offset_(byte_offset) {}

    /**
     * @brief Constructs a source location with line/column only
     * @param filename Name of the source file
     * @param line_number Line number (1-based)
     * @param column_number Column number (1-based)
     */
    SourceLocation(StringView filename, u32 line_number, u32 column_number) noexcept
        : SourceLocation(filename, line_number, column_number, 0) {}

    // Comparison operators (for sorting diagnostics)
    [[nodiscard]] auto operator<=>(const SourceLocation& other) const noexcept = default;
    [[nodiscard]] auto operator==(const SourceLocation& other) const noexcept -> bool = default;

    /**
     * @brief Checks if this location is valid
     * @return True if location has valid line/column information
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return line_number_ > 0 && column_number_ > 0;
    }

    /**
     * @brief Gets the filename
     * @return Filename as string view
     */
    [[nodiscard]] auto filename() const noexcept -> StringView {
        return filename_;
    }

    /**
     * @brief Gets the line number (1-based)
     * @return Line number, 0 if invalid
     */
    [[nodiscard]] auto line() const noexcept -> u32 {
        return line_number_;
    }

    /**
     * @brief Gets the column number (1-based)
     * @return Column number, 0 if invalid
     */
    [[nodiscard]] auto column() const noexcept -> u32 {
        return column_number_;
    }

    /**
     * @brief Gets the byte offset from start of file
     * @return Byte offset
     */
    [[nodiscard]] auto offset() const noexcept -> u32 {
        return byte_offset_;
    }

    /**
     * @brief Creates a new location offset by given amount
     * @param line_offset Number of lines to advance
     * @param column_offset Number of columns to advance
     * @param byte_offset_delta Byte offset delta
     * @return New source location
     */
    [[nodiscard]] auto advance(u32 line_offset, u32 column_offset, u32 byte_offset_delta) const noexcept -> SourceLocation {
        return SourceLocation(
            filename_,
            line_number_ + line_offset,
            column_number_ + column_offset,
            byte_offset_ + byte_offset_delta
        );
    }

    /**
     * @brief Formats location as string for display
     * @return Formatted string like "filename:line:column"
     */
    [[nodiscard]] auto to_string() const -> String;

    /**
     * @brief Formats location with optional byte offset
     * @param include_offset Whether to include byte offset
     * @return Formatted string
     */
    [[nodiscard]] auto to_string(bool include_offset) const -> String;

private:
    StringView filename_;
    u32 line_number_ = 0;
    u32 column_number_ = 0;
    u32 byte_offset_ = 0;
};

/**
 * @class SourceRange
 * @brief Represents a range of source code between two locations
 * 
 * SourceRange efficiently represents spans of source code for highlighting
 * errors and warnings with precise start/end boundaries.
 * 
 * @invariant start <= end (in source order)
 * @invariant start and end refer to same file
 */
class SourceRange {
public:
    /**
     * @brief Constructs an invalid range
     */
    SourceRange() noexcept = default;

    /**
     * @brief Constructs a range from start to end locations
     * @param start Starting location (inclusive)
     * @param end Ending location (exclusive)
     * 
     * @pre start.filename() == end.filename()
     * @pre start <= end
     */
    SourceRange(SourceLocation start, SourceLocation end) noexcept
        : start_(start), end_(end) {}

    /**
     * @brief Constructs a single-point range
     * @param location Single location
     */
    explicit SourceRange(SourceLocation location) noexcept
        : start_(location), end_(location) {}

    /**
     * @brief Gets the start location
     * @return Start location (inclusive)
     */
    [[nodiscard]] auto start() const noexcept -> const SourceLocation& {
        return start_;
    }

    /**
     * @brief Gets the end location
     * @return End location (exclusive)
     */
    [[nodiscard]] auto end() const noexcept -> const SourceLocation& {
        return end_;
    }

    /**
     * @brief Checks if range is valid
     * @return True if both start and end are valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return start_.is_valid() && end_.is_valid();
    }

    /**
     * @brief Checks if range is single point
     * @return True if start == end
     */
    [[nodiscard]] auto is_point() const noexcept -> bool {
        return start_ == end_;
    }

    /**
     * @brief Gets the filename for this range
     * @return Filename from start location
     */
    [[nodiscard]] auto filename() const noexcept -> StringView {
        return start_.filename();
    }

    /**
     * @brief Calculates byte length of the range
     * @return Number of bytes spanned
     */
    [[nodiscard]] auto byte_length() const noexcept -> u32 {
        return end_.offset() - start_.offset();
    }

    /**
     * @brief Formats range as string for display
     * @return Formatted string like "filename:start_line:start_col-end_line:end_col"
     */
    [[nodiscard]] auto to_string() const -> String;

    /**
     * @brief Checks if this range contains a location
     * @param location Location to test
     * @return True if location is within this range
     */
    [[nodiscard]] auto contains(const SourceLocation& location) const noexcept -> bool;

    /**
     * @brief Checks if this range overlaps with another
     * @param other Other range to test
     * @return True if ranges overlap
     */
    [[nodiscard]] auto overlaps(const SourceRange& other) const noexcept -> bool;

    /**
     * @brief Merges this range with another
     * @param other Other range to merge with
     * @return New range spanning both ranges
     * @pre Both ranges refer to same file
     */
    [[nodiscard]] auto merge(const SourceRange& other) const noexcept -> SourceRange;

private:
    SourceLocation start_;
    SourceLocation end_;
};

} // namespace photon::diagnostics