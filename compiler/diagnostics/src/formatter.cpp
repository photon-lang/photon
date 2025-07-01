/**
 * @file formatter.cpp
 * @brief Implementation of diagnostic formatting with color support
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/diagnostics/formatter.hpp"
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>

namespace photon::diagnostics {

// DiagnosticFormatter implementation
DiagnosticFormatter::DiagnosticFormatter(Options options) noexcept
    : options_(options) {}

DiagnosticFormatter::DiagnosticFormatter() noexcept
    : options_{} {}

// ColorFormatter implementation
auto ColorFormatter::supports_color() noexcept -> bool {
    if (color_detection_override_) {
        return force_color_enabled_;
    }
    
    // Check if we're outputting to a terminal
    if (!isatty(STDOUT_FILENO)) {
        return false;
    }
    
    // Check environment variables
    const char* term = std::getenv("TERM");
    if (!term) {
        return false;
    }
    
    String term_str(term);
    if (term_str == "dumb") {
        return false;
    }
    
    // Check for explicit color support
    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm) {
        return true;
    }
    
    // Common terminals that support color
    return term_str.find("color") != String::npos ||
           term_str.find("xterm") != String::npos ||
           term_str.find("screen") != String::npos ||
           term_str == "ansi";
}

auto ColorFormatter::escape_sequence(ColorCode color) noexcept -> StringView {
    if (!supports_color()) {
        return "";
    }
    
    switch (color) {
        case ColorCode::Reset: return "\033[0m";
        case ColorCode::Bold: return "\033[1m";
        case ColorCode::Dim: return "\033[2m";
        case ColorCode::Black: return "\033[30m";
        case ColorCode::Red: return "\033[31m";
        case ColorCode::Green: return "\033[32m";
        case ColorCode::Yellow: return "\033[33m";
        case ColorCode::Blue: return "\033[34m";
        case ColorCode::Magenta: return "\033[35m";
        case ColorCode::Cyan: return "\033[36m";
        case ColorCode::White: return "\033[37m";
        case ColorCode::BrightBlack: return "\033[90m";
        case ColorCode::BrightRed: return "\033[91m";
        case ColorCode::BrightGreen: return "\033[92m";
        case ColorCode::BrightYellow: return "\033[93m";
        case ColorCode::BrightBlue: return "\033[94m";
        case ColorCode::BrightMagenta: return "\033[95m";
        case ColorCode::BrightCyan: return "\033[96m";
        case ColorCode::BrightWhite: return "\033[97m";
    }
    return "";
}

auto ColorFormatter::colorize(StringView text, ColorCode color) -> String {
    if (!supports_color()) {
        return String(text);
    }
    
    String result;
    result.reserve(text.size() + 20); // Reserve space for escape sequences
    result += escape_sequence(color);
    result += text;
    result += escape_sequence(ColorCode::Reset);
    return result;
}

auto DiagnosticFormatter::format(const Diagnostic& diagnostic) const -> String {
    std::ostringstream output;
    
    // Format primary diagnostic message
    output << format_diagnostic_header(diagnostic.primary());
    
    // Add source context if enabled
    if (options_.show_source_context) {
        auto context = format_source_context(diagnostic.primary().location());
        if (!context.empty()) {
            output << "\n" << context;
        }
    }
    
    // Format additional notes
    for (const auto& note : diagnostic.notes()) {
        output << "\n" << format_diagnostic_header(note);
        
        if (options_.show_source_context && note.location().is_valid()) {
            auto context = format_source_context(note.location());
            if (!context.empty()) {
                output << "\n" << context;
            }
        }
    }
    
    return output.str();
}

auto DiagnosticFormatter::format_all(const Vec<Diagnostic>& diagnostics) const -> String {
    std::ostringstream output;
    
    for (usize i = 0; i < diagnostics.size(); ++i) {
        if (i > 0) {
            output << "\n\n";
        }
        output << format(diagnostics[i]);
    }
    
    return output.str();
}

auto DiagnosticFormatter::format_summary(usize error_count, usize warning_count, usize note_count) const -> String {
    std::ostringstream output;
    
    if (error_count == 0 && warning_count == 0) {
        output << ColorFormatter::green("compilation completed successfully");
        if (note_count > 0) {
            output << " (" << note_count << " note" << (note_count != 1 ? "s" : "") << ")";
        }
        return output.str();
    }
    
    Vec<String> parts;
    
    if (error_count > 0) {
        String error_str = std::to_string(error_count) + " error" + (error_count != 1 ? "s" : "");
        parts.push_back(supports_color() ? ColorFormatter::red(error_str) : error_str);
    }
    
    if (warning_count > 0) {
        String warning_str = std::to_string(warning_count) + " warning" + (warning_count != 1 ? "s" : "");
        parts.push_back(supports_color() ? ColorFormatter::yellow(warning_str) : warning_str);
    }
    
    if (note_count > 0) {
        String note_str = std::to_string(note_count) + " note" + (note_count != 1 ? "s" : "");
        parts.push_back(supports_color() ? ColorFormatter::blue(note_str) : note_str);
    }
    
    // Join parts with commas
    for (usize i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            output << ", ";
        }
        output << parts[i];
    }
    
    output << " generated";
    
    return output.str();
}

auto DiagnosticFormatter::format_diagnostic_header(const DiagnosticMessage& message) const -> String {
    std::ostringstream output;
    
    if (options_.compact_mode) {
        // Compact format: "file:line:col: level: message"
        output << message.location().to_string() << ": ";
        
        String level_str = String(get_level_prefix(message.level()));
        if (supports_color()) {
            level_str = ColorFormatter::colorize(level_str, get_level_color(message.level()));
        }
        output << level_str << ": " << message.message();
        
        return output.str();
    }
    
    // Full format with colors and styling
    String level_prefix = String(get_level_prefix(message.level()));
    if (supports_color()) {
        level_prefix = ColorFormatter::colorize(level_prefix, get_level_color(message.level()));
        level_prefix = ColorFormatter::bold(level_prefix);
    }
    
    output << level_prefix << ": " << message.message();
    
    // Add location if valid
    if (message.location().is_valid()) {
        output << "\n";
        String location_str = "  --> " + message.location().to_string();
        if (supports_color()) {
            location_str = ColorFormatter::cyan(location_str);
        }
        output << location_str;
    }
    
    // Add error code if present
    if (message.error_code() != 0) {
        String code_str = " [E" + std::to_string(message.error_code()) + "]";
        if (supports_color()) {
            code_str = ColorFormatter::dim(code_str);
        }
        output << code_str;
    }
    
    return output.str();
}

auto DiagnosticFormatter::format_source_context(const SourceLocation& location) const -> String {
    if (!location.is_valid() || !options_.show_source_context) {
        return "";
    }
    
    SourceCodeReader reader;
    if (!reader.file_exists(location.filename())) {
        return "";
    }
    
    std::ostringstream output;
    
    // Calculate line range for context
    u32 start_line = (location.line() > options_.context_lines) 
                     ? location.line() - options_.context_lines 
                     : 1;
    u32 end_line = location.line() + options_.context_lines;
    
    auto lines = reader.read_lines(location.filename(), start_line, end_line);
    if (lines.empty()) {
        return "";
    }
    
    // Calculate line number width for alignment
    u32 line_number_width = static_cast<u32>(std::to_string(end_line).length());
    
    for (u32 i = 0; i < lines.size(); ++i) {
        u32 current_line = start_line + i;
        const auto& line_content = lines[i];
        
        // Format line number
        String line_num_str = std::to_string(current_line);
        while (line_num_str.length() < line_number_width) {
            line_num_str = " " + line_num_str;
        }
        
        if (options_.show_line_numbers) {
            if (supports_color()) {
                line_num_str = ColorFormatter::dim(line_num_str);
            }
            output << " " << line_num_str << " | ";
        }
        
        // Truncate long lines
        String display_line = truncate_line(line_content);
        output << display_line << "\n";
        
        // Add column marker for the error line
        if (current_line == location.line() && options_.show_column_markers) {
            if (options_.show_line_numbers) {
                String spaces(line_number_width + 3, ' ');
                if (supports_color()) {
                    spaces = ColorFormatter::dim(spaces);
                }
                output << spaces << "| ";
            }
            
            output << format_column_marker(location.column()) << "\n";
        }
    }
    
    return output.str();
}

auto DiagnosticFormatter::format_column_marker(u32 column, u32 length) const -> String {
    if (column == 0) {
        return "";
    }
    
    std::ostringstream output;
    
    // Add spaces before the marker
    for (u32 i = 1; i < column; ++i) {
        output << " ";
    }
    
    // Add the marker (caret for single character, underline for ranges)
    String marker = (length == 1) ? "^" : String(length, '~');
    if (supports_color()) {
        marker = ColorFormatter::colorize(marker, ColorCode::BrightRed);
    }
    output << marker;
    
    return output.str();
}

auto DiagnosticFormatter::get_level_color(DiagnosticLevel level) const -> ColorCode {
    switch (level) {
        case DiagnosticLevel::Fatal:
        case DiagnosticLevel::Error:
            return ColorCode::BrightRed;
        case DiagnosticLevel::Warning:
            return ColorCode::BrightYellow;
        case DiagnosticLevel::Note:
            return ColorCode::BrightBlue;
    }
    return ColorCode::Reset;
}

auto DiagnosticFormatter::get_level_prefix(DiagnosticLevel level) const -> StringView {
    switch (level) {
        case DiagnosticLevel::Fatal:
            return "fatal error";
        case DiagnosticLevel::Error:
            return "error";
        case DiagnosticLevel::Warning:
            return "warning";
        case DiagnosticLevel::Note:
            return "note";
    }
    return "unknown";
}

auto DiagnosticFormatter::truncate_line(StringView line) const -> String {
    if (line.length() <= options_.max_line_length) {
        return String(line);
    }
    
    String result(line.substr(0, options_.max_line_length - 3));
    result += "...";
    return result;
}

auto SourceCodeReader::read_line(StringView filename, u32 line_number) const -> Opt<String> {
    auto lines_opt = read_file_lines(filename);
    if (!lines_opt || line_number == 0 || line_number > lines_opt->size()) {
        return std::nullopt;
    }
    
    return lines_opt->at(line_number - 1);
}

auto SourceCodeReader::read_lines(StringView filename, u32 start_line, u32 end_line) const -> Vec<String> {
    auto lines_opt = read_file_lines(filename);
    if (!lines_opt || start_line == 0 || start_line > end_line) {
        return {};
    }
    
    const auto& all_lines = *lines_opt;
    Vec<String> result;
    
    u32 actual_start = std::max(1u, start_line) - 1; // Convert to 0-based
    u32 actual_end = std::min(static_cast<u32>(all_lines.size()), end_line);
    
    for (u32 i = actual_start; i < actual_end; ++i) {
        result.push_back(all_lines[i]);
    }
    
    return result;
}

auto SourceCodeReader::file_exists(StringView filename) const noexcept -> bool {
    String filename_str(filename);
    std::ifstream file(filename_str);
    return file.good();
}

auto SourceCodeReader::read_file_lines(StringView filename) const -> Opt<Vec<String>> {
    String filename_str(filename);
    
    // Check cache first
    auto cache_it = file_cache_.find(filename_str);
    if (cache_it != file_cache_.end()) {
        return cache_it->second;
    }
    
    // Read file
    std::ifstream file(filename_str);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    Vec<String> lines;
    String line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    // Cache the result
    file_cache_[filename_str] = lines;
    return lines;
}

} // namespace photon::diagnostics