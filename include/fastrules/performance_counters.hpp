#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <mutex>

namespace fastrules {

// Global performance counters for the engine
class PerformanceCounters {
public:
    struct Counters {
        std::atomic<uint64_t> totalRulesExecuted{0};
        std::atomic<uint64_t> totalRulesSuccessful{0};
        std::atomic<uint64_t> totalRulesFailed{0};
        std::atomic<uint64_t> totalRulesSkipped{0};
        std::atomic<uint64_t> totalRulesCached{0};
        std::atomic<uint64_t> totalRulesTimedOut{0};
        std::atomic<uint64_t> totalRulesRateLimited{0};
        std::atomic<uint64_t> totalCompileCount{0};
        std::atomic<uint64_t> totalCompileFailures{0};
        std::atomic<uint64_t> totalExecutionTimeNs{0};

        Counters() = default;
        Counters(const Counters& other) {
            totalRulesExecuted.store(other.totalRulesExecuted.load());
            totalRulesSuccessful.store(other.totalRulesSuccessful.load());
            totalRulesFailed.store(other.totalRulesFailed.load());
            totalRulesSkipped.store(other.totalRulesSkipped.load());
            totalRulesCached.store(other.totalRulesCached.load());
            totalRulesTimedOut.store(other.totalRulesTimedOut.load());
            totalRulesRateLimited.store(other.totalRulesRateLimited.load());
            totalCompileCount.store(other.totalCompileCount.load());
            totalCompileFailures.store(other.totalCompileFailures.load());
            totalExecutionTimeNs.store(other.totalExecutionTimeNs.load());
        }
        Counters& operator=(const Counters& other) {
            totalRulesExecuted.store(other.totalRulesExecuted.load());
            totalRulesSuccessful.store(other.totalRulesSuccessful.load());
            totalRulesFailed.store(other.totalRulesFailed.load());
            totalRulesSkipped.store(other.totalRulesSkipped.load());
            totalRulesCached.store(other.totalRulesCached.load());
            totalRulesTimedOut.store(other.totalRulesTimedOut.load());
            totalRulesRateLimited.store(other.totalRulesRateLimited.load());
            totalCompileCount.store(other.totalCompileCount.load());
            totalCompileFailures.store(other.totalCompileFailures.load());
            totalExecutionTimeNs.store(other.totalExecutionTimeNs.load());
            return *this;
        }
    };

    static PerformanceCounters& instance();

    // Record events
    void recordExecution(bool success, bool skipped, bool cached, bool timedOut, bool rateLimited);
    void recordCompile(bool success);
    void recordExecutionTime(std::chrono::nanoseconds duration);

    // Access counters
    [[nodiscard]] Counters getCounters() const;

    // Reset all counters
    void reset();

    // Throughput metrics
    [[nodiscard]] double getAverageExecutionTimeMs() const;
    [[nodiscard]] double getSuccessRate() const;
    [[nodiscard]] double getCacheHitRate() const;

private:
    PerformanceCounters() = default;
    ~PerformanceCounters() = default;
    PerformanceCounters(const PerformanceCounters&) = delete;
    PerformanceCounters& operator=(const PerformanceCounters&) = delete;

    Counters counters_;
    mutable std::mutex jsonMutex_;
};

} // namespace fastrules
