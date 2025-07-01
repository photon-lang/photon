/**
 * @file diagnostic_test.cpp
 * @brief Comprehensive test suite for diagnostic system components
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/diagnostics/diagnostic.hpp"
#include "photon/diagnostics/diagnostic_engine.hpp"
#include <gtest/gtest.h>

using namespace photon::diagnostics;
using namespace photon::memory;

namespace {

class DiagnosticTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena_ = std::make_unique<MemoryArena<>>();
        location_ = SourceLocation("test.pht", 10, 5, 100);
        message_ = DiagnosticMessage(
            DiagnosticLevel::Error,
            DiagnosticCode::SyntaxUnexpectedToken,
            "unexpected token",
            location_
        );
    }
    
    std::unique_ptr<MemoryArena<>> arena_;
    SourceLocation location_;
    DiagnosticMessage message_;
};

} // anonymous namespace

// DiagnosticMessage tests

TEST_F(DiagnosticTest, DiagnosticMessageConstructionSetsProperties) {
    EXPECT_EQ(message_.level(), DiagnosticLevel::Error);
    EXPECT_EQ(message_.code(), DiagnosticCode::SyntaxUnexpectedToken);
    EXPECT_EQ(message_.message(), "unexpected token");
    EXPECT_EQ(message_.location(), location_);
}

TEST_F(DiagnosticTest, DiagnosticMessageErrorDetection) {
    DiagnosticMessage error_msg(DiagnosticLevel::Error, DiagnosticCode(1), "error", location_);
    DiagnosticMessage fatal_msg(DiagnosticLevel::Fatal, DiagnosticCode(1), "fatal", location_);
    DiagnosticMessage warning_msg(DiagnosticLevel::Warning, DiagnosticCode(1), "warning", location_);
    DiagnosticMessage note_msg(DiagnosticLevel::Note, DiagnosticCode(1), "note", location_);
    
    EXPECT_TRUE(error_msg.is_error());
    EXPECT_TRUE(fatal_msg.is_error());
    EXPECT_FALSE(warning_msg.is_error());
    EXPECT_FALSE(note_msg.is_error());
}

TEST_F(DiagnosticTest, DiagnosticMessageFatalDetection) {
    DiagnosticMessage fatal_msg(DiagnosticLevel::Fatal, DiagnosticCode(1), "fatal", location_);
    DiagnosticMessage error_msg(DiagnosticLevel::Error, DiagnosticCode(1), "error", location_);
    
    EXPECT_TRUE(fatal_msg.is_fatal());
    EXPECT_FALSE(error_msg.is_fatal());
}

TEST_F(DiagnosticTest, DiagnosticMessageErrorCodeExtraction) {
    EXPECT_EQ(message_.error_code(), static_cast<u32>(DiagnosticCode::SyntaxUnexpectedToken));
}

// Diagnostic tests

TEST_F(DiagnosticTest, DiagnosticConstructionWithPrimaryMessage) {
    Diagnostic diag(message_);
    
    EXPECT_EQ(diag.primary().message(), "unexpected token");
    EXPECT_EQ(diag.level(), DiagnosticLevel::Error);
    EXPECT_EQ(diag.code(), DiagnosticCode::SyntaxUnexpectedToken);
    EXPECT_TRUE(diag.is_error());
    EXPECT_FALSE(diag.is_fatal());
    EXPECT_EQ(diag.message_count(), 1u);
    EXPECT_TRUE(diag.notes().empty());
}

TEST_F(DiagnosticTest, DiagnosticAddNoteMessage) {
    Diagnostic diag(message_);
    SourceLocation note_location("test.pht", 12, 8, 150);
    DiagnosticMessage note(DiagnosticLevel::Note, DiagnosticCode(0), "helpful note", note_location);
    
    diag.add_note(note);
    
    EXPECT_EQ(diag.message_count(), 2u);
    EXPECT_EQ(diag.notes().size(), 1u);
    EXPECT_EQ(diag.notes()[0].message(), "helpful note");
    EXPECT_EQ(diag.notes()[0].location(), note_location);
}

TEST_F(DiagnosticTest, DiagnosticAddSimpleNote) {
    Diagnostic diag(message_);
    SourceLocation note_location("test.pht", 12, 8, 150);
    
    diag.add_note("simple note", note_location);
    
    EXPECT_EQ(diag.message_count(), 2u);
    EXPECT_EQ(diag.notes().size(), 1u);
    EXPECT_EQ(diag.notes()[0].message(), "simple note");
    EXPECT_EQ(diag.notes()[0].location(), note_location);
    EXPECT_EQ(diag.notes()[0].level(), DiagnosticLevel::Note);
}

TEST_F(DiagnosticTest, DiagnosticFluentNoteChaining) {
    Diagnostic diag(message_);
    SourceLocation loc1("test.pht", 12, 8, 150);
    SourceLocation loc2("test.pht", 15, 3, 200);
    
    diag.add_note("first note", loc1)
        .add_note("second note", loc2);
    
    EXPECT_EQ(diag.message_count(), 3u);
    EXPECT_EQ(diag.notes().size(), 2u);
    EXPECT_EQ(diag.notes()[0].message(), "first note");
    EXPECT_EQ(diag.notes()[1].message(), "second note");
}

// DiagnosticEngine tests

class DiagnosticEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        arena_ = std::make_unique<MemoryArena<>>();
        engine_ = std::make_unique<DiagnosticEngine>(*arena_);
        location_ = SourceLocation("test.pht", 10, 5, 100);
    }
    
    std::unique_ptr<MemoryArena<>> arena_;
    std::unique_ptr<DiagnosticEngine> engine_;
    SourceLocation location_;
};

TEST_F(DiagnosticEngineTest, InitialStateIsEmpty) {
    EXPECT_EQ(engine_->total_count(), 0u);
    EXPECT_EQ(engine_->error_count(), 0u);
    EXPECT_EQ(engine_->warning_count(), 0u);
    EXPECT_EQ(engine_->note_count(), 0u);
    EXPECT_FALSE(engine_->has_errors());
    EXPECT_FALSE(engine_->has_fatal_error());
    EXPECT_FALSE(engine_->should_stop_compilation());
    EXPECT_TRUE(engine_->diagnostics().empty());
}

TEST_F(DiagnosticEngineTest, ReportErrorUpdatesCounts) {
    bool result = engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "test error", location_);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(engine_->total_count(), 1u);
    EXPECT_EQ(engine_->error_count(), 1u);
    EXPECT_EQ(engine_->warning_count(), 0u);
    EXPECT_EQ(engine_->note_count(), 0u);
    EXPECT_TRUE(engine_->has_errors());
    EXPECT_FALSE(engine_->has_fatal_error());
}

TEST_F(DiagnosticEngineTest, ReportWarningUpdatesCounts) {
    bool result = engine_->warning(DiagnosticCode::SyntaxUnexpectedToken, "test warning", location_);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(engine_->total_count(), 1u);
    EXPECT_EQ(engine_->error_count(), 0u);
    EXPECT_EQ(engine_->warning_count(), 1u);
    EXPECT_EQ(engine_->note_count(), 0u);
    EXPECT_FALSE(engine_->has_errors());
}

TEST_F(DiagnosticEngineTest, ReportNoteUpdatesCounts) {
    bool result = engine_->note("test note", location_);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(engine_->total_count(), 1u);
    EXPECT_EQ(engine_->error_count(), 0u);
    EXPECT_EQ(engine_->warning_count(), 0u);
    EXPECT_EQ(engine_->note_count(), 1u);
    EXPECT_FALSE(engine_->has_errors());
}

TEST_F(DiagnosticEngineTest, ReportFatalSetsFatalFlag) {
    bool result = engine_->fatal(DiagnosticCode::InternalCompilerError, "fatal error", location_);
    
    EXPECT_FALSE(result); // Fatal always returns false
    EXPECT_EQ(engine_->total_count(), 1u);
    EXPECT_EQ(engine_->error_count(), 1u);
    EXPECT_TRUE(engine_->has_errors());
    EXPECT_TRUE(engine_->has_fatal_error());
    EXPECT_TRUE(engine_->should_stop_compilation());
}

TEST_F(DiagnosticEngineTest, MaxErrorLimitStopsAcceptingErrors) {
    engine_->set_max_errors(2);
    
    EXPECT_TRUE(engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error 1", location_));
    EXPECT_TRUE(engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error 2", location_));
    EXPECT_FALSE(engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error 3", location_));
    
    EXPECT_EQ(engine_->error_count(), 2u);
    EXPECT_TRUE(engine_->error_limit_reached());
    EXPECT_TRUE(engine_->should_stop_compilation());
}

TEST_F(DiagnosticEngineTest, MaxErrorLimitZeroMeansUnlimited) {
    engine_->set_max_errors(0);
    
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error", location_));
    }
    
    EXPECT_EQ(engine_->error_count(), 100u);
    EXPECT_FALSE(engine_->error_limit_reached());
}

TEST_F(DiagnosticEngineTest, WarningsDoNotCountTowardErrorLimit) {
    engine_->set_max_errors(1);
    
    EXPECT_TRUE(engine_->warning(DiagnosticCode::SyntaxUnexpectedToken, "warning", location_));
    EXPECT_TRUE(engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error", location_));
    EXPECT_TRUE(engine_->warning(DiagnosticCode::SyntaxUnexpectedToken, "another warning", location_));
    EXPECT_FALSE(engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "second error", location_));
    
    EXPECT_EQ(engine_->error_count(), 1u);
    EXPECT_EQ(engine_->warning_count(), 2u);
}

TEST_F(DiagnosticEngineTest, ClearResetsAllCounters) {
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error", location_);
    engine_->warning(DiagnosticCode::SyntaxUnexpectedToken, "warning", location_);
    engine_->note("note", location_);
    
    engine_->clear();
    
    EXPECT_EQ(engine_->total_count(), 0u);
    EXPECT_EQ(engine_->error_count(), 0u);
    EXPECT_EQ(engine_->warning_count(), 0u);
    EXPECT_EQ(engine_->note_count(), 0u);
    EXPECT_FALSE(engine_->has_errors());
    EXPECT_FALSE(engine_->has_fatal_error());
    EXPECT_TRUE(engine_->diagnostics().empty());
}

TEST_F(DiagnosticEngineTest, DiagnosticsStoredCorrectly) {
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "test error", location_);
    
    const auto& diagnostics = engine_->diagnostics();
    EXPECT_EQ(diagnostics.size(), 1u);
    EXPECT_EQ(diagnostics[0].primary().message(), "test error");
    EXPECT_EQ(diagnostics[0].primary().location(), location_);
}

TEST_F(DiagnosticEngineTest, FilteredDiagnosticsByLevel) {
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error", location_);
    engine_->warning(DiagnosticCode::SyntaxUnexpectedToken, "warning", location_);
    engine_->note("note", location_);
    
    auto errors = engine_->diagnostics_by_level(DiagnosticLevel::Error);
    auto warnings = engine_->diagnostics_by_level(DiagnosticLevel::Warning);
    auto notes = engine_->diagnostics_by_level(DiagnosticLevel::Note);
    
    EXPECT_EQ(errors.size(), 1u);
    EXPECT_EQ(warnings.size(), 1u);
    EXPECT_EQ(notes.size(), 1u);
    
    EXPECT_EQ(errors[0].primary().message(), "error");
    EXPECT_EQ(warnings[0].primary().message(), "warning");
    EXPECT_EQ(notes[0].primary().message(), "note");
}

TEST_F(DiagnosticEngineTest, FilteredDiagnosticsByCode) {
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "syntax error", location_);
    engine_->error(DiagnosticCode::SemanticTypeMismatch, "type error", location_);
    
    auto syntax_errors = engine_->diagnostics_by_code(DiagnosticCode::SyntaxUnexpectedToken);
    auto type_errors = engine_->diagnostics_by_code(DiagnosticCode::SemanticTypeMismatch);
    
    EXPECT_EQ(syntax_errors.size(), 1u);
    EXPECT_EQ(type_errors.size(), 1u);
    
    EXPECT_EQ(syntax_errors[0].primary().message(), "syntax error");
    EXPECT_EQ(type_errors[0].primary().message(), "type error");
}

TEST_F(DiagnosticEngineTest, CustomFilterPredicate) {
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "short", location_);
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "this is a longer message", location_);
    
    auto long_messages = engine_->filtered_diagnostics([](const Diagnostic& diag) {
        return diag.primary().message().length() > 10;
    });
    
    EXPECT_EQ(long_messages.size(), 1u);
    EXPECT_EQ(long_messages[0].primary().message(), "this is a longer message");
}

TEST_F(DiagnosticEngineTest, SortByLocationOrdering) {
    SourceLocation loc1("file1.pht", 10, 5, 100);
    SourceLocation loc2("file1.pht", 5, 3, 50);
    SourceLocation loc3("file2.pht", 1, 1, 0);
    
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error 1", loc1);
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error 2", loc2);
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error 3", loc3);
    
    engine_->sort_by_location();
    
    const auto& diagnostics = engine_->diagnostics();
    EXPECT_EQ(diagnostics[0].primary().location(), loc2); // file1.pht:5:3
    EXPECT_EQ(diagnostics[1].primary().location(), loc1); // file1.pht:10:5
    EXPECT_EQ(diagnostics[2].primary().location(), loc3); // file2.pht:1:1
}

TEST_F(DiagnosticEngineTest, SortBySeverityOrdering) {
    engine_->note("note", location_);
    engine_->warning(DiagnosticCode::SyntaxUnexpectedToken, "warning", location_);
    engine_->fatal(DiagnosticCode::InternalCompilerError, "fatal", location_);
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "error", location_);
    
    engine_->sort_by_severity();
    
    const auto& diagnostics = engine_->diagnostics();
    EXPECT_EQ(diagnostics[0].level(), DiagnosticLevel::Fatal);
    EXPECT_EQ(diagnostics[1].level(), DiagnosticLevel::Error);
    EXPECT_EQ(diagnostics[2].level(), DiagnosticLevel::Warning);
    EXPECT_EQ(diagnostics[3].level(), DiagnosticLevel::Note);
}

TEST_F(DiagnosticEngineTest, MemoryUsageTracking) {
    usize initial_usage = engine_->memory_usage();
    
    engine_->error(DiagnosticCode::SyntaxUnexpectedToken, "test error", location_);
    
    EXPECT_GT(engine_->memory_usage(), initial_usage);
}

// DiagnosticBuilder tests

TEST_F(DiagnosticEngineTest, DiagnosticBuilderBasicUsage) {
    auto builder = make_error(*engine_, DiagnosticCode::SyntaxUnexpectedToken, "test error", location_);
    bool result = builder.emit();
    
    EXPECT_TRUE(result);
    EXPECT_EQ(engine_->error_count(), 1u);
    EXPECT_EQ(engine_->diagnostics()[0].primary().message(), "test error");
}

TEST_F(DiagnosticEngineTest, DiagnosticBuilderFluentInterface) {
    SourceLocation note_loc("test.pht", 12, 8, 150);
    
    make_error(*engine_, DiagnosticCode::SyntaxUnexpectedToken, "main error", location_)
        .note("helpful note", note_loc)
        .suggest("try this instead", note_loc)
        .help("see documentation for more details")
        .emit();
    
    EXPECT_EQ(engine_->error_count(), 1u);
    
    const auto& diagnostic = engine_->diagnostics()[0];
    EXPECT_EQ(diagnostic.message_count(), 4u); // Primary + 3 notes
    EXPECT_EQ(diagnostic.notes().size(), 3u);
    
    EXPECT_EQ(diagnostic.notes()[0].message(), "helpful note");
    EXPECT_EQ(diagnostic.notes()[1].message(), "suggestion: try this instead");
    EXPECT_EQ(diagnostic.notes()[2].message(), "help: see documentation for more details");
}

TEST_F(DiagnosticEngineTest, DiagnosticBuilderAutomaticEmit) {
    {
        auto builder = make_warning(*engine_, DiagnosticCode::SyntaxUnexpectedToken, "auto emit", location_);
        // Builder destructor should automatically emit
    }
    
    EXPECT_EQ(engine_->warning_count(), 1u);
    EXPECT_EQ(engine_->diagnostics()[0].primary().message(), "auto emit");
}

TEST_F(DiagnosticEngineTest, DiagnosticBuilderMoveSemantics) {
    auto builder1 = make_error(*engine_, DiagnosticCode::SyntaxUnexpectedToken, "movable", location_);
    auto builder2 = std::move(builder1);
    
    bool result = builder2.emit();
    
    EXPECT_TRUE(result);
    EXPECT_EQ(engine_->error_count(), 1u);
}

// Factory function tests

TEST_F(DiagnosticEngineTest, MakeErrorFactoryFunction) {
    auto builder = make_error(*engine_, DiagnosticCode::SyntaxUnexpectedToken, "factory error", location_);
    builder.emit();
    
    EXPECT_EQ(engine_->error_count(), 1u);
    EXPECT_EQ(engine_->diagnostics()[0].level(), DiagnosticLevel::Error);
}

TEST_F(DiagnosticEngineTest, MakeWarningFactoryFunction) {
    auto builder = make_warning(*engine_, DiagnosticCode::SyntaxUnexpectedToken, "factory warning", location_);
    builder.emit();
    
    EXPECT_EQ(engine_->warning_count(), 1u);
    EXPECT_EQ(engine_->diagnostics()[0].level(), DiagnosticLevel::Warning);
}

TEST_F(DiagnosticEngineTest, MakeFatalFactoryFunction) {
    auto builder = make_fatal(*engine_, DiagnosticCode::InternalCompilerError, "factory fatal", location_);
    bool result = builder.emit();
    
    EXPECT_FALSE(result);
    EXPECT_EQ(engine_->error_count(), 1u);
    EXPECT_TRUE(engine_->has_fatal_error());
    EXPECT_EQ(engine_->diagnostics()[0].level(), DiagnosticLevel::Fatal);
}

// Error code categorization tests

TEST_F(DiagnosticEngineTest, ErrorCodeCategories) {
    // Test different error code categories
    EXPECT_EQ(static_cast<u32>(DiagnosticCode::LexInvalidCharacter), 1001u);
    EXPECT_EQ(static_cast<u32>(DiagnosticCode::SyntaxUnexpectedToken), 2001u);
    EXPECT_EQ(static_cast<u32>(DiagnosticCode::SemanticUndeclaredIdentifier), 3001u);
    EXPECT_EQ(static_cast<u32>(DiagnosticCode::TypeInferenceFailure), 4001u);
    EXPECT_EQ(static_cast<u32>(DiagnosticCode::OwnershipMoveAfterBorrow), 5001u);
    EXPECT_EQ(static_cast<u32>(DiagnosticCode::QuantumInvalidMeasurement), 6001u);
    EXPECT_EQ(static_cast<u32>(DiagnosticCode::InternalCompilerError), 9001u);
}

// Thread safety tests (basic verification)

TEST_F(DiagnosticEngineTest, ConcurrentCounterUpdates) {
    // This is a basic test - full thread safety would require more sophisticated testing
    std::atomic<int> completed_threads{0};
    constexpr int num_threads = 4;
    constexpr int reports_per_thread = 10;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < reports_per_thread; ++j) {
                engine_->error(DiagnosticCode::SyntaxUnexpectedToken, 
                              "thread " + std::to_string(i) + " error " + std::to_string(j), 
                              location_);
            }
            completed_threads.fetch_add(1);
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completed_threads.load(), num_threads);
    EXPECT_EQ(engine_->error_count(), static_cast<usize>(num_threads * reports_per_thread));
}