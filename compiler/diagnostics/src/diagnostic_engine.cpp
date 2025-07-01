/**
 * @file diagnostic_engine.cpp
 * @brief Implementation of diagnostic engine functionality
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/diagnostics/diagnostic_engine.hpp"
#include <algorithm>

namespace photon::diagnostics {

auto DiagnosticEngine::report(Diagnostic diagnostic) -> bool {
    // Check if we should stop accepting diagnostics
    if (should_stop_compilation()) {
        return false;
    }
    
    // Update counters
    update_counters(diagnostic);
    
    // Store diagnostic in arena-allocated memory
    diagnostics_.push_back(std::move(diagnostic));
    
    // Check if this diagnostic should stop compilation
    return !should_stop_compilation();
}

auto DiagnosticEngine::report(DiagnosticLevel level, DiagnosticCode code,
                             String message, SourceLocation location) -> bool {
    auto diagnostic_message = DiagnosticMessage(level, code, std::move(message), location);
    auto diagnostic = Diagnostic(std::move(diagnostic_message));
    return report(std::move(diagnostic));
}

auto DiagnosticEngine::fatal(DiagnosticCode code, String message, SourceLocation location) -> bool {
    fatal_encountered_.store(true, std::memory_order_relaxed);
    report(DiagnosticLevel::Fatal, code, std::move(message), location);
    return false; // Fatal errors always return false to stop compilation
}

auto DiagnosticEngine::filtered_diagnostics(const FilterPredicate& filter) const -> Vec<Diagnostic> {
    Vec<Diagnostic> filtered;
    filtered.reserve(diagnostics_.size() / 2); // Rough estimate
    
    for (const auto& diagnostic : diagnostics_) {
        if (filter(diagnostic)) {
            filtered.push_back(diagnostic);
        }
    }
    
    return filtered;
}

auto DiagnosticEngine::diagnostics_by_level(DiagnosticLevel level) const -> Vec<Diagnostic> {
    return filtered_diagnostics([level](const Diagnostic& diag) {
        return diag.level() == level;
    });
}

auto DiagnosticEngine::diagnostics_by_code(DiagnosticCode code) const -> Vec<Diagnostic> {
    return filtered_diagnostics([code](const Diagnostic& diag) {
        return diag.code() == code;
    });
}

auto DiagnosticEngine::clear() noexcept -> void {
    diagnostics_.clear();
    arena_.reset();
    
    error_count_.store(0, std::memory_order_relaxed);
    warning_count_.store(0, std::memory_order_relaxed);
    note_count_.store(0, std::memory_order_relaxed);
    fatal_encountered_.store(false, std::memory_order_relaxed);
}

auto DiagnosticEngine::sort_by_location() -> void {
    std::sort(diagnostics_.begin(), diagnostics_.end(),
              [](const Diagnostic& a, const Diagnostic& b) {
                  const auto& loc_a = a.primary().location();
                  const auto& loc_b = b.primary().location();
                  
                  // First sort by filename
                  if (loc_a.filename() != loc_b.filename()) {
                      return loc_a.filename() < loc_b.filename();
                  }
                  
                  // Then by line number
                  if (loc_a.line() != loc_b.line()) {
                      return loc_a.line() < loc_b.line();
                  }
                  
                  // Finally by column
                  return loc_a.column() < loc_b.column();
              });
}

auto DiagnosticEngine::sort_by_severity() -> void {
    std::sort(diagnostics_.begin(), diagnostics_.end(),
              [](const Diagnostic& a, const Diagnostic& b) {
                  // Fatal > Error > Warning > Note
                  auto level_priority = [](DiagnosticLevel level) -> int {
                      switch (level) {
                          case DiagnosticLevel::Fatal: return 3;
                          case DiagnosticLevel::Error: return 2;
                          case DiagnosticLevel::Warning: return 1;
                          case DiagnosticLevel::Note: return 0;
                      }
                      return 0;
                  };
                  
                  return level_priority(a.level()) > level_priority(b.level());
              });
}

auto DiagnosticEngine::update_counters(const Diagnostic& diagnostic) noexcept -> void {
    switch (diagnostic.level()) {
        case DiagnosticLevel::Fatal:
            fatal_encountered_.store(true, std::memory_order_relaxed);
            [[fallthrough]];
        case DiagnosticLevel::Error:
            error_count_.fetch_add(1, std::memory_order_relaxed);
            break;
        case DiagnosticLevel::Warning:
            warning_count_.fetch_add(1, std::memory_order_relaxed);
            break;
        case DiagnosticLevel::Note:
            note_count_.fetch_add(1, std::memory_order_relaxed);
            break;
    }
}

} // namespace photon::diagnostics