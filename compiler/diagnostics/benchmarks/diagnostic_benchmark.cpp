/**
 * @file diagnostic_benchmark.cpp
 * @brief Performance benchmarks for diagnostic system components
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/diagnostics/diagnostic.hpp"
#include "photon/diagnostics/diagnostic_engine.hpp"
#include "photon/diagnostics/formatter.hpp"
#include "photon/memory/arena.hpp"
#include <benchmark/benchmark.h>

using namespace photon::diagnostics;
using namespace photon::memory;

namespace {

// Test data for benchmarks
constexpr const char* TEST_FILENAME = "benchmark_test.pht";
constexpr usize TEST_LINE = 42;
constexpr usize TEST_COLUMN = 15;
constexpr usize TEST_OFFSET = 1000;

// Helper to create consistent test data
inline auto create_test_location() -> SourceLocation {
    return SourceLocation(TEST_FILENAME, TEST_LINE, TEST_COLUMN, TEST_OFFSET);
}

inline auto create_test_message(DiagnosticLevel level = DiagnosticLevel::Error) -> DiagnosticMessage {
    return DiagnosticMessage(
        level,
        DiagnosticCode::SyntaxUnexpectedToken,
        "benchmark diagnostic message for performance testing",
        create_test_location()
    );
}

} // anonymous namespace

// SourceLocation benchmarks

static void BM_SourceLocationConstruction(benchmark::State& state) {
    for (auto _ : state) {
        auto location = SourceLocation(TEST_FILENAME, TEST_LINE, TEST_COLUMN, TEST_OFFSET);
        benchmark::DoNotOptimize(location);
    }
}
BENCHMARK(BM_SourceLocationConstruction);

static void BM_SourceLocationComparison(benchmark::State& state) {
    auto loc1 = create_test_location();
    auto loc2 = SourceLocation(TEST_FILENAME, TEST_LINE + 1, TEST_COLUMN, TEST_OFFSET + 50);
    
    for (auto _ : state) {
        bool result = loc1 < loc2;
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_SourceLocationComparison);

static void BM_SourceRangeContains(benchmark::State& state) {
    auto start = create_test_location();
    auto end = SourceLocation(TEST_FILENAME, TEST_LINE + 5, TEST_COLUMN + 20, TEST_OFFSET + 100);
    auto range = SourceRange(start, end);
    auto test_location = SourceLocation(TEST_FILENAME, TEST_LINE + 2, TEST_COLUMN + 10, TEST_OFFSET + 50);
    
    for (auto _ : state) {
        bool result = range.contains(test_location);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_SourceRangeContains);

// DiagnosticMessage benchmarks

static void BM_DiagnosticMessageConstruction(benchmark::State& state) {
    auto location = create_test_location();
    
    for (auto _ : state) {
        auto message = DiagnosticMessage(
            DiagnosticLevel::Error,
            DiagnosticCode::SyntaxUnexpectedToken,
            "benchmark message",
            location
        );
        benchmark::DoNotOptimize(message);
    }
}
BENCHMARK(BM_DiagnosticMessageConstruction);

static void BM_DiagnosticMessageErrorDetection(benchmark::State& state) {
    auto message = create_test_message();
    
    for (auto _ : state) {
        bool is_error = message.is_error();
        bool is_fatal = message.is_fatal();
        benchmark::DoNotOptimize(is_error);
        benchmark::DoNotOptimize(is_fatal);
    }
}
BENCHMARK(BM_DiagnosticMessageErrorDetection);

// Diagnostic benchmarks

static void BM_DiagnosticConstruction(benchmark::State& state) {
    auto message = create_test_message();
    
    for (auto _ : state) {
        auto diagnostic = Diagnostic(message);
        benchmark::DoNotOptimize(diagnostic);
    }
}
BENCHMARK(BM_DiagnosticConstruction);

static void BM_DiagnosticAddNote(benchmark::State& state) {
    auto message = create_test_message();
    auto note_location = create_test_location();
    
    for (auto _ : state) {
        auto diagnostic = Diagnostic(message);
        diagnostic.add_note("benchmark note message", note_location);
        benchmark::DoNotOptimize(diagnostic);
    }
}
BENCHMARK(BM_DiagnosticAddNote);

static void BM_DiagnosticAddMultipleNotes(benchmark::State& state) {
    auto message = create_test_message();
    auto note_location = create_test_location();
    
    for (auto _ : state) {
        auto diagnostic = Diagnostic(message);
        diagnostic.add_note("first note", note_location)
                  .add_note("second note", note_location)
                  .add_note("third note", note_location)
                  .add_note("fourth note", note_location)
                  .add_note("fifth note", note_location);
        benchmark::DoNotOptimize(diagnostic);
    }
}
BENCHMARK(BM_DiagnosticAddMultipleNotes);

// DiagnosticEngine benchmarks

static void BM_DiagnosticEngineCreation(benchmark::State& state) {
    for (auto _ : state) {
        MemoryArena<> arena;
        auto engine = DiagnosticEngine(arena);
        benchmark::DoNotOptimize(engine);
    }
}
BENCHMARK(BM_DiagnosticEngineCreation);

static void BM_DiagnosticEngineReportError(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    
    for (auto _ : state) {
        bool result = engine.error(DiagnosticCode::SyntaxUnexpectedToken, "benchmark error", location);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DiagnosticEngineReportError);

static void BM_DiagnosticEngineReportWarning(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    
    for (auto _ : state) {
        bool result = engine.warning(DiagnosticCode::SyntaxUnexpectedToken, "benchmark warning", location);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DiagnosticEngineReportWarning);

static void BM_DiagnosticEngineReportNote(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    
    for (auto _ : state) {
        bool result = engine.note("benchmark note", location);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DiagnosticEngineReportNote);

static void BM_DiagnosticEngineFilterByLevel(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    
    // Pre-populate with diagnostics
    for (int i = 0; i < 100; ++i) {
        engine.error(DiagnosticCode::SyntaxUnexpectedToken, "error", location);
        engine.warning(DiagnosticCode::SyntaxUnexpectedToken, "warning", location);
        engine.note("note", location);
    }
    
    for (auto _ : state) {
        auto errors = engine.diagnostics_by_level(DiagnosticLevel::Error);
        benchmark::DoNotOptimize(errors);
    }
}
BENCHMARK(BM_DiagnosticEngineFilterByLevel);

static void BM_DiagnosticEngineFilterByCode(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    
    // Pre-populate with different diagnostic codes
    for (int i = 0; i < 100; ++i) {
        engine.error(DiagnosticCode::SyntaxUnexpectedToken, "syntax error", location);
        engine.error(DiagnosticCode::SemanticTypeMismatch, "type error", location);
        engine.error(DiagnosticCode::LexInvalidCharacter, "lex error", location);
    }
    
    for (auto _ : state) {
        auto syntax_errors = engine.diagnostics_by_code(DiagnosticCode::SyntaxUnexpectedToken);
        benchmark::DoNotOptimize(syntax_errors);
    }
}
BENCHMARK(BM_DiagnosticEngineFilterByCode);

static void BM_DiagnosticEngineSortByLocation(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    
    // Pre-populate with diagnostics in random order
    for (int i = 100; i >= 0; --i) {
        auto location = SourceLocation(TEST_FILENAME, static_cast<usize>(i), 1, static_cast<usize>(i * 10));
        engine.error(DiagnosticCode::SyntaxUnexpectedToken, "error", location);
    }
    
    for (auto _ : state) {
        engine.sort_by_location();
        benchmark::DoNotOptimize(engine.diagnostics());
    }
}
BENCHMARK(BM_DiagnosticEngineSortByLocation);

static void BM_DiagnosticEngineSortBySeverity(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    
    // Pre-populate with diagnostics in mixed order
    for (int i = 0; i < 25; ++i) {
        engine.note("note", location);
        engine.warning(DiagnosticCode::SyntaxUnexpectedToken, "warning", location);
        engine.error(DiagnosticCode::SyntaxUnexpectedToken, "error", location);
        if (i % 10 == 0) {
            engine.fatal(DiagnosticCode::InternalCompilerError, "fatal", location);
        }
    }
    
    for (auto _ : state) {
        engine.sort_by_severity();
        benchmark::DoNotOptimize(engine.diagnostics());
    }
}
BENCHMARK(BM_DiagnosticEngineSortBySeverity);

// DiagnosticBuilder benchmarks

static void BM_DiagnosticBuilderBasicUsage(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    
    for (auto _ : state) {
        auto builder = make_error(engine, DiagnosticCode::SyntaxUnexpectedToken, "builder error", location);
        bool result = builder.emit();
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DiagnosticBuilderBasicUsage);

static void BM_DiagnosticBuilderWithNotes(benchmark::State& state) {
    MemoryArena<> arena;
    DiagnosticEngine engine(arena);
    auto location = create_test_location();
    auto note_location = create_test_location();
    
    for (auto _ : state) {
        make_error(engine, DiagnosticCode::SyntaxUnexpectedToken, "builder error", location)
            .note("first note", note_location)
            .note("second note", note_location)
            .suggest("try this", note_location)
            .help("see documentation")
            .emit();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DiagnosticBuilderWithNotes);

// ColorFormatter benchmarks

static void BM_ColorFormatterEscapeSequence(benchmark::State& state) {
    ColorFormatter::set_color_enabled(true);
    
    for (auto _ : state) {
        auto sequence = ColorFormatter::escape_sequence(ColorCode::Red);
        benchmark::DoNotOptimize(sequence);
    }
}
BENCHMARK(BM_ColorFormatterEscapeSequence);

static void BM_ColorFormatterColorize(benchmark::State& state) {
    ColorFormatter::set_color_enabled(true);
    
    for (auto _ : state) {
        auto colored = ColorFormatter::colorize("benchmark text", ColorCode::Red);
        benchmark::DoNotOptimize(colored);
    }
}
BENCHMARK(BM_ColorFormatterColorize);

static void BM_ColorFormatterHelperMethods(benchmark::State& state) {
    ColorFormatter::set_color_enabled(true);
    
    for (auto _ : state) {
        auto red_text = ColorFormatter::red("error");
        auto yellow_text = ColorFormatter::yellow("warning");
        auto blue_text = ColorFormatter::blue("note");
        benchmark::DoNotOptimize(red_text);
        benchmark::DoNotOptimize(yellow_text);
        benchmark::DoNotOptimize(blue_text);
    }
}
BENCHMARK(BM_ColorFormatterHelperMethods);

// DiagnosticFormatter benchmarks

static void BM_DiagnosticFormatterSimple(benchmark::State& state) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    auto message = create_test_message();
    auto diagnostic = Diagnostic(message);
    
    for (auto _ : state) {
        auto result = formatter.format(diagnostic);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DiagnosticFormatterSimple);

static void BM_DiagnosticFormatterWithNotes(benchmark::State& state) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    auto message = create_test_message();
    auto diagnostic = Diagnostic(message);
    auto note_location = create_test_location();
    diagnostic.add_note("first note", note_location)
              .add_note("second note", note_location)
              .add_note("third note", note_location);
    
    for (auto _ : state) {
        auto result = formatter.format(diagnostic);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DiagnosticFormatterWithNotes);

static void BM_DiagnosticFormatterWithColors(benchmark::State& state) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = true;
    DiagnosticFormatter formatter(options);
    ColorFormatter::set_color_enabled(true);
    
    auto message = create_test_message();
    auto diagnostic = Diagnostic(message);
    
    for (auto _ : state) {
        auto result = formatter.format(diagnostic);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DiagnosticFormatterWithColors);

static void BM_DiagnosticFormatterCompactMode(benchmark::State& state) {
    DiagnosticFormatter::Options options;
    options.compact_mode = true;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    auto message = create_test_message();
    auto diagnostic = Diagnostic(message);
    
    for (auto _ : state) {
        auto result = formatter.format(diagnostic);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DiagnosticFormatterCompactMode);

static void BM_DiagnosticFormatterMultipleDiagnostics(benchmark::State& state) {
    DiagnosticFormatter::Options options;
    options.show_source_context = false;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    Vec<Diagnostic> diagnostics;
    for (int i = 0; i < 10; ++i) {
        auto message = create_test_message();
        diagnostics.emplace_back(message);
    }
    
    for (auto _ : state) {
        auto result = formatter.format_all(diagnostics);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DiagnosticFormatterMultipleDiagnostics);

static void BM_DiagnosticFormatterSummary(benchmark::State& state) {
    DiagnosticFormatter::Options options;
    options.show_colors = false;
    DiagnosticFormatter formatter(options);
    
    for (auto _ : state) {
        auto summary = formatter.format_summary(5, 10, 3);
        benchmark::DoNotOptimize(summary);
    }
}
BENCHMARK(BM_DiagnosticFormatterSummary);

// Memory usage benchmarks

static void BM_DiagnosticMemoryUsage(benchmark::State& state) {
    for (auto _ : state) {
        MemoryArena<> arena;
        DiagnosticEngine engine(arena);
        auto location = create_test_location();
        
        // Report many diagnostics to stress memory allocation
        for (int i = 0; i < 1000; ++i) {
            engine.error(DiagnosticCode::SyntaxUnexpectedToken, "memory test error", location);
        }
        
        auto memory_usage = engine.memory_usage();
        benchmark::DoNotOptimize(memory_usage);
    }
    
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_DiagnosticMemoryUsage);

// Stress tests

static void BM_DiagnosticEngineStressTest(benchmark::State& state) {
    constexpr int num_diagnostics = 10000;
    
    for (auto _ : state) {
        MemoryArena<> arena;
        DiagnosticEngine engine(arena);
        auto location = create_test_location();
        
        // Mix of different diagnostic types
        for (int i = 0; i < num_diagnostics; ++i) {
            switch (i % 4) {
                case 0:
                    engine.error(DiagnosticCode::SyntaxUnexpectedToken, "stress error", location);
                    break;
                case 1:
                    engine.warning(DiagnosticCode::SyntaxUnexpectedToken, "stress warning", location);
                    break;
                case 2:
                    engine.note("stress note", location);
                    break;
                case 3:
                    make_error(engine, DiagnosticCode::SyntaxUnexpectedToken, "stress builder", location)
                        .note("stress note", location)
                        .emit();
                    break;
            }
        }
        
        benchmark::DoNotOptimize(engine.total_count());
    }
    
    state.SetItemsProcessed(state.iterations() * num_diagnostics);
}
BENCHMARK(BM_DiagnosticEngineStressTest);

BENCHMARK_MAIN();