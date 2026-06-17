/**
 * @file rate_limiter.cpp
 * @brief Token bucket rate limiter implementation
 * 
 * The RateLimiter provides per-rule execution rate limiting using a
 * sliding window algorithm. It supports both per-second and per-minute
 * limits with configurable burst capacity.
 * 
 * Design Overview:
 * - Each rule has its own RuleState with independent windows
 * - Sliding windows track timestamps of recent executions
 * - Cleanup happens lazily when checking limits (not on a separate thread)
 * - Global singleton for application-wide rate limiting
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses two-level locking: global states mutex + per-rule mutex
 * - Avoids deadlocks by always acquiring locks in the same order
 * 
 * Algorithm:
 * 1. Clean up expired entries from windows (older than 1s/1m)
 * 2. Check if current window size exceeds configured limits
 * 3. If allowed, record execution timestamp in windows
 * 4. Return whether execution is permitted
 * 
 * Memory Management:
 * - Windows grow to max(burst_size, per_minute_limit) entries
 * - Memory reclaimed lazily during cleanup
 * - No background threads or timers
 */

#include "fastrules/rate_limiter.hpp"
#include <chrono>
#include <sstream>
#include <mutex>

namespace fastrules {

// ============================================================================
// Global Singleton
// ============================================================================

/** Global rate limiter instance (lazy-initialized) */
std::unique_ptr<RateLimiter> RateLimiter::globalInstance_;

/** Once flag for thread-safe singleton initialization */
std::once_flag RateLimiter::globalOnce_;

/**
 * @brief Get the global rate limiter instance
 * 
 * Uses Meyers' singleton pattern with std::call_once for thread-safe
 * initialization. The global instance is suitable for application-wide
 * rate limiting when you don't need separate limiters per workflow.
 * 
 * @return Reference to the global RateLimiter instance
 */
RateLimiter& RateLimiter::global() {
    std::call_once(globalOnce_, []() {
        globalInstance_ = std::make_unique<RateLimiter>();
    });
    return *globalInstance_;
}

// ============================================================================
// Rate Limiting
// ============================================================================

/**
 * @brief Check if rule execution is allowed, throwing on denial
 * 
 * Convenience method that calls isAllowed() and throws a RateLimitException
 * if the rate limit is exceeded. Use this when you want exceptions for flow
 * control (catch at boundary).
 * 
 * @param ruleName Name of the rule being executed
 * @throws RateLimitException if rate limit is exceeded
 */
void RateLimiter::checkAllowed(const std::string& ruleName) {
    if (!isAllowed(ruleName)) {
        std::ostringstream oss;
        oss << "Rate limit exceeded for rule '" << ruleName << "'";
        throw RateLimitException(oss.str());
    }
}

/**
 * @brief Check if rule execution is permitted under current rate limits
 * 
 * Core rate limiting logic:
 * 1. Acquire locks (states mutex, then rule-specific mutex)
 * 2. Clean up expired entries from windows
 * 3. Check if current window sizes exceed configured limits
 * 4. If allowed, record execution timestamp
 * 5. Return true if permitted, false if denied
 * 
 * The sliding window approach is more accurate than token bucket for
 * bursty traffic patterns, as it tracks actual request timestamps.
 * 
 * @param ruleName Name of the rule to check
 * @return true if execution is permitted, false if rate limited
 */
bool RateLimiter::isAllowed(const std::string& ruleName) {
    // First lock: global states map
    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = states_.find(ruleName);
    if (it == states_.end()) {
        return true;  // No rate limit configured for this rule
    }

    RuleState& state = it->second;
    // Second lock: per-rule state (fine-grained locking reduces contention)
    std::lock_guard<std::mutex> stateLock(state.mutex);

    // Remove timestamps older than window periods
    cleanupOldEntries(state);

    const auto& config = state.config;
    if (!config.isEnabled()) {
        return true;  // Rate limiting disabled for this rule
    }

    auto now = std::chrono::steady_clock::now();

    // Check per-second burst limit
    if (config.maxExecutionsPerSecond > 0) {
        int currentInSecond = static_cast<int>(state.secondWindow.size());
        // Burst allows temporary spikes above sustained rate
        int burst = config.burstSize > 0 ? config.burstSize : config.maxExecutionsPerSecond;
        if (currentInSecond >= burst) {
            return false;  // Burst limit exceeded
        }
    }

    // Check per-minute sustained rate limit
    if (config.maxExecutionsPerMinute > 0) {
        int currentInMinute = static_cast<int>(state.minuteWindow.size());
        if (currentInMinute >= config.maxExecutionsPerMinute) {
            return false;  // Per-minute limit exceeded
        }
    }

    // Record this execution attempt's timestamp in both windows
    state.secondWindow.push(now);
    state.minuteWindow.push(now);

    return true;
}

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configure rate limiting for a rule
 * 
 * Creates or updates the rate limit configuration for a rule.
 * Configuration changes take effect immediately for subsequent calls.
 * 
 * @param config Rate limit configuration (limits, windows, burst)
 */
void RateLimiter::configure(const Config& config) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    states_[config.ruleName].config = config;
}

/**
 * @brief Remove rate limiting configuration for a rule
 * 
 * Removes the rule from the states map. Subsequent isAllowed() calls
 * will return true (no limit) until reconfigured.
 * 
 * @param ruleName Name of the rule to unconfigure
 */
void RateLimiter::remove(const std::string& ruleName) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    states_.erase(ruleName);
}

/**
 * @brief Reset all rate limiting state
 * 
 * Clears all configured rules and their execution history.
 * Useful for testing or when reinitializing the application.
 */
void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(statesMutex_);
    states_.clear();
}

// ============================================================================
// Metrics
// ============================================================================

/**
 * @brief Get current executions per second for a rule
 * 
 * Returns the count of executions in the last second window.
 * This is useful for monitoring and observability.
 * 
 * Note: This method cleans up expired entries before counting, so
 * the result reflects the actual current rate.
 * 
 * @param ruleName Name of the rule to query
 * @return Number of executions in the last second
 */
int RateLimiter::getCurrentExecutionsPerSecond(const std::string& ruleName) const {
    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = states_.find(ruleName);
    if (it == states_.end()) return 0;

    // Need to cleanup to get accurate count
    RuleState& state = const_cast<RuleState&>(it->second);
    std::lock_guard<std::mutex> stateLock(state.mutex);
    cleanupOldEntries(state);
    return static_cast<int>(state.secondWindow.size());
}

/**
 * @brief Get current executions per minute for a rule
 * 
 * Returns the count of executions in the last minute window.
 * Useful for monitoring sustained rate vs burst rate.
 * 
 * @param ruleName Name of the rule to query
 * @return Number of executions in the last minute
 */
int RateLimiter::getCurrentExecutionsPerMinute(const std::string& ruleName) const {
    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = states_.find(ruleName);
    if (it == states_.end()) return 0;

    RuleState& state = const_cast<RuleState&>(it->second);
    std::lock_guard<std::mutex> stateLock(state.mutex);
    cleanupOldEntries(state);
    return static_cast<int>(state.minuteWindow.size());
}

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * @brief Clean up expired entries from rate limit windows
 * 
 * Removes timestamps older than:
 * - 1 second from the secondWindow
 * - 1 minute from the minuteWindow
 * 
 * This is called lazily during isAllowed() checks rather than on a
 * timer, which avoids background thread complexity and ensures
 * cleanup happens when needed.
 * 
 * @param state The RuleState to clean up
 */
void RateLimiter::cleanupOldEntries(RuleState& state) const {
    auto now = std::chrono::steady_clock::now();
    auto oneSecondAgo = now - std::chrono::seconds(1);
    auto oneMinuteAgo = now - std::chrono::minutes(1);

    // Pop entries older than 1 second from secondWindow
    while (!state.secondWindow.empty() && state.secondWindow.front() < oneSecondAgo) {
        state.secondWindow.pop();
    }

    // Pop entries older than 1 minute from minuteWindow
    while (!state.minuteWindow.empty() && state.minuteWindow.front() < oneMinuteAgo) {
        state.minuteWindow.pop();
    }
}

} // namespace fastrules
