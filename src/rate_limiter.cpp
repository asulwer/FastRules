#include "fastrules/rate_limiter.hpp"
#include <chrono>
#include <sstream>
#include <mutex>

namespace fastrules {

std::unique_ptr<RateLimiter> RateLimiter::globalInstance_;
std::once_flag RateLimiter::globalOnce_;

RateLimiter& RateLimiter::global() {
    std::call_once(globalOnce_, []() {
        globalInstance_ = std::make_unique<RateLimiter>();
    });
    return *globalInstance_;
}

void RateLimiter::checkAllowed(const std::string& ruleName) {
    if (!isAllowed(ruleName)) {
        std::ostringstream oss;
        oss << "Rate limit exceeded for rule '" << ruleId << "'";
        throw RateLimitException(oss.str());
    }
}

bool RateLimiter::isAllowed(const std::string& ruleName) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = states_.find(ruleName);
    if (it == states_.end()) {
        return true;  // No rate limit configured
    }

    RuleState& state = it->second;
    std::lock_guard<std::mutex> stateLock(state.mutex);

    cleanupOldEntries(state);

    const auto& config = state.config;
    if (!config.isEnabled()) {
        return true;
    }

    auto now = std::chrono::steady_clock::now();

    // Check per-second limit
    if (config.maxExecutionsPerSecond > 0) {
        int currentInSecond = static_cast<int>(state.secondWindow.size());
        int burst = config.burstSize > 0 ? config.burstSize : config.maxExecutionsPerSecond;
        if (currentInSecond >= burst) {
            return false;
        }
    }

    // Check per-minute limit
    if (config.maxExecutionsPerMinute > 0) {
        int currentInMinute = static_cast<int>(state.minuteWindow.size());
        if (currentInMinute >= config.maxExecutionsPerMinute) {
            return false;
        }
    }

    // Record execution
    state.secondWindow.push(now);
    state.minuteWindow.push(now);

    return true;
}

void RateLimiter::configure(const Config& config) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    states_[config.ruleName].config = config;
}

void RateLimiter::remove(const std::string& ruleName) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    states_.erase(ruleName);
}

int RateLimiter::getCurrentExecutionsPerSecond(const std::string& ruleName) const {
    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = states_.find(ruleName);
    if (it == states_.end()) return 0;

    RuleState& state = const_cast<RuleState&>(it->second);
    std::lock_guard<std::mutex> stateLock(state.mutex);
    cleanupOldEntries(state);
    return static_cast<int>(state.secondWindow.size());
}

int RateLimiter::getCurrentExecutionsPerMinute(const std::string& ruleName) const {
    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = states_.find(ruleName);
    if (it == states_.end()) return 0;

    RuleState& state = const_cast<RuleState&>(it->second);
    std::lock_guard<std::mutex> stateLock(state.mutex);
    cleanupOldEntries(state);
    return static_cast<int>(state.minuteWindow.size());
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(statesMutex_);
    states_.clear();
}

void RateLimiter::cleanupOldEntries(RuleState& state) const {
    auto now = std::chrono::steady_clock::now();
    auto oneSecondAgo = now - std::chrono::seconds(1);
    auto oneMinuteAgo = now - std::chrono::minutes(1);

    while (!state.secondWindow.empty() && state.secondWindow.front() < oneSecondAgo) {
        state.secondWindow.pop();
    }

    while (!state.minuteWindow.empty() && state.minuteWindow.front() < oneMinuteAgo) {
        state.minuteWindow.pop();
    }
}

} // namespace fastrules
