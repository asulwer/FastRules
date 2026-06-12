#pragma once

#include "rule_result.hpp"

#include <chrono>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <memory>

namespace fastrules {

// Token bucket rate limiter per rule
class RateLimiter {
public:
    struct Config {
        std::string ruleName;
        int maxExecutionsPerSecond = 0;  // 0 = unlimited
        int maxExecutionsPerMinute = 0;    // 0 = unlimited
        int burstSize = 0;               // 0 = no burst

        bool isEnabled() const {
            return maxExecutionsPerSecond > 0 || maxExecutionsPerMinute > 0;
        }
    };

    RateLimiter() = default;

    // Check if execution is allowed. Throws RateLimitException if not.
    void checkAllowed(const std::string& ruleName);

    // Non-throwing version
    [[nodiscard]] bool isAllowed(const std::string& ruleName);

    // Global singleton for default rate limiting
    [[nodiscard]] static RateLimiter& global();

    // Configure rate limiting for a rule
    void configure(const Config& config);

    // Remove configuration for a rule
    void remove(const std::string& ruleId);

    // Get current metrics
    [[nodiscard]] int getCurrentExecutionsPerSecond(const std::string& ruleId) const;
    [[nodiscard]] int getCurrentExecutionsPerMinute(const std::string& ruleId) const;

    // Reset all counters
    void reset();

private:
    struct RuleState {
        Config config;
        std::queue<std::chrono::steady_clock::time_point> secondWindow;
        std::queue<std::chrono::steady_clock::time_point> minuteWindow;
        mutable std::mutex mutex;
    };

    mutable std::mutex statesMutex_;
    std::unordered_map<std::string, RuleState> states_;

    static std::unique_ptr<RateLimiter> globalInstance_;
    static std::once_flag globalOnce_;

    void cleanupOldEntries(RuleState& state) const;
};

} // namespace fastrules
