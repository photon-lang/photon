/**
 * @file formatter_test.cpp
 * @brief Comprehensive test suite for diagnostic formatting system
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/diagnostics/formatter.hpp"
#include <gtest/gtest.h>
#include <sstream>

using namespace photon::diagnostics;

namespace {

class FormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        location_ = SourceLocation("test.pht", 10, 5, 100);
        error_message_ = DiagnosticMessage(
            DiagnosticLevel::Error,
            DiagnosticCode::SyntaxUnexpectedToken,
            "unexpected token ';'",
            location_
        );
        warning_message_ = DiagnosticMessage(
            DiagnosticLevel::Warning,
            DiagnosticCode::SyntaxUnexpectedToken,
            "unused variable 'x'",
            location_
        );
        note_message_ = DiagnosticMessage(
            DiagnosticLevel::Note,
            DiagnosticCode(0),
            "consider using '_' prefix for unused variables",
            location_
        );
        
        // Disable colors for consistent testing
        ColorFormatter::set_color_enabled(false);
    }
    
    void TearDown() override {
        // Reset color detection to automatic
        ColorFormatter::set_color_enabled(false);
    }
    
    SourceLocation location_;
    DiagnosticMessage error_message_;
    DiagnosticMessage warning_message_;
    DiagnosticMessage note_message_;
};

} // anonymous namespace

// ColorFormatter tests

TEST_F(FormatterTest, ColorFormatterDisabledReturnsEmptySequences) {
    ColorFormatter::set_color_enabled(false);
    
    EXPECT_EQ(ColorFormatter::escape_sequence(ColorCode::Red), "");
    EXPECT_EQ(ColorFormatter::escape_sequence(ColorCode::Bold), "");
    EXPECT_EQ(ColorFormatter::colorize("test", ColorCode::Red), "test");
}

TEST_F(FormatterTest, ColorFormatterEnabledReturnsSequences) {
    ColorFormatter::set_color_enabled(true);
    
    EXPECT_EQ(ColorFormatter::escape_sequence(ColorCode::Red), "\033[31m");
    EXPECT_EQ(ColorFormatter::escape_sequence(ColorCode::Bold), "\033[1m");
    EXPECT_EQ(ColorFormatter::escape_sequence(ColorCode::Reset), "\033[0m");
    
    auto colorized = ColorFormatter::colorize("test", ColorCode::Red);
    EXPECT_EQ(colorized, "\033[31mtest\033[0m");
}

TEST_F(FormatterTest, ColorFormatterHelperMethods) {
    ColorFormatter::set_color_enabled(true);
    
    EXPECT_EQ(ColorFormatter::red("error"), "\033[31merror\033[0m");
    EXPECT_EQ(ColorFormatter::yellow("warning"), "\033[33mwarning\033[0m");
    EXPECT_EQ(ColorFormatter::blue("note"), "\033[34mnote\033[0m");
    EXPECT_EQ(ColorFormatter::cyan("location"), "\033[36mlocation\033[0m");
    EXPECT_EQ(ColorFormatter::bold("bold"), "\033[1mbold\033[0m");
    EXPECT_EQ(ColorFormatter::dim("dim"), "\033[2mdim\033[0m");
}

// DiagnosticFormatter tests

TEST_F(FormatterTest, DiagnosticFormatterDefaultOptions) {
    DiagnosticFormatter formatter;
    const auto& options = formatter.options();
    
    EXPECT_TRUE(options.show_colors);
    EXPECT_TRUE(options.show_source_context);
    EXPECT_EQ(options.context_lines, 2u);
    EXPECT_TRUE(options.show_line_numbers);
    EXPECT_TRUE(options.show_column_markers);
    EXPECT_FALSE(options.compact_mode);
    EXPECT_EQ(options.max_line_length, 120u);
}

TEST_F(FormatterTest, DiagnosticFormatterCustomOptions) {
    DiagnosticFormatter::Options custom_options;
    custom_options.show_colors = false;
    custom_options.compact_mode = true;
    custom_options.context_lines = 1;
    
    DiagnosticFormatter formatter(custom_options);
    const auto& options = formatter.options();
    
    EXPECT_FALSE(options.show_colors);
    EXPECT_TRUE(options.compact_mode);
    EXPECT_EQ(options.context_lines, 1u);
}

TEST_F(FormatterTest, DiagnosticFormatterSupportsColorDetection) {
    DiagnosticFormatter::Options options;
    options.show_colors = true;
    DiagnosticFormatter formatter(options);
    
    // Should return false when colors are forcibly disabled
    ColorFormatter::set_color_enabled(false);
    EXPECT_FALSE(formatter.supports_color());
    
    ColorFormatter::set_color_enabled(true);
    EXPECT_TRUE(formatter.supports_color());
}

TEST_F(FormatterTest, FormatSimpleErrorDiagnostic) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    Diagnostic diagnostic(error_message_);
    String result = formatter.format(diagnostic);
    
    EXPECT_NE(result.find("error"), String::npos);
    EXPECT_NE(result.find("unexpected token ';'"), String::npos);
    EXPECT_NE(result.find("test.pht:10:5"), String::npos);
    EXPECT_NE(result.find("[E2001]"), String::npos); // Error code
}

TEST_F(FormatterTest, FormatWarningDiagnostic) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    Diagnostic diagnostic(warning_message_);
    String result = formatter.format(diagnostic);
    
    EXPECT_NE(result.find("warning"), String::npos);
    EXPECT_NE(result.find("unused variable 'x'"), String::npos);
}

TEST_F(FormatterTest, FormatNoteDiagnostic) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    Diagnostic diagnostic(note_message_);
    String result = formatter.format(diagnostic);
    
    EXPECT_NE(result.find("note"), String::npos);
    EXPECT_NE(result.find("consider using '_' prefix"), String::npos);
}

TEST_F(FormatterTest, FormatDiagnosticWithNotes) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    Diagnostic diagnostic(error_message_);
    diagnostic.add_note("first note", location_);
    diagnostic.add_note("second note", location_);
    
    String result = formatter.format(diagnostic);
    
    EXPECT_NE(result.find("unexpected token ';'"), String::npos);
    EXPECT_NE(result.find("first note"), String::npos);
    EXPECT_NE(result.find("second note"), String::npos);
}

TEST_F(FormatterTest, FormatCompactMode) {
    DiagnosticFormatter::Options options;
    options.compact_mode = true;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    Diagnostic diagnostic(error_message_);
    String result = formatter.format(diagnostic);
    
    EXPECT_NE(result.find("test.pht:10:5: error: unexpected token ';'"), String::npos);
    // Should not contain multi-line formatting
    EXPECT_EQ(result.find("-->"), String::npos);
}

TEST_F(FormatterTest, FormatAllDiagnostics) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    Vec<Diagnostic> diagnostics;
    diagnostics.emplace_back(error_message_);
    diagnostics.emplace_back(warning_message_);
    
    String result = formatter.format_all(diagnostics);
    
    EXPECT_NE(result.find("unexpected token ';'"), String::npos);
    EXPECT_NE(result.find("unused variable 'x'"), String::npos);
    // Should contain double newline separator
    EXPECT_NE(result.find("\n\n"), String::npos);
}

TEST_F(FormatterTest, FormatSummaryNoIssues) {
    DiagnosticFormatter formatter;
    String result = formatter.format_summary(0, 0, 5);
    
    EXPECT_NE(result.find("compilation completed successfully"), String::npos);
    EXPECT_NE(result.find("5 notes"), String::npos);
}

TEST_F(FormatterTest, FormatSummaryWithIssues) {
    DiagnosticFormatter::Options options;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    String result = formatter.format_summary(2, 3, 1);
    
    EXPECT_NE(result.find("2 errors"), String::npos);
    EXPECT_NE(result.find("3 warnings"), String::npos);
    EXPECT_NE(result.find("1 note"), String::npos);
    EXPECT_NE(result.find("generated"), String::npos);
}

TEST_F(FormatterTest, FormatSummarySingularPlural) {
    DiagnosticFormatter::Options options;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    String result_singular = formatter.format_summary(1, 1, 1);
    EXPECT_NE(result_singular.find("1 error"), String::npos);
    EXPECT_NE(result_singular.find("1 warning"), String::npos);
    EXPECT_NE(result_singular.find("1 note"), String::npos);
    // Should not contain plurals
    EXPECT_EQ(result_singular.find("errors"), String::npos);
    
    String result_plural = formatter.format_summary(2, 2, 2);
    EXPECT_NE(result_plural.find("2 errors"), String::npos);
    EXPECT_NE(result_plural.find("2 warnings"), String::npos);
    EXPECT_NE(result_plural.find("2 notes"), String::npos);
}

// SourceCodeReader tests

TEST_F(FormatterTest, SourceCodeReaderNonExistentFile) {
    SourceCodeReader reader;
    
    EXPECT_FALSE(reader.file_exists("nonexistent.pht"));
    EXPECT_FALSE(reader.read_line("nonexistent.pht", 1).has_value());
    EXPECT_TRUE(reader.read_lines("nonexistent.pht", 1, 5).empty());
}

TEST_F(FormatterTest, SourceCodeReaderInvalidLineNumbers) {
    SourceCodeReader reader;
    
    // Create a temporary test file
    String test_filename = "/tmp/test_source.pht";
    std::ofstream test_file(test_filename);
    test_file << "line 1\nline 2\nline 3\n";
    test_file.close();
    
    EXPECT_TRUE(reader.file_exists(test_filename));
    
    // Test invalid line numbers
    EXPECT_FALSE(reader.read_line(test_filename, 0).has_value());
    EXPECT_FALSE(reader.read_line(test_filename, 10).has_value());
    
    // Test valid line numbers
    auto line1 = reader.read_line(test_filename, 1);
    ASSERT_TRUE(line1.has_value());
    EXPECT_EQ(*line1, "line 1");
    
    auto line3 = reader.read_line(test_filename, 3);
    ASSERT_TRUE(line3.has_value());
    EXPECT_EQ(*line3, "line 3");
    
    // Cleanup
    std::remove(test_filename.c_str());
}

TEST_F(FormatterTest, SourceCodeReaderMultipleLines) {
    SourceCodeReader reader;
    
    // Create a temporary test file
    String test_filename = "/tmp/test_multiline.pht";
    std::ofstream test_file(test_filename);
    test_file << "fn main() {\n    let x = 42;\n    emit(x);\n}\n";
    test_file.close();
    
    auto lines = reader.read_lines(test_filename, 1, 3);
    EXPECT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "fn main() {");
    EXPECT_EQ(lines[1], "    let x = 42;");
    EXPECT_EQ(lines[2], "    emit(x);");
    
    // Test out of range
    auto empty_lines = reader.read_lines(test_filename, 10, 15);
    EXPECT_TRUE(empty_lines.empty());
    
    // Cleanup
    std::remove(test_filename.c_str());
}

TEST_F(FormatterTest, SourceCodeReaderCaching) {
    SourceCodeReader reader;
    
    // Create a temporary test file
    String test_filename = "/tmp/test_caching.pht";
    std::ofstream test_file(test_filename);
    test_file << "cached line\n";
    test_file.close();
    
    // First read
    auto line1 = reader.read_line(test_filename, 1);
    ASSERT_TRUE(line1.has_value());
    
    // Modify file
    std::ofstream test_file2(test_filename);
    test_file2 << "modified line\n";
    test_file2.close();
    
    // Second read should return cached result
    auto line2 = reader.read_line(test_filename, 1);
    ASSERT_TRUE(line2.has_value());
    EXPECT_EQ(*line2, "cached line"); // Should be cached, not modified
    
    // Clear cache and read again
    reader.clear_cache();
    auto line3 = reader.read_line(test_filename, 1);
    ASSERT_TRUE(line3.has_value());
    EXPECT_EQ(*line3, "modified line"); // Should read new content
    
    // Cleanup
    std::remove(test_filename.c_str());
}

// StreamFormatter tests

TEST_F(FormatterTest, StreamFormatterBasicOutput) {
    std::ostringstream output;
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    
    StreamFormatter stream_formatter(output, DiagnosticFormatter(options));
    
    Diagnostic diagnostic(error_message_);
    stream_formatter.write(diagnostic);
    
    String result = output.str();
    EXPECT_NE(result.find("error"), String::npos);
    EXPECT_NE(result.find("unexpected token ';'"), String::npos);
    EXPECT_NE(result.find("\n"), String::npos); // Should end with newline
}

TEST_F(FormatterTest, StreamFormatterSummaryOutput) {
    std::ostringstream output;
    DiagnosticFormatter::Options options;
    options.show_colors = false;
    
    StreamFormatter stream_formatter(output, DiagnosticFormatter(options));
    stream_formatter.write_summary(1, 2, 0);
    
    String result = output.str();
    EXPECT_NE(result.find("1 error"), String::npos);
    EXPECT_NE(result.find("2 warnings"), String::npos);
}

TEST_F(FormatterTest, StreamFormatterChaining) {
    std::ostringstream output;
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    
    StreamFormatter stream_formatter(output, DiagnosticFormatter(options));
    
    Diagnostic diagnostic1(error_message_);
    Diagnostic diagnostic2(warning_message_);
    
    stream_formatter
        .write(diagnostic1)
        .write(diagnostic2)
        .write_summary(1, 1, 0)
        .flush();
    
    String result = output.str();
    EXPECT_NE(result.find("unexpected token ';'"), String::npos);
    EXPECT_NE(result.find("unused variable 'x'"), String::npos);
    EXPECT_NE(result.find("1 error"), String::npos);
}

// Integration tests

TEST_F(FormatterTest, EndToEndFormattingWorkflow) {
    // Create a complete diagnostic with multiple notes
    Diagnostic diagnostic(error_message_);
    SourceLocation note_location("test.pht", 12, 8, 150);
    diagnostic.add_note("variable 'x' declared here", note_location);
    diagnostic.add_note("consider initializing the variable", note_location);
    
    // Format with different options
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    String result = formatter.format(diagnostic);
    
    // Verify all components are present
    EXPECT_NE(result.find("error"), String::npos);
    EXPECT_NE(result.find("unexpected token ';'"), String::npos);
    EXPECT_NE(result.find("test.pht:10:5"), String::npos);
    EXPECT_NE(result.find("variable 'x' declared here"), String::npos);
    EXPECT_NE(result.find("consider initializing"), String::npos);
    EXPECT_NE(result.find("[E2001]"), String::npos);
}

TEST_F(FormatterTest, ColorFormattingIntegration) {
    ColorFormatter::set_color_enabled(true);
    
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = true;
    DiagnosticFormatter formatter(options);
    
    Diagnostic diagnostic(error_message_);
    String result = formatter.format(diagnostic);
    
    // Should contain ANSI escape sequences
    EXPECT_NE(result.find("\033["), String::npos);
    EXPECT_NE(result.find("\033[0m"), String::npos); // Reset sequence
}

TEST_F(FormatterTest, LineTruncationHandling) {
    DiagnosticFormatter::Options options;
    options.max_line_length = 20;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    // This would be tested with actual source code context
    // For now, just verify the option is stored correctly
    EXPECT_EQ(formatter.options().max_line_length, 20u);
}