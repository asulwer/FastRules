#include "fastrules/performance_counters.hpp"
#include <sstream>

namespace fastrules {

PerformanceCounters& PerformanceCounters::instance() {
    static PerformanceCounters instance;
    return instance;
}

void PerformanceCounters::recordExecution(bool success, bool skipped, bool cached, bool timedOut, bool rateLimited) {
    counters_.totalRulesExecuted.fetch_add(1);
    if (success) counters_.totalRulesSuccessful.fetch_add(1);
    else counters_.totalRulesFailed.fetch_add(1);
    if (skipped) counters_.totalRulesSkipped.fetch_add(1);
    if (cached) counters_.totalRulesCached.fetch_add(1);
    if (timedOut) counters_.totalRulesTimedOut.fetch_add(1);
    if (rateLimited) counters_.totalRulesRateLimited.fetch_add(1);
}

void PerformanceCounters::recordCompile(bool success) {
    counters_.totalCompileCount.fetch_add(1);
    if (!success) counters_.totalCompileFailures.fetch_add(1);
}

void PerformanceCounters::recordExecutionTime(std::chrono::nanoseconds duration) {
    counters_.totalExecutionTimeNs.fetch_add(static_cast<uint64_t>(duration.count()));
}

PerformanceCounters::Counters PerformanceCounters::getCounters() const {
    return counters_;
}

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

double PerformanceCounters::getAverageExecutionTimeMs() const {
    uint64_t executed = counters_.totalRulesExecuted.load();
    if (executed == 0) return 0.0;
    uint64_t totalNs = counters_.totalExecutionTimeNs.load();
    return static_cast<double>(totalNs) / static_cast<double>(executed) / 1'000'000.0;
}

double PerformanceCounters::getSuccessRate() const {
    uint64_t executed = counters_.totalRulesExecuted.load();
    if (executed == 0) return 0.0;
    uint64_t successful = counters_.totalRulesSuccessful.load();
    return static_cast<double>(successful) / static_cast<double>(executed);
}

double PerformanceCounters::getCacheHitRate() const {
    uint64_t executed = counters_.totalRulesExecuted.load();
    if (executed == 0) return 0.0;
    uint64_t cached = counters_.totalRulesCached.load();
    return static_cast<double>(cached) / static_cast<double>(executed);
}

} // namespace fastrules
