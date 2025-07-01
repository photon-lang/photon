/**
 * @file source_location.cpp
 * @brief Implementation of source location tracking functionality
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/diagnostics/source_location.hpp"
#include <sstream>

namespace photon::diagnostics {

auto SourceLocation::to_string() const -> String {
    if (!is_valid()) {
        return "<invalid location>";
    }
    
    std::ostringstream oss;
    oss << filename_ << ":" << line_number_ << ":" << column_number_;
    return oss.str();
}

auto SourceLocation::to_string(bool include_offset) const -> String {
    if (!is_valid()) {
        return "<invalid location>";
    }
    
    std::ostringstream oss;
    oss << filename_ << ":" << line_number_ << ":" << column_number_;
    
    if (include_offset && byte_offset_ > 0) {
        oss << " (offset " << byte_offset_ << ")";
    }
    
    return oss.str();
}

auto SourceRange::to_string() const -> String {
    if (!is_valid()) {
        return "<invalid range>";
    }
    
    if (is_point()) {
        return start_.to_string();
    }
    
    if (start_.filename() != end_.filename()) {
        return start_.to_string() + " to " + end_.to_string();
    }
    
    std::ostringstream oss;
    oss << start_.filename() << ":" << start_.line() << ":" << start_.column();
    
    if (start_.line() == end_.line()) {
        // Same line, show column range
        oss << "-" << end_.column();
    } else {
        // Multiple lines
        oss << "-" << end_.line() << ":" << end_.column();
    }
    
    return oss.str();
}

auto SourceRange::contains(const SourceLocation& location) const noexcept -> bool {
    if (!is_valid() || !location.is_valid()) {
        return false;
    }
    
    // Must be same file
    if (location.filename() != filename()) {
        return false;
    }
    
    // Check if location is within range using byte offsets for precision
    return location.offset() >= start_.offset() && location.offset() < end_.offset();
}

auto SourceRange::overlaps(const SourceRange& other) const noexcept -> bool {
    if (!is_valid() || !other.is_valid()) {
        return false;
    }
    
    // Must be same file
    if (filename() != other.filename()) {
        return false;
    }
    
    // Check for overlap using byte offsets
    return start_.offset() < other.end_.offset() && end_.offset() > other.start_.offset();
}

auto SourceRange::merge(const SourceRange& other) const noexcept -> SourceRange {
    if (!is_valid()) {
        return other;
    }
    
    if (!other.is_valid()) {
        return *this;
    }
    
    // Must be same file
    if (filename() != other.filename()) {
        return *this; // Return original range if files don't match
    }
    
    // Find the earliest start and latest end
    const auto& earliest_start = (start_.offset() <= other.start_.offset()) ? start_ : other.start_;
    const auto& latest_end = (end_.offset() >= other.end_.offset()) ? end_ : other.end_;
    
    return SourceRange(earliest_start, latest_end);
}

} // namespace photon::diagnostics