/**
 * @file performance_counters.cpp
 * @brief Global performance metrics tracking
 * 
 * The PerformanceCounters class provides atomic counters for tracking
 * rule execution metrics across the entire application. These metrics
 * are useful for monitoring, debugging, and performance analysis.
 * 
 * Thread Safety:
 * - All counters use std::atomic for lock-free updates
 * - getCounters() returns a snapshot (copy) of current values
 * - Safe to call from multiple threads without synchronization
 * 
 * Metrics Tracked:
 * - Execution counts (total, success, failed, skipped)
 * - Cache performance (cached hits)
 * - Error conditions (timeouts, rate limits)
 * - Compilation statistics
 * - Execution time (total nanoseconds, average calculation)
 * 
 * Usage:
 * @code
 * auto& counters = PerformanceCounters::instance();
 * counters.recordExecution(true, false, false, false, false);
 * 
 * auto metrics = counters.getCounters();
 * std::cout << "Executed: " << metrics.totalRulesExecuted << "\n";
 * std::cout << "Avg time: " << counters.getAverageExecutionTimeMs() << "ms\n";
 * @endcode
 * 
 * Note: These are global counters. For per-rule metrics, use the
 * RuleResult or ExecutionTracer classes.
 */

#include "fastrules/performance_counters.hpp"
#include <sstream>

namespace fastrules {

/**
 * @brief Get the global PerformanceCounters singleton
 * 
 * Uses Meyers' singleton pattern for lazy initialization.
 * Thread-safe initialization guaranteed by C++11 static semantics.
 * 
 * @return Reference to the global PerformanceCounters instance
 */
PerformanceCounters& PerformanceCounters::instance() {
    static PerformanceCounters instance;
    return instance;
}

// ============================================================================
// Execution Recording
// ============================================================================

/**
 * @brief Record a rule execution event
 * 
 * Atomically increments the appropriate counters based on the
 * execution outcome. All parameters are booleans indicating
 * whether the corresponding condition occurred.
 * 
 * @param success Whether the rule evaluated to true
 * @param skipped Whether the rule was skipped (inactive or dependency failed)
 * @param cached Whether the result was served from cache
 * @param timedOut Whether the rule exceeded its timeout
 * @param rateLimited Whether the rule was rate limited
 */
void PerformanceCounters::recordExecution(bool success, bool skipped, bool cached, bool timedOut, bool rateLimited) {
    counters_.totalRulesExecuted.fetch_add(1);
    if (success) counters_.totalRulesSuccessful.fetch_add(1);
    else counters_.totalRulesFailed.fetch_add(1);
    if (skipped) counters_.totalRulesSkipped.fetch_add(1);
    if (cached) counters_.totalRulesCached.fetch_add(1);
    if (timedOut) counters_.totalRulesTimedOut.fetch_add(1);
    if (rateLimited) counters_.totalRulesRateLimited.fetch_add(1);
}

/**
 * @brief Record a compilation event
 * 
 * Tracks compilation attempts and failures. Useful for monitoring
 * compilation overhead and detecting syntax errors in rules.
 * 
 * @param success Whether the compilation succeeded
 */
void PerformanceCounters::recordCompile(bool success) {
    counters_.totalCompileCount.fetch_add(1);
    if (!success) counters_.totalCompileFailures.fetch_add(1);
}

/**
 * @brief Record rule execution time
 * 
 * Adds the execution duration to the total. The average can be
 * calculated later by dividing by totalRulesExecuted.
 * 
 * @param duration The execution duration in nanoseconds
 */
void PerformanceCounters::recordExecutionTime(std::chrono::nanoseconds duration) {
    counters_.totalExecutionTimeNs.fetch_add(static_cast<uint64_t>(duration.count()));
}

// ============================================================================
// Metrics Access
// ============================================================================

/**
 * @brief Get a snapshot of all counters
 * 
 * Returns a copy of the Counters struct with current values.
 * Note: This is a point-in-time snapshot; values may change
 * immediately after the call.
 * 
 * @return Counters struct with current metric values
 */
PerformanceCounters::Counters PerformanceCounters::getCounters() const {
    return counters_;
}

/**
 * @brief Reset all counters to zero
 * 
 * Atomically sets all counter values to 0.
 * Useful for testing or when starting a new measurement period.
 */
void PerformanceCounters::reset() {
    counters_.totalRulesExecuted.store(0);
    counters_.totalRulesSuccessful.store(0);
    counters_.totalRulesFailed.store(0);
    counters_.totalRulesSkipped.store(0);
    counters_.totalRulesCached.store(0);
    counters_.totalRulesTimedOut.store(0);
    counters_.totalRulesRateLimited.store(0);
    counters_.totalCompileCount.store(0);
    counters_.totalCompileFailures.store(0);
    counters_.totalExecutionTimeNs.store(0);
}

// ============================================================================
// Derived Metrics
// ============================================================================

/**
 * @brief Calculate average execution time in milliseconds
 * 
 * Computes the average by dividing total execution time by
 * total rules executed. Returns 0.0 if no rules have been executed.
 * 
 * @return Average execution time in milliseconds
 */
double PerformanceCounters::getAverageExecutionTimeMs() const {
    uint64_t executed = counters_.totalRulesExecuted.load();
    if (executed == 0) return 0.0;
    uint64_t totalNs = counters_.totalExecutionTimeNs.load();
    return static_cast<double>(totalNs) / static_cast<double>(executed) / 1'000'000.0;
}

/**
 * @brief Calculate success rate
 * 
 * Returns the ratio of successful rules to total executed.
 * Returns 0.0 if no rules have been executed.
 * 
 * @return Success rate as a ratio (0.0 to 1.0)
 */
double PerformanceCounters::getSuccessRate() const {
    uint64_t executed = counters_.totalRulesExecuted.load();
    if (executed == 0) return 0.0;
    uint64_t successful = counters_.totalRulesSuccessful.load();
    return static_cast<double>(successful) / static_cast<double>(executed);
}

/**
 * @brief Calculate cache hit rate
 * 
 * Returns the ratio of cached results to total executed.
 * Useful for evaluating cache effectiveness.
 * 
 * @return Cache hit rate as a ratio (0.0 to 1.0)
 */
double PerformanceCounters::getCacheHitRate() const {
    uint64_t executed = counters_.totalRulesExecuted.load();
    if (executed == 0) return 0.0;
    uint64_t cached = counters_.totalRulesCached.load();
    return static_cast<double>(cached) / static_cast<double>(executed);
}

} // namespace fastrules
