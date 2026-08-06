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
    const bool is_error = diagnostic.is_error();
    const bool is_fatal = diagnostic.is_fatal();

    std::lock_guard<std::mutex> guard(mutex_);

    if (is_error && error_limit_reached()) {
        return false;
    }

    update_counters(diagnostic);
    diagnostics_.push_back(std::move(diagnostic));

    return !is_fatal;
}

auto DiagnosticEngine::report(DiagnosticLevel level, DiagnosticCode code,
                             String message, SourceLocation location) -> bool {
    auto diagnostic_message = DiagnosticMessage(level, code, std::move(message), location);
    auto diagnostic = Diagnostic(std::move(diagnostic_message));
    return report(std::move(diagnostic));
}

auto DiagnosticEngine::fatal(DiagnosticCode code, String message, SourceLocation location) -> bool {
    report(DiagnosticLevel::Fatal, code, std::move(message), location);
    return false; // Fatal errors always return false to stop compilation
}

auto DiagnosticEngine::filtered_diagnostics(const FilterPredicate& filter) const -> Vec<Diagnostic> {
    std::lock_guard<std::mutex> guard(mutex_);

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
    std::lock_guard<std::mutex> guard(mutex_);

    diagnostics_.clear();

    error_count_.store(0, std::memory_order_relaxed);
    warning_count_.store(0, std::memory_order_relaxed);
    note_count_.store(0, std::memory_order_relaxed);
    fatal_encountered_.store(false, std::memory_order_relaxed);
}

auto DiagnosticEngine::memory_usage() const noexcept -> usize {
    std::lock_guard<std::mutex> guard(mutex_);

    usize message_bytes = 0;
    for (const auto& diagnostic : diagnostics_) {
        message_bytes += diagnostic.primary().message().capacity();
        for (const auto& note : diagnostic.notes()) {
            message_bytes += note.message().capacity();
        }
        message_bytes += diagnostic.notes().capacity() * sizeof(DiagnosticMessage);
    }

    return arena_.bytes_used()
         + diagnostics_.capacity() * sizeof(Diagnostic)
         + message_bytes;
}

auto DiagnosticEngine::sort_by_location() -> void {
    std::lock_guard<std::mutex> guard(mutex_);

    std::stable_sort(diagnostics_.begin(), diagnostics_.end(),
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
    std::lock_guard<std::mutex> guard(mutex_);

    std::stable_sort(diagnostics_.begin(), diagnostics_.end(),
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